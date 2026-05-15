/*
 * Android JNI ↔ WebView host bridge (dispatch to Java).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NV_ANDROID_HOST_H
#define NV_ANDROID_HOST_H

#include <jni.h>

/**
 * Binds the global dispatch host (WebView owner). Pass NULL to clear.
 * The host object must implement dispatch(int, long, String, String).
 */
void nv_android_bind_dispatch_host(JNIEnv *env, jobject host_or_null);

#endif /* NV_ANDROID_HOST_H */
