#include "nv_core_internal.h"
#include "nv_ipc_internal.h"
#include "nv_window_internal.h"

#include "nv.h"

#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <string.h>

#define NV_ANDROID_LOG_TAG "nativeview"

typedef struct {
  nv_window_t* window;
} nv_android_window_platform_t;

static JavaVM* g_nv_android_vm = NULL;
static jobject g_nv_android_host = NULL;
static jclass g_nv_android_host_cls = NULL;
static jmethodID g_nv_android_mid_dispatch = NULL;

enum {
  NV_ANDROID_OP_WINDOW_CREATE = 1,
  NV_ANDROID_OP_WINDOW_DESTROY = 2,
  NV_ANDROID_OP_WINDOW_SHOW = 3,
  NV_ANDROID_OP_WINDOW_HIDE = 4,
  NV_ANDROID_OP_WINDOW_LOAD_URL = 5,
  NV_ANDROID_OP_WINDOW_LOAD_HTML = 6,
  NV_ANDROID_OP_WINDOW_EVAL_JS = 7,
};

static JNIEnv* nv_android_get_env(int* out_detach) {
  if (out_detach) *out_detach = 0;
  if (!g_nv_android_vm) return NULL;
  JNIEnv* env = NULL;
  jint rc = (*g_nv_android_vm)->GetEnv(g_nv_android_vm, (void**)&env, JNI_VERSION_1_6);
  if (rc == JNI_OK) return env;
  if (rc == JNI_EDETACHED) {
    if ((*g_nv_android_vm)->AttachCurrentThread(g_nv_android_vm, &env, NULL) != JNI_OK) {
      return NULL;
    }
    if (out_detach) *out_detach = 1;
    return env;
  }
  return NULL;
}

static void nv_android_release_env(int detach) {
  if (!g_nv_android_vm) return;
  if (detach) {
    (*g_nv_android_vm)->DetachCurrentThread(g_nv_android_vm);
  }
}

static void nv_android_host_dispatch(int op, nv_window_t* window, const char* s1, const char* s2) {
  if (!window) return;
  if (!g_nv_android_vm || !g_nv_android_host || !g_nv_android_mid_dispatch) return;

  int detach = 0;
  JNIEnv* env = nv_android_get_env(&detach);
  if (!env) return;

  jstring js1 = NULL;
  jstring js2 = NULL;
  if (s1) js1 = (*env)->NewStringUTF(env, s1);
  if (s2) js2 = (*env)->NewStringUTF(env, s2);

  (*env)->CallVoidMethod(env, g_nv_android_host, g_nv_android_mid_dispatch,
                         (jint)op, (jlong)(intptr_t)window, js1, js2);

  if (js1) (*env)->DeleteLocalRef(env, js1);
  if (js2) (*env)->DeleteLocalRef(env, js2);

  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
    __android_log_print(ANDROID_LOG_ERROR, NV_ANDROID_LOG_TAG, "Host dispatch exception (op=%d)", op);
  }

  nv_android_release_env(detach);
}

static void nv_android_window_create(nv_window_t* window) {
  if (!window) return;
  nv_arena_t* arena = nv_window_get_arena(window);
  if (!arena) return;
  nv_android_window_platform_t* p = (nv_android_window_platform_t*)nv_arena_alloc(arena, sizeof(*p));
  if (!p) return;
  memset(p, 0, sizeof(*p));
  p->window = window;
  nv_window_set_platform(window, p);
  nv_android_host_dispatch(NV_ANDROID_OP_WINDOW_CREATE, window, NULL, NULL);
}

static void nv_android_window_destroy(nv_window_t* window) {
  if (!window) return;
  nv_android_host_dispatch(NV_ANDROID_OP_WINDOW_DESTROY, window, NULL, NULL);
  nv_window_set_platform(window, NULL);
}

static void nv_android_window_show(nv_window_t* window) {
  if (!window) return;
  window->visible = 1;
  nv_android_host_dispatch(NV_ANDROID_OP_WINDOW_SHOW, window, NULL, NULL);
}

static void nv_android_window_hide(nv_window_t* window) {
  if (!window) return;
  window->visible = 0;
  nv_android_host_dispatch(NV_ANDROID_OP_WINDOW_HIDE, window, NULL, NULL);
}

static void nv_android_window_load_html(nv_window_t* window, const char* html, const char* base_url) {
  if (!window) return;
  nv_android_host_dispatch(NV_ANDROID_OP_WINDOW_LOAD_HTML, window, html, base_url);
}

static void nv_android_window_load_url(nv_window_t* window, const char* url) {
  if (!window) return;
  nv_android_host_dispatch(NV_ANDROID_OP_WINDOW_LOAD_URL, window, url, NULL);
}

static void nv_android_window_eval_js(nv_window_t* window, const char* js) {
  if (!window) return;
  nv_android_host_dispatch(NV_ANDROID_OP_WINDOW_EVAL_JS, window, js, NULL);
}

static void nv_android_smoke_on_message(nv_window_t* window, const char* event, const char* json, void* userdata) {
  (void)userdata;
  if (!window || !event || !json) return;
  if (strcmp(event, "ping") != 0) return;
  nv_send(window, "pong", json);
}

NV_INTERNAL void nv_app_platform_init(nv_app_t* app) {
  if (!app) return;
  app->platform_api.platform_name = "android";
  app->platform_api.window_create = nv_android_window_create;
  app->platform_api.window_destroy = nv_android_window_destroy;
  app->platform_api.window_show = nv_android_window_show;
  app->platform_api.window_hide = nv_android_window_hide;
  app->platform_api.window_load_html = nv_android_window_load_html;
  app->platform_api.window_load_url = nv_android_window_load_url;
  app->platform_api.window_eval_js = nv_android_window_eval_js;
}

NV_INTERNAL void nv_app_platform_run(nv_app_t* app) {
  (void)app;
}

NV_INTERNAL void nv_app_platform_quit(nv_app_t* app) {
  (void)app;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  (void)reserved;
  g_nv_android_vm = vm;
  return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeSetHost(
    JNIEnv* env, jclass clazz, jobject host) {
  (void)clazz;
  if (!env) return;

  if (g_nv_android_host) {
    (*env)->DeleteGlobalRef(env, g_nv_android_host);
    g_nv_android_host = NULL;
  }
  if (g_nv_android_host_cls) {
    (*env)->DeleteGlobalRef(env, g_nv_android_host_cls);
    g_nv_android_host_cls = NULL;
  }
  g_nv_android_mid_dispatch = NULL;

  if (!host) return;

  g_nv_android_host = (*env)->NewGlobalRef(env, host);
  jclass cls = (*env)->GetObjectClass(env, host);
  if (!cls) return;
  g_nv_android_host_cls = (*env)->NewGlobalRef(env, cls);
  (*env)->DeleteLocalRef(env, cls);

  g_nv_android_mid_dispatch = (*env)->GetMethodID(env, g_nv_android_host_cls, "dispatch",
                                                  "(IJLjava/lang/String;Ljava/lang/String;)V");
}

JNIEXPORT jstring JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeGetInjectScript(
    JNIEnv* env, jclass clazz) {
  (void)clazz;
  if (!env) return NULL;
  const char* raw = nv_ipc_inject_script();
  if (!raw) return NULL;
  return (*env)->NewStringUTF(env, raw);
}

JNIEXPORT jlong JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeCreateApp(
    JNIEnv* env, jclass clazz) {
  (void)env;
  (void)clazz;
  nv_app_t* app = nv_app_create();
  return (jlong)(intptr_t)app;
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeDestroyApp(
    JNIEnv* env, jclass clazz, jlong app_ptr) {
  (void)env;
  (void)clazz;
  nv_app_destroy((nv_app_t*)(intptr_t)app_ptr);
}

JNIEXPORT jlong JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeCreateWindow(
    JNIEnv* env, jclass clazz, jlong app_ptr, jstring title, jint width, jint height) {
  (void)clazz;
  if (!env) return 0;
  nv_app_t* app = (nv_app_t*)(intptr_t)app_ptr;
  if (!app) return 0;

  const char* c_title = title ? (*env)->GetStringUTFChars(env, title, NULL) : NULL;

  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = c_title ? c_title : "nativeview android";
  cfg.width = (int)width;
  cfg.height = (int)height;
  cfg.resizable = 0;
  cfg.frameless = 1;

  nv_window_t* window = nv_window_create(app, &cfg);

  if (c_title) (*env)->ReleaseStringUTFChars(env, title, c_title);
  return (jlong)(intptr_t)window;
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeShowWindow(
    JNIEnv* env, jclass clazz, jlong window_ptr) {
  (void)env;
  (void)clazz;
  nv_window_show((nv_window_t*)(intptr_t)window_ptr);
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeLoadHtml(
    JNIEnv* env, jclass clazz, jlong window_ptr, jstring html, jstring base_url) {
  (void)clazz;
  if (!env) return;
  nv_window_t* window = (nv_window_t*)(intptr_t)window_ptr;
  if (!window) return;

  const char* c_html = html ? (*env)->GetStringUTFChars(env, html, NULL) : NULL;
  const char* c_base = base_url ? (*env)->GetStringUTFChars(env, base_url, NULL) : NULL;

  nv_load_html(window, c_html ? c_html : "", c_base);

  if (c_html) (*env)->ReleaseStringUTFChars(env, html, c_html);
  if (c_base) (*env)->ReleaseStringUTFChars(env, base_url, c_base);
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeEvalJs(
    JNIEnv* env, jclass clazz, jlong window_ptr, jstring js) {
  (void)clazz;
  if (!env) return;
  nv_window_t* window = (nv_window_t*)(intptr_t)window_ptr;
  if (!window) return;

  const char* c_js = js ? (*env)->GetStringUTFChars(env, js, NULL) : NULL;
  nv_eval_js(window, c_js ? c_js : "");
  if (c_js) (*env)->ReleaseStringUTFChars(env, js, c_js);
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeInstallSmokeHandlers(
    JNIEnv* env, jclass clazz, jlong window_ptr) {
  (void)env;
  (void)clazz;
  nv_window_t* window = (nv_window_t*)(intptr_t)window_ptr;
  if (!window) return;
  nv_on_message(window, nv_android_smoke_on_message, NULL);
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeOnWebMessage(
    JNIEnv* env, jclass clazz, jlong window_ptr, jstring wire_json) {
  (void)clazz;
  if (!env) return;
  nv_window_t* window = (nv_window_t*)(intptr_t)window_ptr;
  if (!window) return;
  nv_ipc_state_t* ipc = nv_window_get_ipc(window);
  if (!ipc) return;

  const char* raw = wire_json ? (*env)->GetStringUTFChars(env, wire_json, NULL) : NULL;
  if (raw) {
    nv_ipc_dispatch(window, ipc, raw);
    (*env)->ReleaseStringUTFChars(env, wire_json, raw);
  }
}

JNIEXPORT void JNICALL Java_com_nativeview_androidrunner_NativeviewBridge_nativeOnReady(
    JNIEnv* env, jclass clazz, jlong window_ptr) {
  (void)env;
  (void)clazz;
  nv_window_t* window = (nv_window_t*)(intptr_t)window_ptr;
  if (!window) return;
  nv_ipc_state_t* ipc = nv_window_get_ipc(window);
  if (!ipc) return;
  nv_ipc_invoke_ready(window, ipc);
}
