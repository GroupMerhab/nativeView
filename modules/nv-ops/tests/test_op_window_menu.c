/* =============================================================================
 * test_op_window_menu.c — window.setContextMenu JSON + dispatch tests
 * =============================================================================
 */
#include "nv_op_window.h"
#include "nv_window_internal.h"
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

static nv_window_t* make_window(void) {
  nv_window_cfg_t cfg = {
      .title = "CtxMenuTest",
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
  return nv_window_alloc(NULL, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void test_invalid_payload(void) {
  nv_window_t* win = make_window();
  if (!win) {
    fail("alloc", "nv_window_alloc");
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
  nv_json_val_t* bad = parse(arena, "\"not-an-object\"");
  nv_op_window_set_context_menu(win, 1, bad, arena);
  if (test_last_reply_ok()) {
    fail("invalid type", "expected error");
  } else {
    ok("invalid payload rejected");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* missing = parse(arena, "{}");
  nv_op_window_set_context_menu(win, 2, missing, arena);
  if (test_last_reply_ok()) {
    fail("missing items", "expected error");
  } else {
    ok("missing items rejected");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

static void test_parse_dispatch_and_window_state(void) {
  nv_window_t* win = make_window();
  if (!win) {
    fail("alloc", "nv_window_alloc");
    return;
  }
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    fail("arena", "nv_arena_create");
    nv_window_free(win);
    return;
  }

  const char* json =
      "{\"items\":["
      "{\"id\":\"copy\",\"label\":\"Copy\",\"enabled\":true},"
      "{\"id\":\"paste\",\"label\":\"Paste\",\"disabled\":true}"
      "]}";

  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* args = parse(arena, json);
  nv_op_window_set_context_menu(win, 7, args, arena);

  if (!test_last_reply_ok()) {
    fail("reply", test_last_reply_error_code() ? test_last_reply_error_code() : "not ok");
    goto cleanup;
  }

  if (win->ctx_menu_count != 2 || !win->ctx_menu_items) {
    fail("window ctx count", "mismatch");
    goto cleanup;
  }
  if (!win->ctx_menu_items[0].id || strcmp(win->ctx_menu_items[0].id, "copy") != 0) {
    fail("item0 id", "mismatch");
    goto cleanup;
  }
  if (!win->ctx_menu_items[0].label || strcmp(win->ctx_menu_items[0].label, "Copy") != 0) {
    fail("item0 label", "mismatch");
    goto cleanup;
  }
  if (win->ctx_menu_items[0].enabled != 1) {
    fail("item0 enabled", "expected 1");
    goto cleanup;
  }
  if (win->ctx_menu_items[1].enabled != 0) {
    fail("item1 enabled", "expected 0 for disabled");
    goto cleanup;
  }

  ok("window.setContextMenu parse, window state, and platform dispatch");

cleanup:
  nv_arena_destroy(arena);
  nv_window_free(win);
}

static void test_clear_menu(void) {
  nv_window_t* win = make_window();
  if (!win) {
    fail("alloc", "nv_window_alloc");
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
  nv_op_window_set_context_menu(win, 1, parse(arena, "{\"items\":[{\"id\":\"a\",\"label\":\"A\"}]}"), arena);
  if (!test_last_reply_ok()) {
    fail("set one", "expected ok");
    nv_arena_destroy(arena);
    nv_window_free(win);
    return;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_window_set_context_menu(win, 2, parse(arena, "{\"items\":[]}"), arena);
  if (!test_last_reply_ok()) {
    fail("clear", "expected ok");
  } else if (win->ctx_menu_count != 0 || win->ctx_menu_items != NULL) {
    fail("clear state", "expected empty menu");
  } else {
    ok("clear context menu with empty items");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
  printf("=== nativeview window.setContextMenu op tests ===\n\n");
  test_invalid_payload();
  test_parse_dispatch_and_window_state();
  test_clear_menu();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
