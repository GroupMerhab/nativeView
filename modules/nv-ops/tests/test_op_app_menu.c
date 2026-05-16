/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * test_op_app_menu.c — app.setMenu JSON parse + dispatch smoke tests
 * =============================================================================
 */
#include "nv_op_app.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_menu.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_helpers.h"

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name) {
  printf("✓ %s\n", name);
  tests_passed++;
}
static void fail(const char* name, const char* why) {
  printf("✗ %s: %s\n", name, why);
  tests_failed++;
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

static int g_menu_calls;
static const nv_menu_item_t* g_last_items;
static int g_last_count;

static void test_app_set_menu(nv_window_t* w, const nv_menu_item_t* items, int count) {
  (void)w;
  g_menu_calls++;
  g_last_items = items;
  g_last_count = count;
}

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = {
      .title = "MenuTest",
      .width = 320,
      .height = 240,
      .min_width = 0,
      .min_height = 0,
      .resizable = 1,
      .frameless = 0,
      .transparent = 0,
      .devtools = 0,
      .modal = 0,
  };
  return nv_window_alloc(app, &cfg);
}

static void test_parse_and_dispatch_json_menu(void) {
  g_menu_calls = 0;
  g_last_items = NULL;
  g_last_count = -1;

  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.app_set_menu = test_app_set_menu;

  nv_window_t* win = make_window(&app);
  if (!win) {
    fail("window", "nv_window_alloc");
    return;
  }

  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    fail("arena", "nv_arena_create");
    nv_window_destroy(win);
    return;
  }

  const char* json =
      "["
      "{\"id\":\"file\",\"label\":\"_File\",\"children\":["
      "{\"id\":\"file.new\",\"label\":\"_New\",\"shortcut\":\"CmdOrCtrl+N\"},"
      "{\"separator\":true},"
      "{\"id\":\"file.save\",\"label\":\"_Save\",\"shortcut\":\"CmdOrCtrl+S\",\"disabled\":true}"
      "]}"
      "]";

  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* args = parse(arena, json);
  nv_op_app_set_menu(win, 42, args, arena);

  if (!test_last_reply_ok()) {
    fail("setMenu reply", test_last_reply_error_code() ? test_last_reply_error_code() : "not ok");
    goto cleanup;
  }
  if (g_menu_calls != 1) {
    fail("app_set_menu calls", "expected 1");
    goto cleanup;
  }
  if (g_last_count != 1) {
    fail("top-level count", "expected 1");
    goto cleanup;
  }
  const nv_menu_item_t* r = g_last_items;
  if (!r || !r->label || strcmp(r->label, "_File") != 0) {
    fail("root label", "mismatch");
    goto cleanup;
  }
  if (r->child_count != 3) {
    fail("child_count", "expected 3 children");
    goto cleanup;
  }
  if (!r->children[0].id || strcmp(r->children[0].id, "file.new") != 0) {
    fail("child0 id", "mismatch");
    goto cleanup;
  }
  if (!r->children[0].shortcut || strstr(r->children[0].shortcut, "CmdOrCtrl") == NULL) {
    fail("child0 shortcut", "missing");
    goto cleanup;
  }
  if (!r->children[1].separator) {
    fail("separator", "expected separator item");
    goto cleanup;
  }
  if (r->children[2].enabled != 0) {
    fail("disabled", "expected enabled 0");
    goto cleanup;
  }
  ok("app.setMenu JSON parse and vtable dispatch");

cleanup:
  nv_arena_destroy(arena);
  nv_window_destroy(win);
}

static void test_set_menu_not_supported(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.app_set_menu = NULL;

  nv_window_t* win = make_window(&app);
  if (!win) {
    fail("window", "nv_window_alloc");
    return;
  }
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    fail("arena", "nv_arena_create");
    nv_window_destroy(win);
    return;
  }
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_app_set_menu(win, 1, parse(arena, "[]"), arena);
  if (test_last_reply_ok()) {
    fail("not supported", "expected error reply");
  } else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_NOT_SUPPORTED") != 0) {
    fail("not supported", "wrong error code");
  } else {
    ok("app.setMenu returns ERR_NOT_SUPPORTED when missing");
  }
  nv_arena_destroy(arena);
  nv_window_destroy(win);
}

int main(void) {
  printf("=== nativeview app.setMenu op tests ===\n\n");
  test_parse_and_dispatch_json_menu();
  test_set_menu_not_supported();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
