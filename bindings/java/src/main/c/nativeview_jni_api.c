/*
 * JNI entry points for io.jamharah.nativeview.NativeView (desktop host).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nativeview_jni_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NV_JNI_APP(p) ((nv_app_t *)(intptr_t)(p))
#define NV_JNI_WIN(p) ((nv_window_t *)(intptr_t)(p))

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  (void)reserved;
  nv_jni_set_vm(vm);
  return JNI_VERSION_1_6;
}

JNIEXPORT jboolean JNICALL Java_io_jamharah_nativeview_NativeView_nvIsProcessMainThread(JNIEnv *env,
                                                                                        jclass cls) {
  (void)env;
  (void)cls;
  return nv_is_process_main_thread() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL Java_io_jamharah_nativeview_NativeView_nvAppCreate(JNIEnv *env,
                                                                           jclass cls) {
  (void)env;
  (void)cls;
  return (jlong)(intptr_t)nv_app_create();
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvAppRun(JNIEnv *env,
                                                                       jclass cls,
                                                                       jlong app) {
  (void)env;
  (void)cls;
  nv_app_run(NV_JNI_APP(app));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvAppQuit(JNIEnv *env,
                                                                        jclass cls,
                                                                        jlong app) {
  (void)env;
  (void)cls;
  nv_app_quit(NV_JNI_APP(app));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvAppDestroy(JNIEnv *env,
                                                                           jclass cls,
                                                                           jlong app) {
  (void)env;
  (void)cls;
  nv_app_destroy(NV_JNI_APP(app));
}

JNIEXPORT jlong JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowCreate0(
    JNIEnv *env, jclass cls, jlong app, jstring title, jint width, jint height,
    jint min_width, jint min_height, jint resizable, jint frameless,
    jint transparent, jint devtools, jint modal) {
  nv_window_cfg_t cfg;
  const char *title_utf = NULL;
  jboolean title_copy = JNI_FALSE;
  (void)cls;
  memset(&cfg, 0, sizeof(cfg));
  if (title) {
    title_utf = (*env)->GetStringUTFChars(env, title, &title_copy);
    cfg.title = title_utf;
  }
  cfg.width = (int)width;
  cfg.height = (int)height;
  cfg.min_width = (int)min_width;
  cfg.min_height = (int)min_height;
  cfg.resizable = (int)resizable;
  cfg.frameless = (int)frameless;
  cfg.transparent = (int)transparent;
  cfg.devtools = (int)devtools;
  cfg.modal = (int)modal;
  nv_window_t *w = nv_window_create(NV_JNI_APP(app), &cfg);
  if (title && title_utf) (*env)->ReleaseStringUTFChars(env, title, title_utf);
  return (jlong)(intptr_t)w;
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowClose(JNIEnv *env,
                                                                          jclass cls,
                                                                          jlong window) {
  (void)env;
  (void)cls;
  nv_window_close(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowDestroy(JNIEnv *env,
                                                                              jclass cls,
                                                                              jlong window) {
  (void)env;
  (void)cls;
  nv_jni_hooks_remove(NV_JNI_WIN(window));
  nv_window_destroy(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowShow(JNIEnv *env,
                                                                           jclass cls,
                                                                           jlong window) {
  (void)env;
  (void)cls;
  nv_window_show(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowHide(JNIEnv *env,
                                                                           jclass cls,
                                                                           jlong window) {
  (void)env;
  (void)cls;
  nv_window_hide(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowSetTitle(
    JNIEnv *env, jclass cls, jlong window, jstring title) {
  const char *utf = NULL;
  jboolean is_copy = JNI_FALSE;
  (void)cls;
  if (!title) {
    nv_window_set_title(NV_JNI_WIN(window), NULL);
    return;
  }
  utf = (*env)->GetStringUTFChars(env, title, &is_copy);
  nv_window_set_title(NV_JNI_WIN(window), utf);
  (*env)->ReleaseStringUTFChars(env, title, utf);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowSetSize(
    JNIEnv *env, jclass cls, jlong window, jint width, jint height) {
  (void)env;
  (void)cls;
  nv_window_set_size(NV_JNI_WIN(window), (int)width, (int)height);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowSetMinSize(
    JNIEnv *env, jclass cls, jlong window, jint width, jint height) {
  (void)env;
  (void)cls;
  nv_window_set_min_size(NV_JNI_WIN(window), (int)width, (int)height);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowCenter(JNIEnv *env,
                                                                             jclass cls,
                                                                             jlong window) {
  (void)env;
  (void)cls;
  nv_window_center(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowFullscreen(
    JNIEnv *env, jclass cls, jlong window, jint enable) {
  (void)env;
  (void)cls;
  nv_window_fullscreen(NV_JNI_WIN(window), (int)enable);
}

JNIEXPORT jint JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowIsFullscreen(
    JNIEnv *env, jclass cls, jlong window) {
  (void)env;
  (void)cls;
  return (jint)nv_window_is_fullscreen(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowMinimize(JNIEnv *env,
                                                                               jclass cls,
                                                                               jlong window) {
  (void)env;
  (void)cls;
  nv_window_minimize(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowMaximize(JNIEnv *env,
                                                                               jclass cls,
                                                                               jlong window) {
  (void)env;
  (void)cls;
  nv_window_maximize(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowRestore(JNIEnv *env,
                                                                              jclass cls,
                                                                              jlong window) {
  (void)env;
  (void)cls;
  nv_window_restore(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowFocus(JNIEnv *env,
                                                                            jclass cls,
                                                                            jlong window) {
  (void)env;
  (void)cls;
  nv_window_focus(NV_JNI_WIN(window));
}

JNIEXPORT jint JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowIsFocused(
    JNIEnv *env, jclass cls, jlong window) {
  (void)env;
  (void)cls;
  return (jint)nv_window_is_focused(NV_JNI_WIN(window));
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowSetResizable(
    JNIEnv *env, jclass cls, jlong window, jint enable) {
  (void)env;
  (void)cls;
  nv_window_set_resizable(NV_JNI_WIN(window), (int)enable);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowSetAlwaysOnTop(
    JNIEnv *env, jclass cls, jlong window, jint enable) {
  (void)env;
  (void)cls;
  nv_window_set_always_on_top(NV_JNI_WIN(window), (int)enable);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowSetOpacity(
    JNIEnv *env, jclass cls, jlong window, jint opacity_pct) {
  (void)env;
  (void)cls;
  nv_window_set_opacity(NV_JNI_WIN(window), (int)opacity_pct);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowSetZoomFactor(
    JNIEnv *env, jclass cls, jlong window, jdouble factor) {
  (void)env;
  (void)cls;
  nv_window_set_zoom_factor(NV_JNI_WIN(window), (double)factor);
}

JNIEXPORT void JNICALL Java_io_jamharah_nativeview_NativeView_nvWindowRequestClose(
    JNIEnv *env, jclass cls, jlong window) {
  (void)env;
  (void)cls;
  nv_window_request_close(NV_JNI_WIN(window));
}
