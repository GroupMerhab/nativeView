/* =============================================================================
 * nv_vue_slot.c - Vue component slot: load_component, set_props (Option B)
 *
 * C owns a rectangular slot; Vue mounts via createApp per slot.
 * nv_vue_load_component injects a Vue component into the slot.
 * nv_vue_set_props merges new props into the slot's reactive props object.
 *
 * Apache 2.0 - See LICENSE for details
 * ============================================================================= */

#include "nv_ui.h"
#include "nv_str.h"
#include "nv_arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Escape str for embedding inside a JS double-quoted string literal. */
static void append_js_escaped(nv_str_t* s, const char* str) {
  if (!s || !str) return;
  for (; *str; str++) {
    switch (*str) {
      case '\\': nv_str_append(s, "\\\\"); break;
      case '"':  nv_str_append(s, "\\\""); break;
      case '\n': nv_str_append(s, "\\n");  break;
      case '\r': nv_str_append(s, "\\r");  break;
      case '\t': nv_str_append(s, "\\t");  break;
      default:   nv_str_append_len(s, str, 1); break;
    }
  }
}

NV_API void nv_vue_load_component(nv_vue_t* slot, const char* js_fn_body) {
  if (!slot || slot->type != NV_COMP_VUE || !js_fn_body) return;
  nv_form_t* form = nv_component_get_form((nv_component_t*)slot);
  if (!form || !form->state.form.window || !form->state.form.initialized)
    return;

  nv_arena_t* arena = nv_arena_create_pool_growable(4096, 4);
  if (!arena) return;

  nv_str_t* buf = nv_str_create(arena);
  if (!buf) { nv_arena_destroy(arena); return; }

  nv_str_append(buf, "window.__nv_load_vue_component(\"");
  append_js_escaped(buf, slot->name);
  nv_str_append(buf, "\",\"");
  append_js_escaped(buf, js_fn_body);
  nv_str_append(buf, "\",\"");
  append_js_escaped(buf, slot->state.vue.props_json ? slot->state.vue.props_json : "{}");
  nv_str_append(buf, "\")");

  const char* js = nv_str_cstr(buf);
  if (js) {
    size_t len = strlen(js);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
      memcpy(copy, js, len + 1);
      nv_eval_js(form->state.form.window, copy);
      free(copy);
    }
  }
  nv_arena_destroy(arena);
}

NV_API void nv_vue_set_props(nv_vue_t* slot, const char* props_json) {
  if (!slot || slot->type != NV_COMP_VUE || !props_json) return;
  nv_form_t* form = nv_component_get_form((nv_component_t*)slot);
  if (!form || !form->state.form.window) return;

  nv_arena_t* arena = nv_arena_create_pool_growable(2048, 2);
  if (!arena) return;

  nv_str_t* buf = nv_str_create(arena);
  if (!buf) { nv_arena_destroy(arena); return; }

  nv_str_append(buf, "window.__nv_set_vue_props(\"");
  append_js_escaped(buf, slot->name);
  nv_str_append(buf, "\",\"");
  append_js_escaped(buf, props_json);
  nv_str_append(buf, "\")");

  const char* js = nv_str_cstr(buf);
  if (js) {
    size_t len = strlen(js);
    char* copy = (char*)malloc(len + 1);
    if (copy) {
      memcpy(copy, js, len + 1);
      nv_eval_js(form->state.form.window, copy);
      free(copy);
    }
  }
  nv_arena_destroy(arena);
}
