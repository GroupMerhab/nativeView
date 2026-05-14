#include "nv_op_app.h"
#include "nv_window_internal.h"
#include "nv_window_manager.h"
#include "util/nv_log.h"
#include "nv.h"
#include "nv_core_internal.h"
#include <stdlib.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

NV_INTERNAL void nv_op_app_quit(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  if (w && w->app) {
    nv_app_quit(w->app);
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_app_get_version(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "version", nv_version_string());
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_app_get_name(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "name", "nativeview");
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_app_get_data_dir(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_json_t* obj = nv_json_object(arena);
  const char* path = "";
  char* heap = NULL;
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (api && api->app_get_data_dir) {
    heap = api->app_get_data_dir();
  }
  if (heap) path = heap;
  nv_json_str(obj, "path", path);
  if (heap) free(heap);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_app_get_exe_path(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_json_t* obj = nv_json_object(arena);
  const char* path = "";
  char* heap = NULL;
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (api && api->app_get_exe_path) {
    heap = api->app_get_exe_path();
  }
  if (heap) path = heap;
  nv_json_str(obj, "path", path);
  if (heap) free(heap);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_app_get_resource_dir(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_json_t* obj = nv_json_object(arena);
  const char* path = "";
  char* heap = NULL;
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (api && api->app_get_resource_dir) {
    heap = api->app_get_resource_dir();
  }
  if (heap) path = heap;
  nv_json_str(obj, "path", path);
  if (heap) free(heap);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_app_get_platform(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_json_t* obj = nv_json_object(arena);
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  const char* p = (api && api->platform_name) ? api->platform_name : "unknown";
  nv_json_str(obj, "platform", p);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_app_get_locale(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_json_t* obj = nv_json_object(arena);
  const char* loc = "en-US";
  char* heap = NULL;
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (api && api->app_get_locale) {
    heap = api->app_get_locale();
  }
  if (heap && heap[0]) loc = heap;
  nv_json_str(obj, "locale", loc);
  if (heap) free(heap);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_app_handshake(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* js_ver = args ? nv_json_get_str(args, "jsVersion") : NULL;
  long long wire = args ? nv_json_get_int(args, "wireVersion") : 0;
  (void)js_ver; /* not used for gating right now */
#ifndef NV_WIRE_VERSION
#define NV_WIRE_VERSION 1
#endif
  if ((int)wire != NV_WIRE_VERSION) {
    nv_ipc_reply_err(w, seq, "ERR_VERSION_MISMATCH", "wire protocol mismatch", arena);
    return;
  }
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "cVersion", nv_version_string());
  nv_json_int(obj, "wireVersion", NV_WIRE_VERSION);

  // MW: include windowId in handshake reply
  const char* win_id = nv_wm_get_id_by_window(w);
  if (!win_id) {
    win_id = "main";
    NV_LOG("WARNING: nv_op_app_handshake: window not registered, fallback to 'main'");
  }
  nv_json_str(obj, "windowId", win_id);

  nv_ipc_reply_ok(w, seq, obj, arena);
}
