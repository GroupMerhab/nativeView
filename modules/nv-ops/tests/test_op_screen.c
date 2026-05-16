/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * test_op_screen.c — screen.* op handlers (mock hooks + assertions)
 * =============================================================================
 */
#include "nv_op_screen.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

static char* test_screen_get_all(void) {
  return strdup(
      "[{\"id\":7,\"x\":10,\"y\":20,\"width\":800,\"height\":600,\"scaleFactor\":2,"
      "\"localizedName\":\"MockDisplay\",\"isPrimary\":true,"
      "\"bounds\":{\"x\":10,\"y\":20,\"width\":800,\"height\":600},"
      "\"workArea\":{\"x\":10,\"y\":40,\"width\":800,\"height\":560}}]");
}

static char* test_screen_get_primary(void) {
  return strdup(
      "{\"id\":0,\"x\":0,\"y\":0,\"width\":1920,\"height\":1080,\"scaleFactor\":1.5,"
      "\"localizedName\":\"PrimaryMock\",\"isPrimary\":true,"
      "\"bounds\":{\"x\":0,\"y\":0,\"width\":1920,\"height\":1080},"
      "\"workArea\":{\"x\":0,\"y\":25,\"width\":1920,\"height\":1055}}");
}

static char* test_screen_get_cursor(void) { return strdup("{\"x\":101,\"y\":202}"); }

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name) {
  printf("✓ %s\n", name);
  tests_passed++;
}
static void fail(const char* name, const char* why) {
  printf("✗ %s: %s\n", name, why);
  tests_failed++;
}

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = {"ScreenTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void test_screen_mock_replies(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.screen_get_all = test_screen_get_all;
  app.platform_api.screen_get_primary = test_screen_get_primary;
  app.platform_api.screen_get_cursor = test_screen_get_cursor;

  nv_window_t* win = make_window(&app);
  if (!win) {
    fail("make_window", "alloc failed");
    return;
  }
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    fail("arena", "alloc failed");
    nv_window_free(win);
    return;
  }

  /* getAll -> { displays: [...] } */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_screen_get_all(win, 1, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("getAll ok", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    nv_arena_t* pa = nv_arena_create(4096);
    nv_json_val_t* root = parse(pa, test_last_reply_json());
    nv_json_val_t* displays = root ? nv_json_get_obj(root, "displays") : NULL;
    if (!displays || !nv_json_is_array(displays) || nv_json_array_len(displays) != 1) {
      fail("getAll displays", "expected one display");
    } else {
      nv_json_val_t* d0 = nv_json_array_get(displays, 0);
      long long id = nv_json_get_int(d0, "id");
      long long x = nv_json_get_int(d0, "x");
      const char* nm = nv_json_get_str(d0, "name");
      nv_json_val_t* b = nv_json_get_obj(d0, "bounds");
      long long bw = b ? nv_json_get_int(b, "width") : 0;
      if (id != 7 || x != 10 || !nm || strcmp(nm, "MockDisplay") != 0 || bw != 800) {
        fail("getAll payload", "field mismatch");
      } else {
        ok("getAll mock payload");
      }
    }
    nv_arena_destroy(pa);
  }

  /* getPrimary */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_screen_get_primary(win, 2, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("getPrimary ok", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    nv_arena_t* pa = nv_arena_create(4096);
    nv_json_val_t* root = parse(pa, test_last_reply_json());
    long long w = root ? nv_json_get_int(root, "width") : 0;
    double sc = root ? nv_json_get_double(root, "scaleFactor") : 0.0;
    int prim = root ? nv_json_get_bool(root, "isPrimary") : 0;
    if (w != 1920 || sc < 1.4 || sc > 1.6 || !prim) {
      fail("getPrimary payload", "field mismatch");
    } else {
      ok("getPrimary mock payload");
    }
    nv_arena_destroy(pa);
  }

  /* getCursorPos */
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_screen_get_cursor(win, 3, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("getCursor ok", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    nv_arena_t* pa = nv_arena_create(4096);
    nv_json_val_t* root = parse(pa, test_last_reply_json());
    long long x = root ? nv_json_get_int(root, "x") : 0;
    long long y = root ? nv_json_get_int(root, "y") : 0;
    if (x != 101 || y != 202) {
      fail("getCursor payload", "field mismatch");
    } else {
      ok("getCursor mock payload");
    }
    nv_arena_destroy(pa);
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
  printf("=== nativeview Screen Ops Tests ===\n\n");
  test_screen_mock_replies();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
