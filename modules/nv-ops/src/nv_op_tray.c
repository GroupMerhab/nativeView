#include "nv_op_tray.h"
#include "nv.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"

#include <stddef.h>
#include <string.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

enum { nv_op_tray_menu_cap = 64 };

static void nv_op_tray_reply_platform_err(nv_window_t* w, int seq, int rc, nv_arena_t* arena) {
  if (rc == NV_PLATFORM_RC_NOT_SUPPORTED) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "tray not available on this system", arena);
    return;
  }
  (void)rc;
  nv_ipc_reply_err(w, seq, "ERR_IO", "tray operation failed", arena);
}

NV_INTERNAL void nv_op_tray_create(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  if (id <= 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'id'", arena);
    return;
  }
  const char* icon = args ? nv_json_get_str(args, "icon") : NULL;
  if (!icon) {
    icon = args ? nv_json_get_str(args, "path") : NULL;
  }
  const char* tooltip = args ? nv_json_get_str(args, "tooltip") : NULL;
  if (!tooltip) {
    tooltip = args ? nv_json_get_str(args, "text") : NULL;
  }
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->tray_create) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "tray not supported", arena);
    return;
  }
  int rc = api->tray_create(id, icon, tooltip, w);
  if (rc != 0) {
    nv_op_tray_reply_platform_err(w, seq, rc, arena);
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_tray_destroy(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  if (id <= 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'id'", arena);
    return;
  }
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->tray_destroy) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "tray not supported", arena);
    return;
  }
  int rc = api->tray_destroy(id);
  if (rc != 0) {
    nv_op_tray_reply_platform_err(w, seq, rc, arena);
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_tray_set_icon(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (!path) {
    path = args ? nv_json_get_str(args, "icon") : NULL;
  }
  if (id <= 0 || !path) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'id' or icon path", arena);
    return;
  }
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->tray_set_icon) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "tray not supported", arena);
    return;
  }
  int rc = api->tray_set_icon(id, path);
  if (rc != 0) {
    nv_op_tray_reply_platform_err(w, seq, rc, arena);
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_tray_set_tooltip(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  const char* text = args ? nv_json_get_str(args, "text") : NULL;
  if (!text) {
    text = args ? nv_json_get_str(args, "tooltip") : NULL;
  }
  if (id <= 0 || !text) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'id' or tooltip text", arena);
    return;
  }
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->tray_set_tooltip) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "tray not supported", arena);
    return;
  }
  int rc = api->tray_set_tooltip(id, text);
  if (rc != 0) {
    nv_op_tray_reply_platform_err(w, seq, rc, arena);
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_tray_set_menu(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  if (id <= 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'id'", arena);
    return;
  }
  nv_json_val_t* items = args ? nv_json_get_obj(args, "items") : NULL;
  if (!items || !nv_json_is_array(items)) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'items' array", arena);
    return;
  }
  size_t n = nv_json_array_len(items);
  const char* label_ptrs[nv_op_tray_menu_cap];
  long long item_ids[nv_op_tray_menu_cap];
  int count = 0;
  for (size_t i = 0; i < n && count < nv_op_tray_menu_cap; i++) {
    nv_json_val_t* el = nv_json_array_get(items, i);
    if (!el || !nv_json_is_object(el)) {
      continue;
    }
    if (nv_json_get_bool(el, "separator")) {
      label_ptrs[count] = "";
      item_ids[count] = -1;
      count++;
      continue;
    }
    const char* lbl = nv_json_get_str(el, "label");
    if (!lbl) {
      lbl = "";
    }
    long long item_id = nv_json_get_int(el, "id");
    label_ptrs[count] = lbl;
    item_ids[count] = item_id;
    count++;
  }
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->tray_set_menu) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "tray not supported", arena);
    return;
  }
  int rc = api->tray_set_menu(id, label_ptrs, item_ids, count);
  if (rc != 0) {
    nv_op_tray_reply_platform_err(w, seq, rc, arena);
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_tray_clicked_push(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)seq;
  long long id = args ? nv_json_get_int(args, "id") : 0;
  nv_json_t* ev = nv_json_object(arena);
  nv_json_int(ev, "id", id);
  const char* payload = nv_json_serialize(ev);
  nv_send(w, "tray.clicked", payload);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_tray_menu_action_push(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)seq;
  long long id = args ? nv_json_get_int(args, "id") : 0;
  long long itemId = args ? nv_json_get_int(args, "itemId") : 0;
  nv_json_t* ev = nv_json_object(arena);
  nv_json_int(ev, "id", id);
  nv_json_int(ev, "itemId", itemId);
  const char* payload = nv_json_serialize(ev);
  nv_send(w, "tray.menuAction", payload);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}
