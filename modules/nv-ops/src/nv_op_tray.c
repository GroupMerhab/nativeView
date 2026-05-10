#include "nv_op_tray.h"
#include "nv.h"
#include "nv_window_internal.h"

#include <stddef.h>
#include <string.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

#ifdef __APPLE__
NV_INTERNAL int nv_mac_tray_create(long long id, const char* icon_path,
                                   const char* tooltip, nv_window_t* w);
NV_INTERNAL int nv_mac_tray_destroy(long long id);
NV_INTERNAL int nv_mac_tray_set_icon(long long id, const char* icon_path);
NV_INTERNAL int nv_mac_tray_set_tooltip(long long id, const char* tooltip);
NV_INTERNAL int nv_mac_tray_set_menu(long long id, const char** labels,
                                     const long long* item_ids, int count);
#elif defined(_WIN32)
#if (defined(__GNUC__) || defined(__clang__))
/* Windows: real symbols from nv-platform-win; weak fallbacks for nv-ops-only links. */
__attribute__((weak)) int nv_win_tray_create(long long id, const char* icon_path, const char* tooltip,
                                              nv_window_t* w) {
  (void)id;
  (void)icon_path;
  (void)tooltip;
  (void)w;
  return -1;
}
__attribute__((weak)) int nv_win_tray_destroy(long long id) {
  (void)id;
  return -1;
}
__attribute__((weak)) int nv_win_tray_set_icon(long long id, const char* icon_path) {
  (void)id;
  (void)icon_path;
  return -1;
}
__attribute__((weak)) int nv_win_tray_set_tooltip(long long id, const char* tooltip) {
  (void)id;
  (void)tooltip;
  return -1;
}
__attribute__((weak)) int nv_win_tray_set_menu(long long id, const char** labels, const long long* item_ids,
                                               int count) {
  (void)id;
  (void)labels;
  (void)item_ids;
  (void)count;
  return -1;
}
#endif
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER) && defined(__linux__)
/* Linux: nv_linux_tray_* from nv-platform-linux (weak); no nv_mac stubs in this TU. */
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
/* Non-mac non-linux: weak nv_mac_* stubs (BSD, etc.); unit tests may override with strong mocks. */
__attribute__((weak)) int nv_mac_tray_create(long long id, const char* icon_path,
                                              const char* tooltip, nv_window_t* w) {
  (void)id;
  (void)icon_path;
  (void)tooltip;
  (void)w;
  return -1;
}
__attribute__((weak)) int nv_mac_tray_destroy(long long id) {
  (void)id;
  return -1;
}
__attribute__((weak)) int nv_mac_tray_set_icon(long long id, const char* icon_path) {
  (void)id;
  (void)icon_path;
  return -1;
}
__attribute__((weak)) int nv_mac_tray_set_tooltip(long long id, const char* tooltip) {
  (void)id;
  (void)tooltip;
  return -1;
}
__attribute__((weak)) int nv_mac_tray_set_menu(long long id, const char** labels,
                                               const long long* item_ids, int count) {
  (void)id;
  (void)labels;
  (void)item_ids;
  (void)count;
  return -1;
}
#else
/* MSVC and other toolchains: strong not-implemented stubs (tests use real platform on macOS). */
static int nv_mac_tray_create_stub(long long id, const char* icon_path, const char* tooltip, nv_window_t* w) {
  (void)id;
  (void)icon_path;
  (void)tooltip;
  (void)w;
  return -1;
}
static int nv_mac_tray_destroy_stub(long long id) {
  (void)id;
  return -1;
}
static int nv_mac_tray_set_icon_stub(long long id, const char* icon_path) {
  (void)id;
  (void)icon_path;
  return -1;
}
static int nv_mac_tray_set_tooltip_stub(long long id, const char* tooltip) {
  (void)id;
  (void)tooltip;
  return -1;
}
static int nv_mac_tray_set_menu_stub(long long id, const char** labels, const long long* item_ids, int count) {
  (void)id;
  (void)labels;
  (void)item_ids;
  (void)count;
  return -1;
}
#define nv_mac_tray_create     nv_mac_tray_create_stub
#define nv_mac_tray_destroy    nv_mac_tray_destroy_stub
#define nv_mac_tray_set_icon   nv_mac_tray_set_icon_stub
#define nv_mac_tray_set_tooltip nv_mac_tray_set_tooltip_stub
#define nv_mac_tray_set_menu   nv_mac_tray_set_menu_stub
#endif

enum { nv_op_tray_menu_cap = 64 };

static void nv_op_tray_reply_platform_err(nv_window_t* w, int seq, int rc, nv_arena_t* arena) {
#if defined(__linux__) && !defined(__APPLE__)
  if (rc == NV_TRAY_RC_NOT_SUPPORTED) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "tray not available on this system", arena);
    return;
  }
#endif
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
  int rc;
#ifdef __APPLE__
  rc = nv_mac_tray_create(id, icon, tooltip, w);
#elif defined(_WIN32)
  rc = nv_win_tray_create(id, icon, tooltip, w);
#elif defined(__linux__)
  rc = nv_linux_tray_create(id, icon, tooltip, w);
#else
  rc = nv_mac_tray_create(id, icon, tooltip, w);
#endif
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
  int rc;
#ifdef __APPLE__
  rc = nv_mac_tray_destroy(id);
#elif defined(_WIN32)
  rc = nv_win_tray_destroy(id);
#elif defined(__linux__)
  rc = nv_linux_tray_destroy(id);
#else
  rc = nv_mac_tray_destroy(id);
#endif
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
  int rc;
#ifdef __APPLE__
  rc = nv_mac_tray_set_icon(id, path);
#elif defined(_WIN32)
  rc = nv_win_tray_set_icon(id, path);
#elif defined(__linux__)
  rc = nv_linux_tray_set_icon(id, path);
#else
  rc = nv_mac_tray_set_icon(id, path);
#endif
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
  int rc;
#ifdef __APPLE__
  rc = nv_mac_tray_set_tooltip(id, text);
#elif defined(_WIN32)
  rc = nv_win_tray_set_tooltip(id, text);
#elif defined(__linux__)
  rc = nv_linux_tray_set_tooltip(id, text);
#else
  rc = nv_mac_tray_set_tooltip(id, text);
#endif
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
  int rc;
#ifdef __APPLE__
  rc = nv_mac_tray_set_menu(id, label_ptrs, item_ids, count);
#elif defined(_WIN32)
  rc = nv_win_tray_set_menu(id, label_ptrs, item_ids, count);
#elif defined(__linux__)
  rc = nv_linux_tray_set_menu(id, label_ptrs, item_ids, count);
#else
  rc = nv_mac_tray_set_menu(id, label_ptrs, item_ids, count);
#endif
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
