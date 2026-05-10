/*
 * Tests for nv_component_paint, nv_ui_flush (no-op when no window),
 * and nv_ui_schedule_diff behavior.
 */

#include "nv_ui.h"
#include "nv_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_count;
static int test_passed;

#define OK(name)   do { test_count++; test_passed++; printf("OK %s\n", name); } while(0)
#define FAIL(n,r)  do { test_count++; printf("FAIL %s: %s\n", n, r); } while(0)
#define ASSERT(n,c) do { if (c) OK(n); else FAIL(n, #c " is false"); } while(0)

static int contains(const char* hay, const char* needle) {
  return hay && needle && strstr(hay, needle) != NULL;
}

int main(void) {
  test_count = 0;
  test_passed = 0;

  /* ---- nv_component_paint produces expected HTML ---- */
  {
    nv_form_cfg_t fcfg = { .title = "paint", .width = 200, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_label_new((nv_component_t*)form,
      &(nv_label_cfg_t){ .name = "l1", .text = "Hello" });
    nv_button_new((nv_component_t*)form,
      &(nv_button_cfg_t){ .name = "b1", .caption = "Click", .enabled = 1 });

    nv_arena_t* arena = nv_arena_create_pool_growable(4096, 4);
    ASSERT("arena_for_paint", arena != NULL);

    nv_canvas_t* canvas = nv_canvas_create_for_test(arena);
    ASSERT("canvas_create", canvas != NULL);

    nv_component_paint((nv_component_t*)form, canvas);
    const char* html = nv_canvas_get_html(canvas);
    ASSERT("paint_html_not_null", html != NULL);
    ASSERT("paint_has_doctype", contains(html, "<!DOCTYPE html>"));
    ASSERT("paint_has_nv_root", contains(html, "nv-root"));
    ASSERT("paint_has_label_hello", contains(html, "Hello"));
    ASSERT("paint_has_button_click", contains(html, "Click"));
    ASSERT("paint_has_nv_button", contains(html, "nv-button"));
    ASSERT("paint_has_nv_label", contains(html, "nv-label"));

    nv_component_destroy((nv_component_t*)form);
    nv_arena_destroy(arena);
  }

  /* ---- nv_ui_flush no-op when form has no window ---- */
  {
    nv_form_cfg_t fcfg = { .title = "nowin", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_label_new((nv_component_t*)form,
      &(nv_label_cfg_t){ .name = "l2", .text = "x" });
    form->state.form.dirty = 1;
    nv_ui_flush(form);  /* should return early, no crash */
    ASSERT("flush_no_window_no_crash", 1);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- nv_ui_flush no-op when form not dirty ---- */
  {
    nv_form_cfg_t fcfg = { .title = "nodirty", .width = 100, .height = 100 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    form->state.form.dirty = 0;
    nv_ui_flush(form);
    ASSERT("flush_not_dirty_no_crash", 1);
    nv_component_destroy((nv_component_t*)form);
  }

  /* ---- nv_ui_flush NULL form is safe ---- */
  {
    nv_ui_flush(NULL);
    OK("flush_null_safe");
  }

  /* ---- schedule_diff on NULL is safe ---- */
  {
    nv_ui_schedule_diff(NULL);
    OK("schedule_diff_null_safe");
  }

  /* ---- paint panel with nested label ---- */
  {
    nv_form_cfg_t fcfg = { .title = "panel", .width = 200, .height = 150 };
    nv_form_t* form = nv_form_new(NULL, &fcfg);
    nv_panel_t* pan = nv_panel_new((nv_component_t*)form,
      &(nv_panel_cfg_t){ .name = "p1", .x = 10, .y = 20, .w = 100, .h = 80 });
    nv_label_new((nv_component_t*)pan,
      &(nv_label_cfg_t){ .name = "inner", .text = "nested" });

    nv_arena_t* arena = nv_arena_create_pool_growable(4096, 4);
    nv_canvas_t* canvas = nv_canvas_create_for_test(arena);
    nv_component_paint((nv_component_t*)form, canvas);
    const char* html = nv_canvas_get_html(canvas);
    ASSERT("paint_panel_geometry", contains(html, "left:10px"));
    ASSERT("paint_panel_id", contains(html, "nv-panel"));
    ASSERT("paint_nested_label", contains(html, "nested"));

    nv_component_destroy((nv_component_t*)form);
    nv_arena_destroy(arena);
  }

  printf("%d/%d passed\n", test_passed, test_count);
  return test_passed == test_count ? 0 : 1;
}
