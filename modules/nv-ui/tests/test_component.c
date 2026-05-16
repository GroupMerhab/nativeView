/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/*
 * Tests for composite component tree, events, and all widget types.
 *
 * Uses the public creation API (nv_form_new, nv_button_new, etc.) so that
 * arena allocation is exercised correctly.
 */

#include "nv_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_count;
static int test_passed;

static int event_fired;
static void* event_ctx_seen;

static void on_click_cb(nv_component_t* c, void* ctx) {
  event_fired = 1;
  event_ctx_seen = ctx;
  (void)c;
}

#define OK(name) do { test_count++; test_passed++; printf("OK %s\n", name); } while(0)
#define FAIL(name, reason) do { test_count++; printf("FAIL %s: %s\n", name, reason); } while(0)
#define ASSERT(name, cond) do { if (cond) OK(name); else FAIL(name, #cond " is false"); } while(0)

int main(void) {
  test_count = 0;
  test_passed = 0;

  /* ---- nv_component_init / tree wiring ---- */
  {
    /* Use a raw arena-backed form to test init directly */
    nv_form_cfg_t fcfg = { .title = "t", .width = 400, .height = 300 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    ASSERT("form_new_not_null", form != NULL);
    ASSERT("form_type", form->type == NV_COMP_FORM);
    ASSERT("form_width", form->state.form.width == 400);
    ASSERT("form_height", form->state.form.height == 300);

    /* Button */
    nv_button_t* btn = nv_button_new((nv_component_t*)form,
      &(nv_button_cfg_t){ .name = "b1", .caption = "Click", .enabled = 1,
                          .x = 10, .y = 20, .w = 80, .h = 28 });
    ASSERT("button_new_not_null", btn != NULL);
    ASSERT("button_type", btn->type == NV_COMP_BUTTON);
    ASSERT("button_caption", strcmp(btn->state.button.caption, "Click") == 0);
    ASSERT("button_enabled", btn->state.button.enabled == 1);
    ASSERT("button_x", btn->state.button.x == 10);
    ASSERT("button_y", btn->state.button.y == 20);
    ASSERT("button_w", btn->state.button.w == 80);
    ASSERT("button_h", btn->state.button.h == 28);
    ASSERT("button_in_tree", form->owned_head == (nv_component_t*)btn);

    /* Label */
    nv_label_t* lbl = nv_label_new((nv_component_t*)form,
      &(nv_label_cfg_t){ .name = "lbl", .text = "Hello",
                         .x = 0, .y = 0, .w = 200, .h = 24 });
    ASSERT("label_new_not_null", lbl != NULL);
    ASSERT("label_type", lbl->type == NV_COMP_LABEL);
    ASSERT("label_text", strcmp(lbl->state.label.text, "Hello") == 0);
    ASSERT("label_w", lbl->state.label.w == 200);

    /* Input */
    nv_input_t* inp = nv_input_new((nv_component_t*)form,
      &(nv_input_cfg_t){ .name = "inp", .text = "val", .placeholder = "ph",
                         .x = 0, .y = 60, .w = 200, .h = 28 });
    ASSERT("input_new_not_null", inp != NULL);
    ASSERT("input_text", strcmp(inp->state.input.text, "val") == 0);
    ASSERT("input_placeholder", strcmp(inp->state.input.placeholder, "ph") == 0);
    ASSERT("input_y", inp->state.input.y == 60);

    /* Panel */
    nv_panel_t* pan = nv_panel_new((nv_component_t*)form,
      &(nv_panel_cfg_t){ .name = "pan", .x = 5, .y = 5, .w = 300, .h = 200 });
    ASSERT("panel_new_not_null", pan != NULL);
    ASSERT("panel_x", pan->state.panel.x == 5);
    ASSERT("panel_w", pan->state.panel.w == 300);

    /* Checkbox */
    nv_checkbox_t* chk = nv_checkbox_new((nv_component_t*)form,
      &(nv_checkbox_cfg_t){ .name = "chk", .label = "Enable", .checked = 1,
                            .x = 0, .y = 100, .w = 160, .h = 24 });
    ASSERT("checkbox_new_not_null", chk != NULL);
    ASSERT("checkbox_type", chk->type == NV_COMP_CHECKBOX);
    ASSERT("checkbox_checked", chk->state.checkbox.checked == 1);
    ASSERT("checkbox_label", strcmp(chk->state.checkbox.label, "Enable") == 0);
    ASSERT("checkbox_y", chk->state.checkbox.y == 100);

    /* Select */
    const char* items[] = { "Apple", "Banana", "Cherry" };
    nv_select_t* sel = nv_select_new((nv_component_t*)form,
      &(nv_select_cfg_t){ .name = "sel", .items = items, .item_count = 3,
                          .selected_index = 1, .x = 0, .y = 130, .w = 200, .h = 28 });
    ASSERT("select_new_not_null", sel != NULL);
    ASSERT("select_type", sel->type == NV_COMP_SELECT);
    ASSERT("select_item_count", sel->state.select_.item_count == 3);
    ASSERT("select_selected", sel->state.select_.selected_index == 1);
    ASSERT("select_items_ptr", sel->state.select_.items == items);

    /* Slider */
    nv_slider_t* sld = nv_slider_new((nv_component_t*)form,
      &(nv_slider_cfg_t){ .name = "sld", .value = 50.0f, .min = 0.0f, .max = 100.0f,
                          .step = 1.0f, .x = 0, .y = 170, .w = 200, .h = 24 });
    ASSERT("slider_new_not_null", sld != NULL);
    ASSERT("slider_type", sld->type == NV_COMP_SLIDER);
    ASSERT("slider_value", sld->state.slider.value == 50.0f);
    ASSERT("slider_min", sld->state.slider.min == 0.0f);
    ASSERT("slider_max", sld->state.slider.max == 100.0f);
    ASSERT("slider_step", sld->state.slider.step == 1.0f);

    /* Textarea */
    nv_textarea_t* ta = nv_textarea_new((nv_component_t*)form,
      &(nv_textarea_cfg_t){ .name = "ta", .text = "hello\nworld", .rows = 4,
                            .x = 0, .y = 200, .w = 300, .h = 80 });
    ASSERT("textarea_new_not_null", ta != NULL);
    ASSERT("textarea_type", ta->type == NV_COMP_TEXTAREA);
    ASSERT("textarea_text", strcmp(ta->state.textarea.text, "hello\nworld") == 0);
    ASSERT("textarea_rows", ta->state.textarea.rows == 4);
    ASSERT("textarea_w", ta->state.textarea.w == 300);

    /* Vue slot */
    nv_vue_t* vue = nv_vue_new((nv_component_t*)form,
      &(nv_vue_cfg_t){ .name = "vue1", .component_name = "demo",
                       .props_json = "{\"x\":1}", .x = 0, .y = 230, .w = 300, .h = 100 });
    ASSERT("vue_new_not_null", vue != NULL);
    ASSERT("vue_type", vue->type == NV_COMP_VUE);
    ASSERT("vue_props_json", strcmp(vue->state.vue.props_json, "{\"x\":1}") == 0);
    ASSERT("vue_w", vue->state.vue.w == 300);

    /* find_by_name */
    nv_component_t* found = nv_component_find_by_name((nv_component_t*)form, "chk");
    ASSERT("find_checkbox", found == (nv_component_t*)chk);
    found = nv_component_find_by_name((nv_component_t*)form, "sld");
    ASSERT("find_slider", found == (nv_component_t*)sld);
    found = nv_component_find_by_name((nv_component_t*)form, "vue1");
    ASSERT("find_vue", found == (nv_component_t*)vue);
    found = nv_component_find_by_name((nv_component_t*)form, "notexist");
    ASSERT("find_missing", found == NULL);

    /* Destroy form: frees arena (all children) in one shot */
    nv_component_destroy((nv_component_t*)form);
    OK("destroy_form_no_crash");
  }

  /* ---- Event bind / emit ---- */
  {
    nv_form_cfg_t fcfg = { .title = "evtest", .width = 200, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_button_t* btn = nv_button_new((nv_component_t*)form,
      &(nv_button_cfg_t){ .name = "ev_btn", .caption = "Go", .enabled = 1 });

    event_fired = 0;
    event_ctx_seen = NULL;
    nv_component_bind_event((nv_component_t*)btn, NV_EVENT_CLICK, on_click_cb, (void*)0xABCD);
    nv_component_emit_event((nv_component_t*)btn, NV_EVENT_CLICK);
    ASSERT("emit_click_fired", event_fired == 1);
    ASSERT("emit_click_ctx", event_ctx_seen == (void*)0xABCD);

    /* Emit unbound slot: no crash */
    nv_component_emit_event((nv_component_t*)btn, NV_EVENT_INPUT);
    OK("emit_unbound_no_crash");

    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- nv_component_get_form ---- */
  {
    nv_form_cfg_t fcfg = { .title = "gf", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_label_t* lbl = nv_label_new((nv_component_t*)form,
      &(nv_label_cfg_t){ .name = "gl", .text = "x" });
    nv_form_t* f = nv_component_get_form((nv_component_t*)lbl);
    ASSERT("get_form", f == form);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- schedule_diff marks form dirty ---- */
  {
    nv_form_cfg_t fcfg = { .title = "dirty", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_label_t* lbl = nv_label_new((nv_component_t*)form,
      &(nv_label_cfg_t){ .name = "dl", .text = "x" });
    ASSERT("not_dirty_initially", form->state.form.dirty == 0);
    nv_ui_schedule_diff((nv_component_t*)lbl);
    ASSERT("dirty_after_schedule", form->state.form.dirty == 1);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- NV_SET_STATE updates field and marks dirty ---- */
  {
    nv_form_cfg_t fcfg = { .title = "set", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_label_t* lbl = nv_label_new((nv_component_t*)form,
      &(nv_label_cfg_t){ .name = "sl", .text = "before" });
    NV_SET_STATE(lbl, state.label.text, "after");
    ASSERT("set_state_value", strcmp(lbl->state.label.text, "after") == 0);
    ASSERT("set_state_dirty", form->state.form.dirty == 1);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- find_by_name with nested children (panel -> label) ---- */
  {
    nv_form_cfg_t fcfg = { .title = "nested", .width = 200, .height = 150 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_panel_t* pan = nv_panel_new((nv_component_t*)form,
      &(nv_panel_cfg_t){ .name = "p1", .x = 0, .y = 0, .w = 200, .h = 100 });
    nv_label_t* lbl = nv_label_new((nv_component_t*)pan,
      &(nv_label_cfg_t){ .name = "inner", .text = "nested" });
    nv_component_t* found = nv_component_find_by_name((nv_component_t*)form, "inner");
    ASSERT("find_nested", found == (nv_component_t*)lbl);
    found = nv_component_find_by_name((nv_component_t*)form, "p1");
    ASSERT("find_panel", found == (nv_component_t*)pan);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- get_form from nested component (child of panel) ---- */
  {
    nv_form_cfg_t fcfg = { .title = "gf2", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_panel_t* pan = nv_panel_new((nv_component_t*)form,
      &(nv_panel_cfg_t){ .name = "p2", .x = 0, .y = 0, .w = 50, .h = 50 });
    nv_button_t* btn = nv_button_new((nv_component_t*)pan,
      &(nv_button_cfg_t){ .name = "b2", .caption = "X", .enabled = 1 });
    nv_form_t* f = nv_component_get_form((nv_component_t*)btn);
    ASSERT("get_form_nested", f == form);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- nv_component_remove unlinks child ---- */
  {
    nv_form_cfg_t fcfg = { .title = "rm", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_panel_t* pan = nv_panel_new((nv_component_t*)form,
      &(nv_panel_cfg_t){ .name = "pan", .x = 0, .y = 0, .w = 50, .h = 50 });
    nv_button_t* btn = nv_button_new((nv_component_t*)pan,
      &(nv_button_cfg_t){ .name = "btn", .caption = "X", .enabled = 1 });
    ASSERT("before_remove", form->owned_head == (nv_component_t*)pan);
    ASSERT("pan_owns_btn", pan->owned_head == (nv_component_t*)btn);
    nv_component_remove((nv_component_t*)pan, (nv_component_t*)btn);
    ASSERT("after_remove", pan->owned_head == NULL);
    ASSERT("btn_unlinked", btn->owner == NULL);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- nv_component_resize updates form and panel ---- */
  {
    nv_form_cfg_t fcfg = { .title = "resize", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_panel_t* pan = nv_panel_new((nv_component_t*)form,
      &(nv_panel_cfg_t){ .name = "r1", .x = 0, .y = 0, .w = 50, .h = 50 });
    nv_component_resize((nv_component_t*)form, 800, 600);
    nv_component_resize((nv_component_t*)pan, 200, 150);
    ASSERT("resize_form", form->state.form.width == 800 && form->state.form.height == 600);
    ASSERT("resize_panel", pan->state.panel.w == 200 && pan->state.panel.h == 150);
    nv_component_destroy((nv_component_t*)form);
  }

  printf("%d/%d passed\n", test_passed, test_count);
  return test_passed == test_count ? 0 : 1;
}
