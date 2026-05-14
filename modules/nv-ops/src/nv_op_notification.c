#include "nv_op_notification.h"
#include "nv.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

enum { NV_RC_NOT_SUPPORTED = -100 };

NV_INTERNAL void nv_op_notification_show(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  if (id <= 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'id'", arena);
    return;
  }
  const char* title = args ? nv_json_get_str(args, "title") : NULL;
  const char* body = args ? nv_json_get_str(args, "body") : NULL;
  const char* icon = args ? nv_json_get_str(args, "icon") : NULL;
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->notification_show) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "notification.show not supported", arena);
    return;
  }
  int rc = api->notification_show(id, title, body, icon, w);
  if (rc != 0) {
    if (rc == NV_RC_NOT_SUPPORTED) {
      nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "notifications not available", arena);
    } else if (rc == -1) {
      nv_ipc_reply_err(w, seq, "ERR_PERMISSION", "notifications disabled or blocked for this app", arena);
    } else {
      nv_ipc_reply_err(w, seq, "ERR_IO", "notification.show failed", arena);
    }
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_notification_close(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  if (id <= 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'id'", arena);
    return;
  }
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->notification_close) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "notification.close not supported", arena);
    return;
  }
  int rc = api->notification_close(id);
  if (rc != 0) {
    if (rc == NV_RC_NOT_SUPPORTED) {
      nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "notifications not available", arena);
    } else {
      nv_ipc_reply_err(w, seq, "ERR_IO", "notification.close failed", arena);
    }
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_notification_clicked_push(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)seq;
  long long id = args ? nv_json_get_int(args, "id") : 0;
  nv_json_t* ev = nv_json_object(arena);
  nv_json_int(ev, "id", id);
  const char* payload = nv_json_serialize(ev);
  nv_send(w, "notification.clicked", payload);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_notification_closed_push(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)seq;
  long long id = args ? nv_json_get_int(args, "id") : 0;
  nv_json_t* ev = nv_json_object(arena);
  nv_json_int(ev, "id", id);
  const char* payload = nv_json_serialize(ev);
  nv_send(w, "notification.closed", payload);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}
