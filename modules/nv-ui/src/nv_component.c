/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_component.c - Composite component: type-based dispatch, owner/parent tree
 *
 * Apache 2.0 - See LICENSE for details
 * ============================================================================= */

#include "nv_ui.h"
#include <stdlib.h>
#include <string.h>

#define NV_NAME_MAX (NV_OBJECT_NAME_MAX - 1)

/* nv_ui_schedule_diff and nv_component_paint are in nv_widget.c */

NV_INTERNAL void nv_component_init(nv_component_t* comp, nv_component_type_t type,
                                   nv_component_t* owner, nv_component_t* parent,
                                   nv_arena_t* arena, const char* name) {
  if (!comp) return;
  comp->type   = type;
  comp->arena  = arena;
  if (name) {
    size_t n = strlen(name);
    if (n > NV_NAME_MAX) n = NV_NAME_MAX;
    memcpy(comp->name, name, n);
    comp->name[n] = '\0';
  } else {
    comp->name[0] = '\0';
  }
  comp->owner       = owner;
  comp->parent      = parent;
  comp->owned_head  = NULL;
  comp->owned_next  = NULL;
  comp->event_count = 0;
  for (int i = 0; i < NV_MAX_EVENTS; i++) {
    comp->events[i]   = NULL;
    comp->event_ctx[i] = NULL;
  }
  memset(&comp->state, 0, sizeof(comp->state));
  if (owner) {
    comp->owned_next   = owner->owned_head;
    owner->owned_head  = comp;
  }
}

NV_INTERNAL void nv_component_add_owned(nv_component_t* owner, nv_component_t* child) {
  if (!owner || !child) return;
  child->owner = owner;
  child->owned_next = owner->owned_head;
  owner->owned_head = child;
}

NV_API void nv_component_remove(nv_component_t* owner, nv_component_t* child) {
  if (!owner || !child) return;
  if (owner->owned_head == child) {
    owner->owned_head = child->owned_next;
    child->owned_next = NULL;
    child->owner = NULL;
    return;
  }
  for (nv_component_t* p = owner->owned_head; p && p->owned_next; p = p->owned_next) {
    if (p->owned_next == child) {
      p->owned_next = child->owned_next;
      child->owned_next = NULL;
      child->owner = NULL;
      return;
    }
  }
}

NV_INTERNAL void nv_component_destroy(nv_component_t* comp) {
  if (!comp) return;
  if (comp->type == NV_COMP_FORM) {
    /* Destroying the form's arena frees all child components in one shot.
     * No need to walk the tree — arena owns all child memory. */
    if (comp->arena)
      nv_arena_destroy(comp->arena);
    free(comp);   /* form itself was malloc'd in nv_form_new */
  }
  /* Child components are arena-allocated; do not free them individually. */
}

/* Stub: resize updates type-specific state when needed. */
NV_INTERNAL void nv_component_resize(nv_component_t* comp, int w, int h) {
  if (!comp) return;
  (void)w;
  (void)h;
  switch (comp->type) {
    case NV_COMP_FORM:
      comp->state.form.width  = w;
      comp->state.form.height = h;
      break;
    case NV_COMP_PANEL:
      comp->state.panel.w = w;
      comp->state.panel.h = h;
      break;
    default:
      break;
  }
}

NV_INTERNAL void nv_component_bind_event(nv_component_t* comp, int slot,
                                          nv_event_fn_t fn, void* ctx) {
  if (!comp || slot < 0 || slot >= NV_MAX_EVENTS) return;
  comp->events[slot]   = fn;
  comp->event_ctx[slot] = ctx;
  if (slot >= comp->event_count)
    comp->event_count = slot + 1;
}

NV_INTERNAL void nv_component_emit_event(nv_component_t* comp, int slot) {
  if (!comp || slot < 0 || slot >= NV_MAX_EVENTS) return;
  if (comp->events[slot])
    comp->events[slot](comp, comp->event_ctx[slot]);
}

NV_INTERNAL nv_component_t* nv_component_find_by_name(nv_component_t* root,
                                                        const char* name) {
  if (!root || !name) return NULL;
  if (root->name[0] && strcmp(root->name, name) == 0) return root;
  for (nv_component_t* c = root->owned_head; c; c = c->owned_next) {
    nv_component_t* found = nv_component_find_by_name(c, name);
    if (found) return found;
  }
  return NULL;
}

NV_INTERNAL nv_form_t* nv_component_get_form(nv_component_t* comp) {
  while (comp) {
    if (comp->type == NV_COMP_FORM) return (nv_form_t*)comp;
    comp = comp->owner;
  }
  return NULL;
}

/* -----------------------------------------------------------------------------
 * Creation API
 * ----------------------------------------------------------------------------- */
NV_API nv_form_t* nv_form_new(nv_app_t* app, const nv_form_cfg_t* cfg) {
  (void)app;
  if (!cfg) return NULL;
  nv_component_t* comp = (nv_component_t*)malloc(sizeof(nv_component_t));
  if (!comp) return NULL;
  nv_arena_t* arena = nv_arena_create_pool_growable(4096, 16);
  if (!arena) { free(comp); return NULL; }
  nv_component_init(comp, NV_COMP_FORM, NULL, NULL, arena,
                    cfg->title ? cfg->title : "form");
  comp->state.form.window = NULL;
  comp->state.form.width  = cfg->width > 0  ? cfg->width  : 800;
  comp->state.form.height = cfg->height > 0 ? cfg->height : 600;
  comp->state.form.dirty  = 0;
  return (nv_form_t*)comp;
}

/* nv_form_set_window is in nv_widget.c (needs nv_on_message) */

/*
 * Allocate a child component from the owner's arena.
 * Arena allocation is O(1) and avoids per-component malloc overhead.
 * The form's arena owns the memory; nv_component_destroy must not free it.
 */
static nv_component_t* comp_new(nv_component_type_t type, nv_component_t* owner,
                                const char* name, nv_arena_t* arena) {
  if (!owner) return NULL;
  nv_arena_t* a = arena ? arena : owner->arena;
  if (!a) return NULL;
  nv_component_t* comp = (nv_component_t*)nv_arena_alloc(a, sizeof(nv_component_t));
  if (!comp) return NULL;
  nv_component_init(comp, type, owner, owner, a, name);
  return comp;
}

NV_API nv_button_t* nv_button_new(nv_component_t* owner, const nv_button_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_BUTTON, owner, cfg->name ? cfg->name : "button", NULL);
  if (!comp) return NULL;
  comp->state.button.caption = cfg->caption ? cfg->caption : "";
  comp->state.button.enabled = cfg->enabled ? 1 : 0;
  comp->state.button.x = cfg->x;
  comp->state.button.y = cfg->y;
  comp->state.button.w = cfg->w > 0 ? cfg->w : 80;
  comp->state.button.h = cfg->h > 0 ? cfg->h : 28;
  return (nv_button_t*)comp;
}

NV_API nv_label_t* nv_label_new(nv_component_t* owner, const nv_label_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_LABEL, owner, cfg->name ? cfg->name : "label", NULL);
  if (!comp) return NULL;
  comp->state.label.text = cfg->text ? cfg->text : "";
  comp->state.label.x = cfg->x;
  comp->state.label.y = cfg->y;
  comp->state.label.w = cfg->w > 0 ? cfg->w : 200;
  comp->state.label.h = cfg->h > 0 ? cfg->h : 24;
  return (nv_label_t*)comp;
}

NV_API nv_input_t* nv_input_new(nv_component_t* owner, const nv_input_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_INPUT, owner, cfg->name ? cfg->name : "input", NULL);
  if (!comp) return NULL;
  comp->state.input.text        = cfg->text ? cfg->text : "";
  comp->state.input.placeholder = cfg->placeholder ? cfg->placeholder : "";
  comp->state.input.x = cfg->x;
  comp->state.input.y = cfg->y;
  comp->state.input.w = cfg->w > 0 ? cfg->w : 200;
  comp->state.input.h = cfg->h > 0 ? cfg->h : 28;
  return (nv_input_t*)comp;
}

NV_API nv_panel_t* nv_panel_new(nv_component_t* owner, const nv_panel_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_PANEL, owner, cfg->name ? cfg->name : "panel", NULL);
  if (!comp) return NULL;
  comp->state.panel.x = cfg->x;
  comp->state.panel.y = cfg->y;
  comp->state.panel.w = cfg->w > 0 ? cfg->w : 100;
  comp->state.panel.h = cfg->h > 0 ? cfg->h : 100;
  return (nv_panel_t*)comp;
}

NV_API nv_checkbox_t* nv_checkbox_new(nv_component_t* owner, const nv_checkbox_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_CHECKBOX, owner,
                                   cfg->name ? cfg->name : "checkbox", NULL);
  if (!comp) return NULL;
  comp->state.checkbox.checked = cfg->checked ? 1 : 0;
  comp->state.checkbox.label   = cfg->label ? cfg->label : "";
  comp->state.checkbox.x = cfg->x;
  comp->state.checkbox.y = cfg->y;
  comp->state.checkbox.w = cfg->w > 0 ? cfg->w : 160;
  comp->state.checkbox.h = cfg->h > 0 ? cfg->h : 24;
  return (nv_checkbox_t*)comp;
}

NV_API nv_select_t* nv_select_new(nv_component_t* owner, const nv_select_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_SELECT, owner,
                                   cfg->name ? cfg->name : "select", NULL);
  if (!comp) return NULL;
  comp->state.select_.items          = cfg->items;
  comp->state.select_.item_count     = cfg->item_count > 0 ? cfg->item_count : 0;
  comp->state.select_.selected_index = cfg->selected_index;
  comp->state.select_.x = cfg->x;
  comp->state.select_.y = cfg->y;
  comp->state.select_.w = cfg->w > 0 ? cfg->w : 200;
  comp->state.select_.h = cfg->h > 0 ? cfg->h : 28;
  return (nv_select_t*)comp;
}

NV_API nv_slider_t* nv_slider_new(nv_component_t* owner, const nv_slider_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_SLIDER, owner,
                                   cfg->name ? cfg->name : "slider", NULL);
  if (!comp) return NULL;
  comp->state.slider.value = cfg->value;
  comp->state.slider.min   = cfg->min;
  comp->state.slider.max   = cfg->max > cfg->min ? cfg->max : cfg->min + 100.0f;
  comp->state.slider.step  = cfg->step > 0.0f ? cfg->step : 1.0f;
  comp->state.slider.x = cfg->x;
  comp->state.slider.y = cfg->y;
  comp->state.slider.w = cfg->w > 0 ? cfg->w : 200;
  comp->state.slider.h = cfg->h > 0 ? cfg->h : 24;
  comp->state.slider.shm = NULL;
  comp->state.slider.shm_offset = 0;
  return (nv_slider_t*)comp;
}

NV_API void nv_slider_attach_shm(nv_slider_t* slider, void* shm) {
  if (!slider || slider->type != NV_COMP_SLIDER) return;
  slider->state.slider.shm = shm;
  slider->state.slider.shm_offset = 0;  /* value at byte 0 */
}

NV_API nv_textarea_t* nv_textarea_new(nv_component_t* owner, const nv_textarea_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_TEXTAREA, owner,
                                   cfg->name ? cfg->name : "textarea", NULL);
  if (!comp) return NULL;
  comp->state.textarea.text = cfg->text ? cfg->text : "";
  comp->state.textarea.rows = cfg->rows > 0 ? cfg->rows : 4;
  comp->state.textarea.x = cfg->x;
  comp->state.textarea.y = cfg->y;
  comp->state.textarea.w = cfg->w > 0 ? cfg->w : 300;
  comp->state.textarea.h = cfg->h > 0 ? cfg->h : 80;
  return (nv_textarea_t*)comp;
}

NV_API nv_vue_t* nv_vue_new(nv_component_t* owner, const nv_vue_cfg_t* cfg) {
  if (!owner || !cfg) return NULL;
  nv_component_t* comp = comp_new(NV_COMP_VUE, owner,
                                   cfg->name ? cfg->name : "vue", NULL);
  if (!comp) return NULL;
  comp->state.vue.component_name = cfg->component_name ? cfg->component_name : "";
  comp->state.vue.props_json    = cfg->props_json ? cfg->props_json : "{}";
  comp->state.vue.x = cfg->x;
  comp->state.vue.y = cfg->y;
  comp->state.vue.w = cfg->w > 0 ? cfg->w : 300;
  comp->state.vue.h = cfg->h > 0 ? cfg->h : 200;
  return (nv_vue_t*)comp;
}
