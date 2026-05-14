/*
 * Per-window listener bookkeeping and C→Java callback dispatch for bindings/java.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nativeview_jni_internal.h"

#include <stdlib.h>
#include <string.h>

static JavaVM *g_nv_jni_vm;
static NvJniWindowHooks *g_nv_jni_hooks;

JavaVM *nv_jni_get_vm(void) { return g_nv_jni_vm; }

void nv_jni_set_vm(JavaVM *vm) { g_nv_jni_vm = vm; }

void nv_jni_ctx_free(NvJniListenerCtx *c) {
  if (!c) return;
  if (g_nv_jni_vm && c->listener_glob) {
    JNIEnv *env = NULL;
    if ((*g_nv_jni_vm)->GetEnv(g_nv_jni_vm, (void **)&env, JNI_VERSION_1_6) ==
        JNI_OK) {
      (*env)->DeleteGlobalRef(env, c->listener_glob);
    }
  }
  free(c);
}

NvJniListenerCtx *nv_jni_ctx_alloc(JNIEnv *env, jobject listener,
                                   const char *method, const char *sig) {
  NvJniListenerCtx *c;
  jclass cls;
  if (!env || !listener) return NULL;
  c = (NvJniListenerCtx *)calloc(1, sizeof(*c));
  if (!c) return NULL;
  if ((*env)->GetJavaVM(env, &c->vm) != JNI_OK) {
    free(c);
    return NULL;
  }
  c->listener_glob = (*env)->NewGlobalRef(env, listener);
  if (!c->listener_glob) {
    free(c);
    return NULL;
  }
  cls = (*env)->GetObjectClass(env, c->listener_glob);
  if (!cls) {
    (*env)->DeleteGlobalRef(env, c->listener_glob);
    free(c);
    return NULL;
  }
  c->mid = (*env)->GetMethodID(env, cls, method, sig);
  (*env)->DeleteLocalRef(env, cls);
  if (!c->mid) {
    (*env)->DeleteGlobalRef(env, c->listener_glob);
    free(c);
    return NULL;
  }
  return c;
}

NvJniWindowHooks *nv_jni_hooks_get(nv_window_t *w) {
  NvJniWindowHooks *h;
  if (!w) return NULL;
  for (h = g_nv_jni_hooks; h; h = h->next) {
    if (h->win == w) return h;
  }
  h = (NvJniWindowHooks *)calloc(1, sizeof(*h));
  if (!h) return NULL;
  h->win = w;
  h->next = g_nv_jni_hooks;
  g_nv_jni_hooks = h;
  return h;
}

void nv_jni_hooks_remove(nv_window_t *w) {
  NvJniWindowHooks **pp = &g_nv_jni_hooks;
  if (!w) return;
  while (*pp) {
    if ((*pp)->win == w) {
      NvJniWindowHooks *h = *pp;
      *pp = h->next;
      nv_jni_ctx_free(h->msg);
      nv_jni_ctx_free(h->ready);
      nv_jni_ctx_free(h->close);
      free(h);
    } else {
      pp = &(*pp)->next;
    }
  }
}

static void nv_jni_msg_cb(nv_window_t *window, const char *event,
                          const char *json, void *userdata) {
  NvJniListenerCtx *c = (NvJniListenerCtx *)userdata;
  JNIEnv *env = NULL;
  jint get_env_rc;
  int attached_here = 0;
  jstring je = NULL;
  jstring jj = NULL;
  if (!c || !c->vm) return;
  get_env_rc = (*c->vm)->GetEnv(c->vm, (void **)&env, JNI_VERSION_1_6);
  if (get_env_rc == JNI_EDETACHED) {
    if ((*c->vm)->AttachCurrentThread(c->vm, (void **)&env, NULL) != JNI_OK) {
      return;
    }
    attached_here = 1;
  } else if (get_env_rc != JNI_OK) {
    return;
  }
  if (event) je = (*env)->NewStringUTF(env, event);
  if (json) jj = (*env)->NewStringUTF(env, json);
  (*env)->CallVoidMethod(env, c->listener_glob, c->mid, (jlong)(intptr_t)window,
                         je, jj);
  if (je) (*env)->DeleteLocalRef(env, je);
  if (jj) (*env)->DeleteLocalRef(env, jj);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  if (attached_here) (*c->vm)->DetachCurrentThread(c->vm);
}

static void nv_jni_ready_cb(nv_window_t *window, void *userdata) {
  NvJniListenerCtx *c = (NvJniListenerCtx *)userdata;
  JNIEnv *env = NULL;
  jint get_env_rc;
  int attached_here = 0;
  if (!c || !c->vm) return;
  get_env_rc = (*c->vm)->GetEnv(c->vm, (void **)&env, JNI_VERSION_1_6);
  if (get_env_rc == JNI_EDETACHED) {
    if ((*c->vm)->AttachCurrentThread(c->vm, (void **)&env, NULL) != JNI_OK) {
      return;
    }
    attached_here = 1;
  } else if (get_env_rc != JNI_OK) {
    return;
  }
  (*env)->CallVoidMethod(env, c->listener_glob, c->mid, (jlong)(intptr_t)window);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  if (attached_here) (*c->vm)->DetachCurrentThread(c->vm);
}

static void nv_jni_close_cb(nv_window_t *window, void *userdata) {
  nv_jni_ready_cb(window, userdata);
}

void nv_jni_install_message_listener(nv_window_t *w, JNIEnv *env,
                                   jobject listener_or_null) {
  NvJniWindowHooks *h = nv_jni_hooks_get(w);
  NvJniListenerCtx *old;
  NvJniListenerCtx *ctx;
  if (!h) return;
  old = h->msg;
  h->msg = NULL;
  nv_jni_ctx_free(old);
  if (!listener_or_null) {
    nv_on_message(w, NULL, NULL);
    return;
  }
  ctx = nv_jni_ctx_alloc(env, listener_or_null, "onMessage",
                         "(JLjava/lang/String;Ljava/lang/String;)V");
  if (!ctx) return;
  h->msg = ctx;
  nv_on_message(w, nv_jni_msg_cb, ctx);
}

void nv_jni_install_ready_listener(nv_window_t *w, JNIEnv *env,
                                   jobject listener_or_null) {
  NvJniWindowHooks *h = nv_jni_hooks_get(w);
  NvJniListenerCtx *old;
  NvJniListenerCtx *ctx;
  if (!h) return;
  old = h->ready;
  h->ready = NULL;
  nv_jni_ctx_free(old);
  if (!listener_or_null) {
    nv_on_ready(w, NULL, NULL);
    return;
  }
  ctx = nv_jni_ctx_alloc(env, listener_or_null, "onReady", "(J)V");
  if (!ctx) return;
  h->ready = ctx;
  nv_on_ready(w, nv_jni_ready_cb, ctx);
}

void nv_jni_install_close_listener(nv_window_t *w, JNIEnv *env,
                                   jobject listener_or_null) {
  NvJniWindowHooks *h = nv_jni_hooks_get(w);
  NvJniListenerCtx *old;
  NvJniListenerCtx *ctx;
  if (!h) return;
  old = h->close;
  h->close = NULL;
  nv_jni_ctx_free(old);
  if (!listener_or_null) {
    nv_window_on_close(w, NULL, NULL);
    return;
  }
  ctx = nv_jni_ctx_alloc(env, listener_or_null, "onClose", "(J)V");
  if (!ctx) return;
  h->close = ctx;
  nv_window_on_close(w, nv_jni_close_cb, ctx);
}
