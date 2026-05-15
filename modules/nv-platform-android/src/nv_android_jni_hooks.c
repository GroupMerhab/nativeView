/*
 * Per-window JNI listener bookkeeping and C->Java callback dispatch (Android).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nv_android_jni_hooks.h"

#include "nv_window_internal.h"

#include <jni.h>
#include <stdlib.h>
#include <string.h>

typedef struct NvAndroidJniListenerCtx {
  JavaVM *vm;
  jobject listener_glob;
  jmethodID mid;
} NvAndroidJniListenerCtx;

typedef struct NvAndroidJniWindowHooks {
  nv_window_t *win;
  NvAndroidJniListenerCtx *msg;
  NvAndroidJniListenerCtx *ready;
  NvAndroidJniListenerCtx *close;
  struct NvAndroidJniWindowHooks *next;
} NvAndroidJniWindowHooks;

static NvAndroidJniWindowHooks *g_nv_android_jni_hooks;

static void nv_android_jni_ctx_free(NvAndroidJniListenerCtx *c) {
  JavaVM *vm;
  JNIEnv *env = NULL;
  if (!c) return;
  vm = c->vm;
  if (vm && c->listener_glob) {
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) == JNI_OK && env) {
      (*env)->DeleteGlobalRef(env, c->listener_glob);
    }
  }
  free(c);
}

static NvAndroidJniListenerCtx *nv_android_jni_ctx_alloc(JNIEnv *env, jobject listener,
                                                         const char *method, const char *sig) {
  NvAndroidJniListenerCtx *c;
  jclass cls;
  if (!env || !listener) return NULL;
  c = (NvAndroidJniListenerCtx *)calloc(1, sizeof(*c));
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
    (*env)->ExceptionClear(env);
    (*env)->DeleteGlobalRef(env, c->listener_glob);
    free(c);
    return NULL;
  }
  return c;
}

static NvAndroidJniWindowHooks *nv_android_jni_hooks_get(nv_window_t *w) {
  NvAndroidJniWindowHooks *h;
  if (!w) return NULL;
  if (!w) return NULL;
  for (h = g_nv_android_jni_hooks; h; h = h->next) {
    if (h->win == w) return h;
  }
  h = (NvAndroidJniWindowHooks *)calloc(1, sizeof(*h));
  if (!h) return NULL;
  h->win = w;
  h->next = g_nv_android_jni_hooks;
  g_nv_android_jni_hooks = h;
  return h;
}

void nv_android_jni_hooks_remove(nv_window_t *w) {
  NvAndroidJniWindowHooks **pp = &g_nv_android_jni_hooks;
  if (!w) return;
  while (*pp) {
    if ((*pp)->win == w) {
      NvAndroidJniWindowHooks *h = *pp;
      *pp = h->next;
      nv_android_jni_ctx_free(h->msg);
      nv_android_jni_ctx_free(h->ready);
      nv_android_jni_ctx_free(h->close);
      free(h);
    } else {
      pp = &(*pp)->next;
    }
  }
}

static void nv_android_jni_msg_cb(nv_window_t *window, const char *event, const char *json,
                                  void *userdata) {
  NvAndroidJniListenerCtx *c = (NvAndroidJniListenerCtx *)userdata;
  JNIEnv *env = NULL;
  jint get_env_rc;
  int attached_here = 0;
  jstring je = NULL;
  jstring jj = NULL;
  (void)window;
  if (!c || !c->vm) return;
  get_env_rc = (*c->vm)->GetEnv(c->vm, (void **)&env, JNI_VERSION_1_6);
  if (get_env_rc == JNI_EDETACHED) {
    if ((*c->vm)->AttachCurrentThread(c->vm, (void **)&env, NULL) != JNI_OK) return;
    attached_here = 1;
  } else if (get_env_rc != JNI_OK) {
    return;
  }
  if (event) je = (*env)->NewStringUTF(env, event);
  if (json) jj = (*env)->NewStringUTF(env, json);
  (*env)->CallVoidMethod(env, c->listener_glob, c->mid, je, jj);
  if (je) (*env)->DeleteLocalRef(env, je);
  if (jj) (*env)->DeleteLocalRef(env, jj);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  if (attached_here) (*c->vm)->DetachCurrentThread(c->vm);
}

static void nv_android_jni_ready_cb(nv_window_t *window, void *userdata) {
  NvAndroidJniListenerCtx *c = (NvAndroidJniListenerCtx *)userdata;
  JNIEnv *env = NULL;
  jint get_env_rc;
  int attached_here = 0;
  (void)window;
  if (!c || !c->vm) return;
  get_env_rc = (*c->vm)->GetEnv(c->vm, (void **)&env, JNI_VERSION_1_6);
  if (get_env_rc == JNI_EDETACHED) {
    if ((*c->vm)->AttachCurrentThread(c->vm, (void **)&env, NULL) != JNI_OK) return;
    attached_here = 1;
  } else if (get_env_rc != JNI_OK) {
    return;
  }
  (*env)->CallVoidMethod(env, c->listener_glob, c->mid);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  if (attached_here) (*c->vm)->DetachCurrentThread(c->vm);
}

static void nv_android_jni_close_cb(nv_window_t *window, void *userdata) {
  nv_android_jni_ready_cb(window, userdata);
}

void nv_android_jni_install_message_listener(nv_window_t *w, JNIEnv *env, jobject listener_or_null) {
  NvAndroidJniWindowHooks *h;
  NvAndroidJniListenerCtx *old;
  NvAndroidJniListenerCtx *ctx;
  if (!w || !env) return;
  h = nv_android_jni_hooks_get(w);
  if (!h) return;
  old = h->msg;
  h->msg = NULL;
  nv_android_jni_ctx_free(old);
  if (!listener_or_null) {
    nv_on_message(w, NULL, NULL);
    return;
  }
  ctx = nv_android_jni_ctx_alloc(env, listener_or_null, "onMessage",
                                 "(Ljava/lang/String;Ljava/lang/String;)V");
  if (!ctx) return;
  h->msg = ctx;
  nv_on_message(w, nv_android_jni_msg_cb, ctx);
}

void nv_android_jni_install_ready_listener(nv_window_t *w, JNIEnv *env, jobject listener_or_null) {
  NvAndroidJniWindowHooks *h;
  NvAndroidJniListenerCtx *old;
  NvAndroidJniListenerCtx *ctx;
  if (!w || !env) return;
  h = nv_android_jni_hooks_get(w);
  if (!h) return;
  old = h->ready;
  h->ready = NULL;
  nv_android_jni_ctx_free(old);
  if (!listener_or_null) {
    nv_on_ready(w, NULL, NULL);
    return;
  }
  ctx = nv_android_jni_ctx_alloc(env, listener_or_null, "onReady", "()V");
  if (!ctx) return;
  h->ready = ctx;
  nv_on_ready(w, nv_android_jni_ready_cb, ctx);
}

void nv_android_jni_install_close_listener(nv_window_t *w, JNIEnv *env, jobject listener_or_null) {
  NvAndroidJniWindowHooks *h;
  NvAndroidJniListenerCtx *old;
  NvAndroidJniListenerCtx *ctx;
  if (!w || !env) return;
  h = nv_android_jni_hooks_get(w);
  if (!h) return;
  old = h->close;
  h->close = NULL;
  nv_android_jni_ctx_free(old);
  if (!listener_or_null) {
    nv_window_on_close(w, NULL, NULL);
    return;
  }
  ctx = nv_android_jni_ctx_alloc(env, listener_or_null, "onClose", "()V");
  if (!ctx) return;
  h->close = ctx;
  nv_window_on_close(w, nv_android_jni_close_cb, ctx);
}
