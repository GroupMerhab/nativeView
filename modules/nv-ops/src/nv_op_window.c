#include "nv_op_window.h"
#include "nv_window_internal.h"
#include "nv_menu.h"
#include "nv.h"
#include "nv_arena.h"
#include <string.h>

/* Reply helper prototypes (implemented in nv_ipc.c). Tests may override them
 * by defining NV_TEST which provides test-specific implementations. */
NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

static int g_last_ok = 0;
static int g_last_seq = 0;
static const char* g_last_json = NULL;
static char g_last_err[64];

NV_INTERNAL void nv_test_reset_replies(void) {
  g_last_ok = 0;
  g_last_seq = 0;
  g_last_json = NULL;
  g_last_err[0] = '\0';
}

NV_INTERNAL int nv_test_last_reply_ok(void) { return g_last_ok; }
NV_INTERNAL const char* nv_test_last_reply_json(void) { return g_last_json; }
NV_INTERNAL const char* nv_test_last_reply_error_code(void) { return g_last_err[0] ? g_last_err : NULL; }
/* Test-only reply interception: when building tests, override reply functions
 * so tests can inspect what would be sent. Guard with NV_TEST so production
 * builds use the real `nv_ipc_reply_*` implementations in `nv_ipc.c`. */
#ifdef NV_TEST
NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena) {
  (void)w;
  g_last_ok = 1;
  g_last_seq = seq;
  g_last_err[0] = '\0';
  g_last_json = result ? nv_json_serialize(result) : NULL;
}

NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena) {
  (void)w; (void)message; (void)arena;
  g_last_ok = 0;
  g_last_seq = seq;
  strncpy(g_last_err, code ? code : "ERR_UNKNOWN", sizeof(g_last_err)-1);
  g_last_err[sizeof(g_last_err)-1] = '\0';
  g_last_json = NULL;
}
#endif

static int require_int(const nv_json_val_t* args, const char* key, long long* out) {
  if (!args || !key || !out) return -1;
  *out = nv_json_get_int(args, key);
  return 0;
}

static const char* require_str(const nv_json_val_t* args, const char* key) {
  return args ? nv_json_get_str(args, key) : NULL;
}

static int require_bool(const nv_json_val_t* args, const char* key, int* out) {
  if (!args || !key || !out) return -1;
  *out = nv_json_get_bool(args, key);
  return 0;
}

NV_INTERNAL void nv_op_window_set_title(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* title = require_str(args, "title");
  if (!title) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'title'", arena); return; }
  nv_window_set_title(w, title);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_set_size(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long ww = 0, hh = 0;
  if (require_int(args, "w", &ww) != 0 || require_int(args, "h", &hh) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'w' or 'h'", arena); return;
  }
  nv_window_set_size(w, (int)ww, (int)hh);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_get_size(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  int ww = 0, hh = 0;
  nv_window_get_size(w, &ww, &hh);
  nv_json_t* obj = nv_json_object(arena);
  nv_json_int(obj, "w", ww);
  nv_json_int(obj, "h", hh);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_window_set_position(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long xx = 0, yy = 0;
  if (require_int(args, "x", &xx) != 0 || require_int(args, "y", &yy) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'x' or 'y'", arena); return;
  }
  nv_window_set_position(w, (int)xx, (int)yy);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_get_position(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  int x = 0, y = 0;
  nv_window_get_position(w, &x, &y);
  nv_json_t* obj = nv_json_object(arena);
  nv_json_int(obj, "x", x);
  nv_json_int(obj, "y", y);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_window_set_min_size(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long ww = 0, hh = 0;
  if (require_int(args, "w", &ww) != 0 || require_int(args, "h", &hh) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'w' or 'h'", arena); return;
  }
  nv_window_set_min_size(w, (int)ww, (int)hh);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_center(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_window_center(w);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_minimize(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_window_minimize(w);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_maximize(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_window_maximize(w);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_restore(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_window_restore(w);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_fullscreen(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  int enable = 0;
  if (require_bool(args, "enable", &enable) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'enable'", arena); return;
  }
  nv_window_fullscreen(w, enable ? 1 : 0);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_is_fullscreen(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  int is = nv_window_is_fullscreen(w) ? 1 : 0;
  nv_json_t* obj = nv_json_object(arena);
  nv_json_bool(obj, "value", is);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_window_close(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_window_request_close(w);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_focus(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  nv_window_focus(w);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_is_focused(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  int is = nv_window_is_focused(w) ? 1 : 0;
  nv_json_t* obj = nv_json_object(arena);
  nv_json_bool(obj, "value", is);
  nv_ipc_reply_ok(w, seq, obj, arena);
}

NV_INTERNAL void nv_op_window_set_resizable(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  int resizable = 0;
  if (require_bool(args, "resizable", &resizable) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'resizable'", arena); return;
  }
  nv_window_set_resizable(w, resizable ? 1 : 0);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_set_always_on_top(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  int on_top = 0;
  if (require_bool(args, "enable", &on_top) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'enable'", arena); return;
  }
  nv_window_set_always_on_top(w, on_top ? 1 : 0);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_set_opacity(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long opacity_pct = 0;
  if (require_int(args, "opacity", &opacity_pct) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'opacity'", arena); return;
  }
  nv_window_set_opacity(w, (int)opacity_pct);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_set_zoom_factor(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  if (!args) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing args", arena); return; }
  double factor = nv_json_get_double(args, "factor");
  if (factor <= 0.0) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "invalid 'factor'", arena); return; }
  nv_window_set_zoom_factor(w, factor);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

static void nv_op_dispatch_context_menu_platform(nv_window_t* w, const nv_menu_item_t* items, int count) {
  nv_mac_window_set_context_menu(w, items, count);
  nv_win_window_set_context_menu(w, items, count);
  nv_linux_window_set_context_menu(w, items, count);
}

static int parse_ctx_menu_skip_sep(const nv_json_val_t* el) {
  if (!el || !nv_json_is_object(el)) return 0;
  if (nv_json_get_bool(el, "separator")) return 1;
  return nv_json_get_int(el, "separator") != 0;
}

static int parse_one_ctx_item(nv_arena_t* wa, const nv_json_val_t* el, nv_menu_item_t* out) {
  memset(out, 0, sizeof(*out));
  if (!el || !nv_json_is_object(el)) return -1;
  if (parse_ctx_menu_skip_sep(el)) return 1;
  const char* id = nv_json_get_str(el, "id");
  const char* label = nv_json_get_str(el, "label");
  if (!id || id[0] == '\0' || !label) return -1;
  out->id = nv_arena_str_dup(wa, id);
  out->label = nv_arena_str_dup(wa, label);
  if (!out->id || !out->label) return -1;
  out->enabled = nv_json_get_bool(el, "disabled") ? 0 : 1;
  return 0;
}

static int parse_ctx_menu_items(nv_arena_t* wa, const nv_json_val_t* arr, nv_menu_item_t** out_items,
                                int* out_count) {
  if (!nv_json_is_array(arr)) return -1;
  size_t n = nv_json_array_len(arr);
  if (n == 0) {
    *out_items = NULL;
    *out_count = 0;
    return 0;
  }
  nv_menu_item_t* items = (nv_menu_item_t*)nv_arena_alloc(wa, n * sizeof(nv_menu_item_t));
  if (!items) return -1;
  int c = 0;
  for (size_t i = 0; i < n; i++) {
    nv_json_val_t* el = nv_json_array_get(arr, i);
    int pr = parse_one_ctx_item(wa, el, &items[c]);
    if (pr == 0) {
      c++;
    } else if (pr < 0) {
      return -1;
    }
  }
  *out_items = items;
  *out_count = c;
  return 0;
}

NV_INTERNAL void nv_op_window_set_context_menu(nv_window_t* w, int seq, const nv_json_val_t* args,
                                               nv_arena_t* arena) {
  (void)arena;
  if (!w) {
    return;
  }
  const nv_json_val_t* arr = args;
  if (args && nv_json_is_object(args)) {
    arr = nv_json_get_obj(args, "items");
  }
  if (!arr || !nv_json_is_array(arr)) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "expected { \"items\": [ ... ] }", arena);
    return;
  }
  nv_arena_t* wa = nv_window_get_arena(w);
  if (!wa) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "window arena missing", arena);
    return;
  }
  nv_menu_item_t* items = NULL;
  int count = 0;
  if (parse_ctx_menu_items(wa, arr, &items, &count) != 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "failed to parse context menu items", arena);
    return;
  }
  w->ctx_menu_items = count > 0 ? items : NULL;
  w->ctx_menu_count = count;
  nv_op_dispatch_context_menu_platform(w, w->ctx_menu_items, w->ctx_menu_count);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}
