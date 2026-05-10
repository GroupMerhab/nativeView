/* =============================================================================
 * test_op_window.c - Window Ops Handler Tests
 * =============================================================================
 */

#include "nv_op_window.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name){ printf("✓ %s\n", name); tests_passed++; }
static void fail(const char* name, const char* why){ printf("✗ %s: %s\n", name, why); tests_failed++; }

static nv_window_t* make_window(void) {
  nv_window_cfg_t cfg = {
    .title = "OpsTest",
    .width = 640,
    .height = 480,
    .min_width = 0,
    .min_height = 0,
    .resizable = 1,
    .frameless = 0,
    .transparent = 0,
    .devtools = 0
  };
  return nv_window_alloc(NULL, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s){
  return nv_json_parse(arena, s ? s : "null");
}

static void test_valid_and_missing_args(void) {
  nv_window_t* win = make_window();
  if (!win) { fail("make_window", "nv_window_alloc failed"); return; }
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) { fail("arena", "nv_arena_create failed"); nv_window_free(win); return; }

  /* set_title ok */
  test_reset_replies();
  nv_json_val_t* a1 = parse(arena, "{\"title\":\"Hello\"}");
  nv_op_window_set_title(win, 1, a1, arena);
  if (!test_last_reply_ok()) { fail("set_title ok", "expected ok"); } else ok("set_title ok");

  /* set_title missing */
  test_reset_replies();
  nv_json_val_t* a1m = parse(arena, "{}");
  nv_op_window_set_title(win, 2, a1m, arena);
  if (test_last_reply_ok()) { fail("set_title missing", "expected error"); }
  else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(),"ERR_INVALID_ARG")!=0) {
    fail("set_title missing", "wrong error code");
  } else ok("set_title missing");

  /* set_size ok */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a2 = parse(arena, "{\"w\":800,\"h\":600}");
  nv_op_window_set_size(win, 3, a2, arena);
  if (!test_last_reply_ok()) { fail("set_size ok", "expected ok"); } else ok("set_size ok");

  /* set_size missing -> ok (defaults applied) */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a2m = parse(arena, "{\"w\":800}");
  nv_op_window_set_size(win, 4, a2m, arena);
  if (!test_last_reply_ok()) { fail("set_size missing", "expected ok"); } else ok("set_size missing");

  /* get_size returns {w,h} */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_window_get_size(win, 5, NULL, arena);
  if (!test_last_reply_ok()) { fail("get_size ok", "expected ok"); }
  else {
    const char* js = test_last_reply_json();
    nv_arena_t* pa = nv_arena_create(2048);
    nv_json_val_t* v = parse(pa, js ? js : "null");
    long long wv = nv_json_get_int(v, "w");
    long long hv = nv_json_get_int(v, "h");
    if (wv <= 0 || hv <= 0) fail("get_size data", "invalid w/h");
    else ok("get_size data");
    nv_arena_destroy(pa);
  }

  /* set_position ok and missing */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a3 = parse(arena, "{\"x\":10,\"y\":20}");
  nv_op_window_set_position(win, 6, a3, arena);
  if (!test_last_reply_ok()) fail("set_position ok", "expected ok"); else ok("set_position ok");
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a3m = parse(arena, "{\"x\":10}");
  nv_op_window_set_position(win, 7, a3m, arena);
  if (!test_last_reply_ok()) fail("set_position missing", "expected ok"); else ok("set_position missing");

  /* set_min_size ok and missing (ok) */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a4 = parse(arena, "{\"w\":320,\"h\":240}");
  nv_op_window_set_min_size(win, 8, a4, arena);
  if (!test_last_reply_ok()) fail("set_min_size ok", "expected ok"); else ok("set_min_size ok");
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a4m = parse(arena, "{\"w\":320}");
  nv_op_window_set_min_size(win, 9, a4m, arena);
  if (!test_last_reply_ok()) fail("set_min_size missing", "expected ok"); else ok("set_min_size missing");

  /* center ok */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_window_center(win, 10, NULL, arena);
  if (!test_last_reply_ok()) fail("center ok", "expected ok"); else ok("center ok");

  /* fullscreen ok and missing (ok defaults to 0) */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a5 = parse(arena, "{\"enable\":true}");
  nv_op_window_fullscreen(win, 11, a5, arena);
  if (!test_last_reply_ok()) fail("fullscreen ok", "expected ok"); else ok("fullscreen ok");
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a5m = parse(arena, "{}");
  nv_op_window_fullscreen(win, 12, a5m, arena);
  if (!test_last_reply_ok()) fail("fullscreen missing", "expected ok"); else ok("fullscreen missing");

  /* set_resizable ok and missing (ok) */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a6 = parse(arena, "{\"resizable\":false}");
  nv_op_window_set_resizable(win, 13, a6, arena);
  if (!test_last_reply_ok()) fail("set_resizable ok", "expected ok"); else ok("set_resizable ok");
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a6m = parse(arena, "{}");
  nv_op_window_set_resizable(win, 14, a6m, arena);
  if (!test_last_reply_ok()) fail("set_resizable missing", "expected ok"); else ok("set_resizable missing");

  /* set_always_on_top ok and missing (ok) */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a7 = parse(arena, "{\"enable\":true}");
  nv_op_window_set_always_on_top(win, 15, a7, arena);
  if (!test_last_reply_ok()) fail("set_always_on_top ok", "expected ok"); else ok("set_always_on_top ok");
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a7m = parse(arena, "{}");
  nv_op_window_set_always_on_top(win, 16, a7m, arena);
  if (!test_last_reply_ok()) fail("set_always_on_top missing", "expected ok"); else ok("set_always_on_top missing");

  /* set_opacity ok and missing (ok) */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a8 = parse(arena, "{\"opacity\":90}");
  nv_op_window_set_opacity(win, 17, a8, arena);
  if (!test_last_reply_ok()) fail("set_opacity ok", "expected ok"); else ok("set_opacity ok");
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a8m = parse(arena, "{}");
  nv_op_window_set_opacity(win, 18, a8m, arena);
  if (!test_last_reply_ok()) fail("set_opacity missing", "expected ok"); else ok("set_opacity missing");

  /* set_zoom_factor ok and missing (error) */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a9 = parse(arena, "{\"factor\":1.2}");
  nv_op_window_set_zoom_factor(win, 19, a9, arena);
  if (!test_last_reply_ok()) fail("set_zoom_factor ok", "expected ok"); else ok("set_zoom_factor ok");
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* a9m = parse(arena, "{}");
  nv_op_window_set_zoom_factor(win, 20, a9m, arena);
  if (test_last_reply_ok()) { fail("set_zoom_factor missing", "expected error"); }
  else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(),"ERR_INVALID_ARG")!=0) {
    fail("set_zoom_factor missing", "wrong error code");
  } else ok("set_zoom_factor missing");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void){
  printf("=== nativeview Window Ops Tests ===\n\n");
  test_valid_and_missing_args();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
