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

#if !defined(_WIN32) && !defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
__attribute__((weak)) void nv_mac_app_set_menu(const nv_menu_item_t* items, int count);

static int g_mac_menu_calls;
static const nv_menu_item_t* g_mac_last_items;
static int g_mac_last_count;

void nv_mac_app_set_menu(const nv_menu_item_t* items, int count) {
  g_mac_menu_calls++;
  g_mac_last_items = items;
  g_mac_last_count = count;
}
#define NV_TEST_APP_MENU_MOCK 1
#endif

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

#if defined(_WIN32)

static void test_set_menu_unsupported_on_windows(void) {
  nv_window_cfg_t cfg = {
      .title = "m",
      .width = 100,
      .height = 100,
      .min_width = 0,
      .min_height = 0,
      .resizable = 1,
      .frameless = 0,
      .transparent = 0,
      .devtools = 0,
      .modal = 0,
  };
  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (!win) {
    fail("win alloc", "nv_window_alloc");
    return;
  }
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    fail("arena", "nv_arena_create");
    nv_window_free(win);
    return;
  }
  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* args = parse(arena, "[]");
  nv_op_app_set_menu(win, 1, args, arena);
  if (test_last_reply_ok()) {
    fail("windows unsupported", "expected error reply");
  } else if (!test_last_reply_error_code() ||
             strcmp(test_last_reply_error_code(), "ERR_NOT_SUPPORTED") != 0) {
    fail("windows unsupported", "wrong error code");
  } else {
    ok("app.setMenu returns ERR_NOT_SUPPORTED on Windows");
  }
  nv_arena_destroy(arena);
  nv_window_free(win);
}

#else /* !_WIN32 */

static nv_window_t* make_stub_window_with_app(nv_app_t* app) {
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
  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (win) {
    win->app = app;
  }
  return win;
}

static void test_parse_and_dispatch_json_menu(void) {
#ifdef NV_TEST_APP_MENU_MOCK
  g_mac_menu_calls = 0;
  g_mac_last_items = NULL;
  g_mac_last_count = -1;
#endif

  nv_app_t* app = nv_app_alloc();
  if (!app) {
    fail("app", "nv_app_alloc");
    return;
  }
  nv_window_t* win = make_stub_window_with_app(app);
  if (!win) {
    fail("window", "nv_window_alloc");
    nv_app_free(app);
    return;
  }

  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    fail("arena", "nv_arena_create");
    nv_window_destroy(win);
    nv_app_free(app);
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

#ifdef NV_TEST_APP_MENU_MOCK
  if (g_mac_menu_calls != 1) {
    fail("nv_mac_app_set_menu calls", "expected 1");
    goto cleanup;
  }
  if (g_mac_last_count != 1) {
    fail("top-level count", "expected 1");
    goto cleanup;
  }
  const nv_menu_item_t* r = g_mac_last_items;
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
#endif

#ifdef NV_TEST_APP_MENU_MOCK
  ok("app.setMenu JSON parse and nv_mac_app_set_menu dispatch");
#else
  ok("app.setMenu JSON parse (platform menu)");
#endif

cleanup:
  nv_arena_destroy(arena);
  nv_window_destroy(win);
  nv_app_free(app);
}

#endif /* !_WIN32 */

int main(void) {
  printf("=== nativeview app.setMenu op tests ===\n\n");
#if defined(_WIN32)
  test_set_menu_unsupported_on_windows();
#else
  test_parse_and_dispatch_json_menu();
#endif
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
