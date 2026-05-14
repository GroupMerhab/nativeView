#include "nv_op_tray.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static void expect_ok(const char* name) {
  if (!test_last_reply_ok()) {
    fail(name, test_last_reply_json() ? test_last_reply_json() : "(null reply)");
    return;
  }
  ok(name);
}

static void expect_err(const char* name, const char* code) {
  if (test_last_reply_ok()) {
    fail(name, "expected error reply");
    return;
  }
  if (!test_last_reply_json_has_error_key()) {
    fail(name, "expected JSON with \"error\" key");
    return;
  }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), code) != 0) {
    fail(name, "wrong error code");
    return;
  }
  ok(name);
}

static int g_tray_create_calls;
static int g_tray_destroy_calls;
static int g_tray_set_icon_calls;
static int g_tray_set_tooltip_calls;
static int g_tray_set_menu_calls;

static int test_tray_create(long long id, const char* icon_path, const char* tooltip, nv_window_t* w) {
  (void)id;
  (void)icon_path;
  (void)tooltip;
  (void)w;
  g_tray_create_calls++;
  return 0;
}
static int test_tray_destroy(long long id) {
  (void)id;
  g_tray_destroy_calls++;
  return 0;
}
static int test_tray_set_icon(long long id, const char* icon_path) {
  (void)id;
  (void)icon_path;
  g_tray_set_icon_calls++;
  return 0;
}
static int test_tray_set_tooltip(long long id, const char* tooltip) {
  (void)id;
  (void)tooltip;
  g_tray_set_tooltip_calls++;
  return 0;
}
static int test_tray_set_menu(long long id, const char** labels, const long long* item_ids, int count) {
  (void)id;
  (void)labels;
  (void)item_ids;
  (void)count;
  g_tray_set_menu_calls++;
  return 0;
}

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = {"TrayTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void test_tray_ops(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.tray_create = test_tray_create;
  app.platform_api.tray_destroy = test_tray_destroy;
  app.platform_api.tray_set_icon = test_tray_set_icon;
  app.platform_api.tray_set_tooltip = test_tray_set_tooltip;
  app.platform_api.tray_set_menu = test_tray_set_menu;

  nv_window_t* win = make_window(&app);
  if (!win) {
    fail("make_window", "alloc failed");
    return;
  }
  nv_arena_t* arena = nv_arena_create(2048);
  if (!arena) {
    fail("arena", "alloc failed");
    nv_window_free(win);
    return;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  g_tray_create_calls = 0;
  nv_op_tray_create(win, 1, parse(arena, "{\"id\":1}"), arena);
  expect_ok("create");
  if (g_tray_create_calls != 1) fail("create dispatch", "expected one call");

  test_reset_replies();
  nv_arena_reset(arena);
  g_tray_set_icon_calls = 0;
  nv_op_tray_set_icon(win, 2, parse(arena, "{\"id\":1,\"path\":\"/tmp/icon.png\"}"), arena);
  expect_ok("set_icon");
  if (g_tray_set_icon_calls != 1) fail("set_icon dispatch", "expected one call");

  test_reset_replies();
  nv_arena_reset(arena);
  g_tray_set_tooltip_calls = 0;
  nv_op_tray_set_tooltip(win, 3, parse(arena, "{\"id\":1,\"text\":\"Tip\"}"), arena);
  expect_ok("set_tooltip");
  if (g_tray_set_tooltip_calls != 1) fail("set_tooltip dispatch", "expected one call");

  test_reset_replies();
  nv_arena_reset(arena);
  g_tray_set_menu_calls = 0;
  nv_op_tray_set_menu(win, 4, parse(arena, "{\"id\":1,\"items\":[]}"), arena);
  expect_ok("set_menu");
  if (g_tray_set_menu_calls != 1) fail("set_menu dispatch", "expected one call");

  test_reset_replies();
  nv_arena_reset(arena);
  g_tray_destroy_calls = 0;
  nv_op_tray_destroy(win, 5, parse(arena, "{\"id\":1}"), arena);
  expect_ok("destroy");
  if (g_tray_destroy_calls != 1) fail("destroy dispatch", "expected one call");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_destroy(win, 6, parse(arena, "{\"id\":0}"), arena);
  expect_err("destroy invalid id", "ERR_INVALID_ARG");

  app.platform_api.tray_create = NULL;
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_create(win, 7, parse(arena, "{\"id\":2}"), arena);
  expect_err("create not supported", "ERR_NOT_SUPPORTED");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
  printf("=== nativeview Tray Ops Tests ===\n\n");
  test_tray_ops();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
