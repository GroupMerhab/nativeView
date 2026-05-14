/*
 * Internal helpers for the desktop Java JNI binding (bindings/java).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NATIVEVIEW_JNI_INTERNAL_H
#define NATIVEVIEW_JNI_INTERNAL_H

#include <jni.h>

#include "nv.h"

typedef struct NvJniListenerCtx {
  JavaVM *vm;
  jobject listener_glob;
  jmethodID mid;
} NvJniListenerCtx;

typedef struct NvJniWindowHooks {
  nv_window_t *win;
  NvJniListenerCtx *msg;
  NvJniListenerCtx *ready;
  NvJniListenerCtx *close;
  struct NvJniWindowHooks *next;
} NvJniWindowHooks;

JavaVM *nv_jni_get_vm(void);
void nv_jni_set_vm(JavaVM *vm);

NvJniWindowHooks *nv_jni_hooks_get(nv_window_t *w);
void nv_jni_hooks_remove(nv_window_t *w);

void nv_jni_ctx_free(NvJniListenerCtx *c);
NvJniListenerCtx *nv_jni_ctx_alloc(JNIEnv *env, jobject listener,
                                  const char *method, const char *sig);

void nv_jni_install_message_listener(nv_window_t *w, JNIEnv *env,
                                     jobject listener_or_null);
void nv_jni_install_ready_listener(nv_window_t *w, JNIEnv *env,
                                   jobject listener_or_null);
void nv_jni_install_close_listener(nv_window_t *w, JNIEnv *env,
                                   jobject listener_or_null);

#endif /* NATIVEVIEW_JNI_INTERNAL_H */
