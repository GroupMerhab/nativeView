/*
 * JNI: load HTML/URL, eval JS, IPC listeners, hotkeys, version helpers.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nativeview_jni_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NV_JNI_WIN(p) ((nv_window_t *)(intptr_t)(p))

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvLoadUrl(JNIEnv *env,
                                                                        jclass cls,
                                                                        jlong window,
                                                                        jstring url) {
  const char *utf = NULL;
  jboolean is_copy = JNI_FALSE;
  (void)cls;
  if (!url) {
    nv_load_url(NV_JNI_WIN(window), NULL);
    return;
  }
  utf = (*env)->GetStringUTFChars(env, url, &is_copy);
  nv_load_url(NV_JNI_WIN(window), utf);
  (*env)->ReleaseStringUTFChars(env, url, utf);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvLoadHtml(
    JNIEnv *env, jclass cls, jlong window, jstring html, jstring base_url) {
  const char *h = NULL;
  const char *b = NULL;
  jboolean h_copy = JNI_FALSE;
  jboolean b_copy = JNI_FALSE;
  (void)cls;
  if (html) h = (*env)->GetStringUTFChars(env, html, &h_copy);
  if (base_url) b = (*env)->GetStringUTFChars(env, base_url, &b_copy);
  nv_load_html(NV_JNI_WIN(window), h, b);
  if (html && h) (*env)->ReleaseStringUTFChars(env, html, h);
  if (base_url && b) (*env)->ReleaseStringUTFChars(env, base_url, b);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvLoadHtmlRef(
    JNIEnv *env, jclass cls, jlong window, jbyteArray html, jlong length,
    jstring base_url) {
  jbyte *bytes = NULL;
  jboolean is_copy = JNI_FALSE;
  jsize alen = 0;
  const char *b = NULL;
  jboolean b_copy = JNI_FALSE;
  size_t n;
  (void)cls;
  if (!html) {
    nv_load_html_ref(NV_JNI_WIN(window), NULL, 0, NULL);
    return;
  }
  alen = (*env)->GetArrayLength(env, html);
  n = (length >= 0) ? (size_t)length : (size_t)alen;
  if (n > (size_t)alen) n = (size_t)alen;
  bytes = (*env)->GetByteArrayElements(env, html, &is_copy);
  if (base_url) b = (*env)->GetStringUTFChars(env, base_url, &b_copy);
  nv_load_html_ref(NV_JNI_WIN(window), (const char *)bytes, n, b);
  if (base_url && b) (*env)->ReleaseStringUTFChars(env, base_url, b);
  if (bytes) (*env)->ReleaseByteArrayElements(env, html, bytes, JNI_ABORT);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvEvalJs(JNIEnv *env,
                                                                       jclass cls,
                                                                       jlong window,
                                                                       jstring js) {
  const char *utf = NULL;
  jboolean is_copy = JNI_FALSE;
  (void)cls;
  if (!js) {
    nv_eval_js(NV_JNI_WIN(window), NULL);
    return;
  }
  utf = (*env)->GetStringUTFChars(env, js, &is_copy);
  nv_eval_js(NV_JNI_WIN(window), utf);
  (*env)->ReleaseStringUTFChars(env, js, utf);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvEvalJsBatch(
    JNIEnv *env, jclass cls, jlong window, jobjectArray scripts) {
  jsize i;
  jsize n;
  const char **ptrs = NULL;
  jstring *jstrs = NULL;
  unsigned char *utf_ok = NULL;
  (void)cls;
  if (!scripts) {
    nv_eval_js_batch(NV_JNI_WIN(window), NULL, 0);
    return;
  }
  n = (*env)->GetArrayLength(env, scripts);
  if (n <= 0) {
    nv_eval_js_batch(NV_JNI_WIN(window), NULL, 0);
    return;
  }
  ptrs = (const char **)calloc((size_t)n, sizeof(*ptrs));
  jstrs = (jstring *)calloc((size_t)n, sizeof(*jstrs));
  utf_ok = (unsigned char *)calloc((size_t)n, sizeof(*utf_ok));
  if (!ptrs || !jstrs || !utf_ok) {
    free(ptrs);
    free(jstrs);
    free(utf_ok);
    return;
  }
  for (i = 0; i < n; i++) {
    jobject o = (*env)->GetObjectArrayElement(env, scripts, i);
    const char *p;
    if (!o) {
      static const char k_empty[] = "";
      ptrs[i] = k_empty;
      continue;
    }
    jstrs[i] = (jstring)o;
    p = (*env)->GetStringUTFChars(env, jstrs[i], NULL);
    if (p) {
      ptrs[i] = p;
      utf_ok[i] = 1;
    } else {
      static const char k_empty[] = "";
      ptrs[i] = k_empty;
    }
  }
  nv_eval_js_batch(NV_JNI_WIN(window), ptrs, (size_t)n);
  for (i = 0; i < n; i++) {
    if (utf_ok[i] && jstrs[i]) (*env)->ReleaseStringUTFChars(env, jstrs[i], ptrs[i]);
    if (jstrs[i]) (*env)->DeleteLocalRef(env, jstrs[i]);
  }
  free(ptrs);
  free(jstrs);
  free(utf_ok);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvOnMessage(
    JNIEnv *env, jclass cls, jlong window, jobject listener) {
  (void)cls;
  nv_jni_install_message_listener(NV_JNI_WIN(window), env, listener);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvOnReady(JNIEnv *env,
                                                                        jclass cls,
                                                                        jlong window,
                                                                        jobject listener) {
  (void)cls;
  nv_jni_install_ready_listener(NV_JNI_WIN(window), env, listener);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowOnClose(
    JNIEnv *env, jclass cls, jlong window, jobject listener) {
  (void)cls;
  nv_jni_install_close_listener(NV_JNI_WIN(window), env, listener);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvSend(JNIEnv *env,
                                                                     jclass cls,
                                                                     jlong window,
                                                                     jstring event,
                                                                     jstring json) {
  const char *e = NULL;
  const char *j = NULL;
  jboolean e_copy = JNI_FALSE;
  jboolean j_copy = JNI_FALSE;
  (void)cls;
  if (event) e = (*env)->GetStringUTFChars(env, event, &e_copy);
  if (json) j = (*env)->GetStringUTFChars(env, json, &j_copy);
  nv_send(NV_JNI_WIN(window), e, j);
  if (event && e) (*env)->ReleaseStringUTFChars(env, event, e);
  if (json && j) (*env)->ReleaseStringUTFChars(env, json, j);
}

JNIEXPORT jint JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowRegisterHotkey(
    JNIEnv *env, jclass cls, jlong window, jstring id, jstring combo) {
  const char *i = NULL;
  const char *c = NULL;
  jboolean i_copy = JNI_FALSE;
  jboolean c_copy = JNI_FALSE;
  int rc;
  (void)cls;
  if (id) i = (*env)->GetStringUTFChars(env, id, &i_copy);
  if (combo) c = (*env)->GetStringUTFChars(env, combo, &c_copy);
  rc = nv_window_register_hotkey(NV_JNI_WIN(window), i, c);
  if (id && i) (*env)->ReleaseStringUTFChars(env, id, i);
  if (combo && c) (*env)->ReleaseStringUTFChars(env, combo, c);
  return (jint)rc;
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowUnregisterHotkey(
    JNIEnv *env, jclass cls, jlong window, jstring id) {
  const char *i = NULL;
  jboolean i_copy = JNI_FALSE;
  (void)cls;
  if (!id) {
    nv_window_unregister_hotkey(NV_JNI_WIN(window), NULL);
    return;
  }
  i = (*env)->GetStringUTFChars(env, id, &i_copy);
  nv_window_unregister_hotkey(NV_JNI_WIN(window), i);
  (*env)->ReleaseStringUTFChars(env, id, i);
}

JNIEXPORT jstring JNICALL Java_io_jamharah_nativeview_NativeView_nvVersionString(
    JNIEnv *env, jclass cls) {
  const char *s;
  (void)cls;
  s = nv_version_string();
  if (!s) return NULL;
  return (*env)->NewStringUTF(env, s);
}

JNIEXPORT jobject JNICALL Java_io_jamharah_nativeview_NativeView_nvGetVersionInfo(
    JNIEnv *env, jclass cls) {
  NvVersionInfo vi;
  jclass vcls;
  jmethodID ctor;
  jstring sha;
  jobject out;
  (void)cls;
  memset(&vi, 0, sizeof(vi));
  nv_get_version_info(&vi);
  vcls = (*env)->FindClass(env, "io/jamharah/nativeview/NvVersionInfo");
  if (!vcls) return NULL;
  ctor = (*env)->GetMethodID(env, vcls, "<init>", "(IIILjava/lang/String;)V");
  if (!ctor) return NULL;
  sha = vi.build_sha ? (*env)->NewStringUTF(env, vi.build_sha) : NULL;
  out = (*env)->NewObject(env, vcls, ctor, (jint)vi.major, (jint)vi.minor,
                          (jint)vi.patch, sha);
  if (sha) (*env)->DeleteLocalRef(env, sha);
  (*env)->DeleteLocalRef(env, vcls);
  return out;
}

JNIEXPORT jlong JNICALL Java_io_jamharah_nativeview_NativeView_nvBenchNow(JNIEnv *env,
                                                                        jclass cls) {
  (void)env;
  (void)cls;
  return (jlong)(int64_t)nv_bench_now();
}
