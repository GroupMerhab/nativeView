/*
 * JNI entry points for com.nativeview.jni.NativeViewJNI (Android library module).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nv_android_host.h"
#include "nv_android_jni_hooks.h"
#include "nv_ipc_internal.h"
#include "nv_window_internal.h"
#include "nv_window_manager.h"

#include "nv.h"

#include <android/log.h>
#include <jni.h>
#include <stdint.h>
#include <string.h>

#define NV_JNI_WIN(p) ((nv_window_t *)(intptr_t)(p))

#define NV_ANDROID_LOG_TAG "nativeview"

static nv_app_t *g_nv_jni_singleton_app = NULL;

static void nv_jni_destroy_all_windows_for_app(nv_app_t *app) {
  nv_window_t *pending[NV_MAX_WINDOWS];
  int n_pending = 0;
  nv_wm_entry_t entries[NV_MAX_WINDOWS];
  size_t n = NV_MAX_WINDOWS;

  if (!app) return;
  if (nv_wm_list(entries, &n) != 0) return;

  for (size_t i = 0; i < n && n_pending < NV_MAX_WINDOWS; i++) {
    nv_window_t *w = entries[i].window;
    if (w && w->app == app) pending[n_pending++] = w;
  }
  for (int i = 0; i < n_pending; i++) {
    nv_android_jni_hooks_remove(pending[i]);
    nv_window_destroy(pending[i]);
  }
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeInit(JNIEnv *env, jclass clazz,
                                                                        jobject context) {
  (void)clazz;
  if (!env || !context) return;

  if (!g_nv_jni_singleton_app) {
    g_nv_jni_singleton_app = nv_app_create();
    if (!g_nv_jni_singleton_app) return;
  }

  jclass iface = (*env)->FindClass(env, "com/nativeview/jni/NativeViewHost");
  if (!iface) {
    (*env)->ExceptionClear(env);
    return;
  }
  if ((*env)->IsInstanceOf(env, context, iface)) {
    nv_android_bind_dispatch_host(env, context);
  }
  (*env)->DeleteLocalRef(env, iface);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeDestroy(JNIEnv *env,
                                                                           jclass clazz) {
  (void)clazz;
  if (!env) return;

  nv_android_bind_dispatch_host(env, NULL);
  if (g_nv_jni_singleton_app) {
    nv_jni_destroy_all_windows_for_app(g_nv_jni_singleton_app);
    nv_app_destroy(g_nv_jni_singleton_app);
    g_nv_jni_singleton_app = NULL;
  }
}

JNIEXPORT jlong JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeCreateWindow(JNIEnv *env,
                                                                                 jclass clazz,
                                                                                 jobject config) {
  jfieldID fid_title, fid_width, fid_height, fid_resizable, fid_devtools;
  jclass cfg_cls;
  jstring jtitle = NULL;
  const char *title_utf = NULL;
  jboolean title_is_copy = JNI_FALSE;
  nv_window_cfg_t cfg;

  (void)clazz;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "nativeview";
  cfg.width = 1024;
  cfg.height = 768;
  cfg.resizable = 0;
  cfg.frameless = 1;
  cfg.transparent = 0;
  cfg.devtools = 0;
  cfg.modal = 0;

  if (!env || !config || !g_nv_jni_singleton_app) return 0;

  cfg_cls = (*env)->GetObjectClass(env, config);
  if (!cfg_cls) return 0;

  fid_title = (*env)->GetFieldID(env, cfg_cls, "title", "Ljava/lang/String;");
  fid_width = (*env)->GetFieldID(env, cfg_cls, "width", "I");
  fid_height = (*env)->GetFieldID(env, cfg_cls, "height", "I");
  fid_resizable = (*env)->GetFieldID(env, cfg_cls, "resizable", "Z");
  fid_devtools = (*env)->GetFieldID(env, cfg_cls, "devtools", "Z");
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, cfg_cls);
    __android_log_print(ANDROID_LOG_ERROR, NV_ANDROID_LOG_TAG,
                        "nativeCreateWindow: NativeViewConfig field lookup failed");
    return 0;
  }

  jtitle = (jstring)(*env)->GetObjectField(env, config, fid_title);
  cfg.width = (*env)->GetIntField(env, config, fid_width);
  cfg.height = (*env)->GetIntField(env, config, fid_height);
  cfg.resizable = (*env)->GetBooleanField(env, config, fid_resizable) ? 1 : 0;
  cfg.devtools = (*env)->GetBooleanField(env, config, fid_devtools) ? 1 : 0;

  if (jtitle) {
    title_utf = (*env)->GetStringUTFChars(env, jtitle, &title_is_copy);
    if (title_utf && title_utf[0]) cfg.title = title_utf;
  }
  if (cfg.width < 100) cfg.width = 100;
  if (cfg.height < 100) cfg.height = 100;

  nv_window_t *w = nv_window_create(g_nv_jni_singleton_app, &cfg);

  if (title_utf) (*env)->ReleaseStringUTFChars(env, jtitle, title_utf);
  if (jtitle) (*env)->DeleteLocalRef(env, jtitle);
  (*env)->DeleteLocalRef(env, cfg_cls);

  return (jlong)(intptr_t)w;
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeDestroyWindow(JNIEnv *env,
                                                                                 jclass clazz,
                                                                                 jlong window_id) {
  (void)env;
  (void)clazz;
  nv_window_t *w = NV_JNI_WIN(window_id);
  if (!w) return;
  nv_android_jni_hooks_remove(w);
  nv_window_destroy(w);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeLoadUrl(JNIEnv *env, jclass clazz,
                                                                           jlong window_id,
                                                                           jstring url) {
  const char *utf = NULL;
  jboolean is_copy = JNI_FALSE;
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)clazz;
  if (!env || !w) return;
  if (!url) return;
  utf = (*env)->GetStringUTFChars(env, url, &is_copy);
  if (!utf) return;
  nv_load_url(w, utf);
  (*env)->ReleaseStringUTFChars(env, url, utf);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeLoadHtml(JNIEnv *env, jclass clazz,
                                                                            jlong window_id,
                                                                            jstring html,
                                                                            jstring base_url) {
  const char *html_utf = NULL;
  const char *base_utf = NULL;
  jboolean c1 = JNI_FALSE, c2 = JNI_FALSE;
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)clazz;
  if (!env || !w) return;
  if (html) html_utf = (*env)->GetStringUTFChars(env, html, &c1);
  if (base_url) base_utf = (*env)->GetStringUTFChars(env, base_url, &c2);
  nv_load_html(w, html_utf ? html_utf : "", base_utf);
  if (html_utf) (*env)->ReleaseStringUTFChars(env, html, html_utf);
  if (base_utf) (*env)->ReleaseStringUTFChars(env, base_url, base_utf);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeEvalJs(JNIEnv *env, jclass clazz,
                                                                          jlong window_id,
                                                                          jstring js) {
  const char *utf = NULL;
  jboolean is_copy = JNI_FALSE;
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)clazz;
  if (!env || !w) return;
  if (!js) {
    nv_eval_js(w, "");
    return;
  }
  utf = (*env)->GetStringUTFChars(env, js, &is_copy);
  nv_eval_js(w, utf ? utf : "");
  (*env)->ReleaseStringUTFChars(env, js, utf);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeSetTitle(JNIEnv *env, jclass clazz,
                                                                            jlong window_id,
                                                                            jstring title) {
  const char *utf = NULL;
  jboolean is_copy = JNI_FALSE;
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)clazz;
  if (!env || !w) return;
  if (!title) return;
  utf = (*env)->GetStringUTFChars(env, title, &is_copy);
  if (!utf) return;
  nv_window_set_title(w, utf);
  (*env)->ReleaseStringUTFChars(env, title, utf);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeSetSize(JNIEnv *env, jclass clazz,
                                                                           jlong window_id, jint width,
                                                                           jint height) {
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)env;
  (void)clazz;
  if (!w) return;
  nv_window_set_size(w, (int)width, (int)height);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeShow(JNIEnv *env, jclass clazz,
                                                                        jlong window_id) {
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)env;
  (void)clazz;
  if (!w) return;
  nv_window_show(w);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeHide(JNIEnv *env, jclass clazz,
                                                                        jlong window_id) {
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)env;
  (void)clazz;
  if (!w) return;
  nv_window_hide(w);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeEmit(JNIEnv *env, jclass clazz,
                                                                        jlong window_id, jstring event,
                                                                        jstring json) {
  const char *ev = NULL;
  const char *js = NULL;
  jboolean ecopy = JNI_FALSE, jcopy = JNI_FALSE;
  nv_window_t *w = NV_JNI_WIN(window_id);
  (void)clazz;
  if (!env || !w) return;
  if (!event) return;
  ev = (*env)->GetStringUTFChars(env, event, &ecopy);
  if (!ev) return;
  if (json) js = (*env)->GetStringUTFChars(env, json, &jcopy);
  nv_send(w, ev, js ? js : "null");
  if (js) (*env)->ReleaseStringUTFChars(env, json, js);
  (*env)->ReleaseStringUTFChars(env, event, ev);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeDispatchWebMessage(JNIEnv *env,
                                                                                       jclass clazz,
                                                                                       jlong window_id,
                                                                                       jstring wireJson) {
  const char *utf = NULL;
  jboolean is_copy = JNI_FALSE;
  nv_window_t *w = NV_JNI_WIN(window_id);
  nv_ipc_state_t *ipc;
  (void)clazz;
  if (!env || !w || !wireJson) return;
  ipc = nv_window_get_ipc(w);
  if (!ipc) return;
  utf = (*env)->GetStringUTFChars(env, wireJson, &is_copy);
  if (!utf) return;
  nv_ipc_dispatch(w, ipc, utf);
  (*env)->ReleaseStringUTFChars(env, wireJson, utf);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeOnMessage(JNIEnv *env, jclass clazz,
                                                                             jlong window_id, jobject cb) {
  (void)clazz;
  nv_android_jni_install_message_listener(NV_JNI_WIN(window_id), env, cb);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeOnReady(JNIEnv *env, jclass clazz,
                                                                           jlong window_id, jobject cb) {
  (void)clazz;
  nv_android_jni_install_ready_listener(NV_JNI_WIN(window_id), env, cb);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeOnClose(JNIEnv *env, jclass clazz,
                                                                           jlong window_id, jobject cb) {
  (void)clazz;
  nv_android_jni_install_close_listener(NV_JNI_WIN(window_id), env, cb);
}

JNIEXPORT void JNICALL Java_com_nativeview_jni_NativeViewJNI_nativeQuit(JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  if (!g_nv_jni_singleton_app) return;
  nv_app_quit(g_nv_jni_singleton_app);
}
