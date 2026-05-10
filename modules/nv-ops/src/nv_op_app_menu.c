/* app.setMenu — JSON menu tree → nv_menu_item_t → nv_app_set_menu */

#include "nv_op_app.h"
#include "nv_window_internal.h"
#include "nv_menu.h"
#include "nv_json.h"
#include "nv.h"
#include <string.h>
#include <stddef.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message,
                                  nv_arena_t* arena);

static int menu_is_separator(const nv_json_val_t* el) {
  if (!el || !nv_json_is_object(el)) {
    return 0;
  }
  if (nv_json_get_bool(el, "separator")) {
    return 1;
  }
  return nv_json_get_int(el, "separator") != 0;
}

static int parse_one_item(nv_arena_t* arena, const nv_json_val_t* el, nv_menu_item_t* out);

static int parse_children(nv_arena_t* arena, const nv_json_val_t* ch, nv_menu_item_t** out_kids,
                          int* out_n) {
  if (!nv_json_is_array(ch)) {
    return -1;
  }
  size_t n = nv_json_array_len(ch);
  if (n == 0) {
    *out_kids = NULL;
    *out_n = 0;
    return 0;
  }
  nv_menu_item_t* kids = (nv_menu_item_t*)nv_arena_alloc(arena, n * sizeof(nv_menu_item_t));
  if (!kids) {
    return -1;
  }
  int k = 0;
  for (size_t i = 0; i < n; i++) {
    nv_json_val_t* el = nv_json_array_get(ch, i);
    if (parse_one_item(arena, el, &kids[k]) == 0) {
      k++;
    }
  }
  *out_kids = kids;
  *out_n = k;
  return 0;
}

static int parse_one_item(nv_arena_t* arena, const nv_json_val_t* el, nv_menu_item_t* out) {
  memset(out, 0, sizeof(*out));
  if (!el || !nv_json_is_object(el)) {
    return -1;
  }
  if (menu_is_separator(el)) {
    out->separator = 1;
    return 0;
  }
  const char* id = nv_json_get_str(el, "id");
  const char* label = nv_json_get_str(el, "label");
  const char* sc = nv_json_get_str(el, "shortcut");
  out->id = id ? nv_arena_str_dup(arena, id) : nv_arena_str_dup(arena, "");
  out->label = label ? nv_arena_str_dup(arena, label) : nv_arena_str_dup(arena, "");
  out->shortcut = sc ? nv_arena_str_dup(arena, sc) : NULL;
  if (!out->id || !out->label) {
    return -1;
  }
  out->enabled = nv_json_get_bool(el, "disabled") ? 0 : 1;
  nv_json_val_t* ch = nv_json_get_obj(el, "children");
  if (ch && nv_json_is_array(ch)) {
    if (parse_children(arena, ch, &out->children, &out->child_count) != 0) {
      return -1;
    }
  }
  return 0;
}

static int parse_root_array(nv_arena_t* arena, const nv_json_val_t* arr, nv_menu_item_t** out_items,
                            int* out_count) {
  if (!nv_json_is_array(arr)) {
    return -1;
  }
  size_t n = nv_json_array_len(arr);
  if (n == 0) {
    *out_items = NULL;
    *out_count = 0;
    return 0;
  }
  nv_menu_item_t* items = (nv_menu_item_t*)nv_arena_alloc(arena, n * sizeof(nv_menu_item_t));
  if (!items) {
    return -1;
  }
  int c = 0;
  for (size_t i = 0; i < n; i++) {
    nv_json_val_t* el = nv_json_array_get(arr, i);
    if (parse_one_item(arena, el, &items[c]) == 0) {
      c++;
    }
  }
  *out_items = items;
  *out_count = c;
  return 0;
}

NV_INTERNAL void nv_op_app_set_menu(nv_window_t* w, int seq, const nv_json_val_t* args,
                                    nv_arena_t* arena) {
#if defined(_WIN32)
  (void)args;
  (void)arena;
  if (!w) {
    return;
  }
  nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "app menu is not supported on Windows", arena);
#else
  if (!w || !w->app) {
    if (w) {
      nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "no application context", arena);
    }
    return;
  }
  const nv_json_val_t* arr = args;
  if (args && nv_json_is_object(args)) {
    arr = nv_json_get_obj(args, "items");
  }
  if (!arr || !nv_json_is_array(arr)) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "expected JSON array of menu items (or {items:[]})",
                     arena);
    return;
  }
  nv_menu_item_t* items = NULL;
  int count = 0;
  if (parse_root_array(arena, arr, &items, &count) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "failed to parse menu items", arena);
    return;
  }
  nv_app_set_menu(w->app, items, count);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
#endif
}
