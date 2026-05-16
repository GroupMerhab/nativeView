/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_widget_flush.c - nv_ui_flush, event router, nv_form_set_window
 *
 * Owns the main flush entry point and the C-side event router that receives
 * "nv.event" messages from the JS bridge and dispatches to C handlers.
 *
 * Apache 2.0 - See LICENSE for details
 * ============================================================================= */

#include "nv_ui.h"
#include "nv_json.h"
#include "nv_arena.h"
#include "nv_str.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations from nv_widget.c (same translation unit group) */
char* nv_widget_build_vue_scaffold(nv_form_t* form);
char* nv_widget_build_patch_js(nv_form_t* form);
void  nv_widget_diff_comp_init(nv_str_t* s, nv_component_t* comp,
                                nv_form_snap_t* snap, int* first);

/* ============================================================================
 * nv_ui_flush — main entry point called after NV_SET_STATE
 * ========================================================================== */

NV_INTERNAL void nv_ui_flush(nv_form_t* form) {
  if (!form || form->type != NV_COMP_FORM) return;
  if (!form->state.form.dirty || !form->state.form.window) return;
  form->state.form.dirty = 0;

  nv_window_t* window = form->state.form.window;

  if (!form->state.form.initialized) {
    /* First flush: allocate snapshot table in form arena, load Vue scaffold */
    if (!form->state.form.snap) {
      form->state.form.snap =
        (nv_form_snap_t*)nv_arena_alloc(form->arena, sizeof(nv_form_snap_t));
      if (form->state.form.snap)
        memset(form->state.form.snap, 0, sizeof(nv_form_snap_t));
    }

    char* html = nv_widget_build_vue_scaffold(form);
    if (html) {
      /* Local Vue is embedded; use about:blank as base (no external scripts) */
      nv_load_html(window, html, "about:blank");
      free(html);
    }

    /* Populate snapshot with initial state so first patch diff is clean */
    if (form->state.form.snap) {
      nv_arena_t* tmp = nv_arena_create_pool_growable(4096, 4);
      if (tmp) {
        nv_str_t* dummy = nv_str_create(tmp);
        if (dummy) {
          int f = 1;
          nv_widget_diff_comp_init(dummy, (nv_component_t*)form,
                                   form->state.form.snap, &f);
        }
        nv_arena_destroy(tmp);
      }
    }

    form->state.form.initialized = 1;
  } else {
    /* Subsequent flushes: send only changed fields as a JS patch */
    char* js = nv_widget_build_patch_js(form);
    if (js) {
      nv_eval_js(window, js);
      free(js);
    }
  }
}

/* ============================================================================
 * C event router — receives "nv.event" messages from the JS bridge
 * ========================================================================== */

static int event_type_to_slot(const char* type) {
  if (!type) return -1;
  if (strcmp(type, "click")  == 0) return NV_EVENT_CLICK;
  if (strcmp(type, "input")  == 0) return NV_EVENT_INPUT;
  if (strcmp(type, "change") == 0) return NV_EVENT_CHANGE;
  return -1;
}

static void nv_ui_on_event(nv_window_t* w, const char* event,
                            const char* json, void* userdata) {
  (void)w;
  if (!event || strcmp(event, "nv.event") != 0) return;
  nv_form_t* form = (nv_form_t*)userdata;
  if (!form || !json) return;

  nv_arena_t* scratch = nv_arena_create_pool_growable(1024, 2);
  if (!scratch) return;

  nv_json_val_t* obj = nv_json_parse(scratch, json);
  if (!obj || !nv_json_is_object(obj)) { nv_arena_destroy(scratch); return; }

  const char* id   = nv_json_get_str(obj, "id");
  const char* type = nv_json_get_str(obj, "type");
  int slot = event_type_to_slot(type);

  if (id && slot >= 0) {
    nv_component_t* comp = nv_component_find_by_name((nv_component_t*)form, id);
    if (comp) {
      switch (comp->type) {
        case NV_COMP_INPUT:
          if (slot == NV_EVENT_INPUT || slot == NV_EVENT_CHANGE) {
            const char* value = nv_json_get_str(obj, "value");
            if (value && form->arena) {
              size_t len = strlen(value) + 1;
              char* copy = (char*)nv_arena_alloc(form->arena, len);
              if (copy) { memcpy(copy, value, len); comp->state.input.text = copy; }
            }
          }
          break;
        case NV_COMP_TEXTAREA:
          if (slot == NV_EVENT_INPUT || slot == NV_EVENT_CHANGE) {
            const char* value = nv_json_get_str(obj, "value");
            if (value && form->arena) {
              size_t len = strlen(value) + 1;
              char* copy = (char*)nv_arena_alloc(form->arena, len);
              if (copy) { memcpy(copy, value, len); comp->state.textarea.text = copy; }
            }
          }
          break;
        case NV_COMP_CHECKBOX:
          if (slot == NV_EVENT_CLICK)
            comp->state.checkbox.checked = nv_json_get_bool(obj, "value");
          break;
        case NV_COMP_SELECT:
          if (slot == NV_EVENT_CHANGE)
            comp->state.select_.selected_index = (int)nv_json_get_int(obj, "value");
          break;
        case NV_COMP_SLIDER:
          if (slot == NV_EVENT_INPUT)
            comp->state.slider.value = (float)nv_json_get_double(obj, "value");
          break;
        default:
          break;
      }
      nv_component_emit_event(comp, slot);
    }
  }

  nv_arena_destroy(scratch);
}

/* ============================================================================
 * nv_form_set_window
 * ========================================================================== */

NV_API void nv_form_set_window(nv_form_t* form, nv_window_t* window) {
  if (!form || form->type != NV_COMP_FORM) return;
  form->state.form.window = window;
  if (window)
    nv_on_message(window, nv_ui_on_event, form);
}
