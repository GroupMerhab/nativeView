#include "nv_op_screen.h"
#include "nv.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include <stdlib.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

static char* screen_fetch_all(nv_window_t* w) {
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->screen_get_all) return NULL;
  return api->screen_get_all();
}

static char* screen_fetch_primary(nv_window_t* w) {
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->screen_get_primary) return NULL;
  return api->screen_get_primary();
}

static char* screen_fetch_cursor(nv_window_t* w) {
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  if (!api || !api->screen_get_cursor) return NULL;
  return api->screen_get_cursor();
}

static nv_json_t* nv_screen_rect_builder(nv_arena_t* a, const nv_json_val_t* r) {
  nv_json_t* o = nv_json_object(a);
  if (!o) return NULL;
  nv_json_int(o, "x", nv_json_get_int(r, "x"));
  nv_json_int(o, "y", nv_json_get_int(r, "y"));
  nv_json_int(o, "width", nv_json_get_int(r, "width"));
  nv_json_int(o, "height", nv_json_get_int(r, "height"));
  return o;
}

static nv_json_t* nv_screen_display_builder(nv_arena_t* a, const nv_json_val_t* src) {
  nv_json_t* o = nv_json_object(a);
  if (!o || !nv_json_is_object(src)) return o;

  nv_json_int(o, "id", nv_json_get_int(src, "id"));
  nv_json_int(o, "x", nv_json_get_int(src, "x"));
  nv_json_int(o, "y", nv_json_get_int(src, "y"));
  nv_json_int(o, "width", nv_json_get_int(src, "width"));
  nv_json_int(o, "height", nv_json_get_int(src, "height"));

  {
    double sc = nv_json_get_double(src, "scaleFactor");
    if (sc == 0.0) sc = 1.0;
    nv_json_double(o, "scale", sc);
    nv_json_double(o, "scaleFactor", sc);
  }

  {
    const char* nm = nv_json_get_str(src, "localizedName");
    if (nm && nm[0]) {
      nv_json_str(o, "name", nm);
      nv_json_str(o, "localizedName", nm);
    }
  }

  nv_json_bool(o, "isPrimary", nv_json_get_bool(src, "isPrimary"));

  {
    nv_json_val_t* b = nv_json_get_obj(src, "bounds");
    nv_json_t* bo = b ? nv_screen_rect_builder(a, b) : NULL;
    if (!bo) {
      bo = nv_json_object(a);
      if (bo) {
        nv_json_int(bo, "x", nv_json_get_int(src, "x"));
        nv_json_int(bo, "y", nv_json_get_int(src, "y"));
        nv_json_int(bo, "width", nv_json_get_int(src, "width"));
        nv_json_int(bo, "height", nv_json_get_int(src, "height"));
      }
    }
    if (bo) nv_json_nest(o, "bounds", bo);
  }

  {
    nv_json_val_t* wa = nv_json_get_obj(src, "workArea");
    if (wa) {
      nv_json_t* wao = nv_screen_rect_builder(a, wa);
      if (wao) nv_json_nest(o, "workArea", wao);
    }
  }

  return o;
}

static nv_json_t* nv_screen_root_from_heap(nv_arena_t* arena, char* heap, int mode) {
  nv_json_val_t* parsed = NULL;
  nv_json_t* out = NULL;

  if (!heap) return NULL;
  parsed = nv_json_parse(arena, heap);
  free(heap);
  heap = NULL;
  if (!parsed) return NULL;

  if (mode == 0) {
    /* getAll: root is array of displays -> {"displays":[...]} */
    if (!nv_json_is_array(parsed)) return NULL;
    nv_json_t* arr = nv_json_array(arena);
    if (!arr) return NULL;
    for (size_t i = 0; i < nv_json_array_len(parsed); i++) {
      nv_json_val_t* el = nv_json_array_get(parsed, i);
      nv_json_t* one = nv_screen_display_builder(arena, el);
      if (one) nv_json_array_push(arr, one);
    }
    out = nv_json_object(arena);
    if (!out) return NULL;
    nv_json_nest(out, "displays", arr);
    return out;
  }

  if (mode == 1) {
    /* getPrimary: single display object */
    if (!nv_json_is_object(parsed)) return NULL;
    return nv_screen_display_builder(arena, parsed);
  }

  /* getCursor: {"x":..,"y":..} */
  if (!nv_json_is_object(parsed)) return NULL;
  out = nv_json_object(arena);
  if (!out) return NULL;
  nv_json_int(out, "x", nv_json_get_int(parsed, "x"));
  nv_json_int(out, "y", nv_json_get_int(parsed, "y"));
  return out;
}

static void screen_reply_from_heap(nv_window_t* w, int seq, char* heap, int mode, nv_arena_t* arena) {
  nv_json_t* root = nv_screen_root_from_heap(arena, heap, mode);
  if (!root) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "screen query failed", arena);
    return;
  }
  nv_ipc_reply_ok(w, seq, root, arena);
}

NV_INTERNAL void nv_op_screen_get_all(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  char* heap = screen_fetch_all(w);
  if (!heap) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_IMPLEMENTED", "screen.getAll not implemented on this platform", arena);
    return;
  }
  screen_reply_from_heap(w, seq, heap, 0, arena);
}

NV_INTERNAL void nv_op_screen_get_primary(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  char* heap = screen_fetch_primary(w);
  if (!heap) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_IMPLEMENTED", "screen.getPrimary not implemented on this platform", arena);
    return;
  }
  screen_reply_from_heap(w, seq, heap, 1, arena);
}

NV_INTERNAL void nv_op_screen_get_cursor(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  (void)args;
  char* heap = screen_fetch_cursor(w);
  if (!heap) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_IMPLEMENTED", "screen.getCursorPos not implemented on this platform", arena);
    return;
  }
  screen_reply_from_heap(w, seq, heap, 2, arena);
}
