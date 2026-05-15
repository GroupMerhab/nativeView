/*
 * Per-window JNI listener hooks for Android NativeViewJNI (C callbacks -> Java).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NV_ANDROID_JNI_HOOKS_H
#define NV_ANDROID_JNI_HOOKS_H

#include "nv.h"

#include <jni.h>

void nv_android_jni_hooks_remove(nv_window_t *win);

void nv_android_jni_install_message_listener(nv_window_t *w, JNIEnv *env, jobject listener_or_null);

void nv_android_jni_install_ready_listener(nv_window_t *w, JNIEnv *env, jobject listener_or_null);

void nv_android_jni_install_close_listener(nv_window_t *w, JNIEnv *env, jobject listener_or_null);

#endif /* NV_ANDROID_JNI_HOOKS_H */
