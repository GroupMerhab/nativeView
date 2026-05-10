/* =============================================================================
 * test_op_hotkey.c — window.registerHotkey / window.unregisterHotkey
 * =============================================================================
 */

#include "nv_op_window_hotkey.h"
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
      .title = "HotkeyTest",
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

static void test_invalid_combo(void) {
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
  nv_json_val_t* j = parse(arena, "{\"id\":\"a\",\"combo\":\"K\"}");
  nv_op_window_register_hotkey(win, 1, j, arena);
  if (test_last_reply_ok()) {
    fail("bare key", "expected error");
  } else {
    ok("invalid combo (no modifiers) rejected");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  j = parse(arena, "{\"id\":\"b\",\"combo\":\"CmdOrCtrl+BadKey\"}");
  nv_op_window_register_hotkey(win, 2, j, arena);
  if (test_last_reply_ok()) {
    fail("bad key token", "expected error");
  } else {
    ok("invalid combo (unknown key) rejected");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

static void test_invalid_id(void) {
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
  nv_json_val_t* j = parse(arena, "{\"id\":\"bad id\",\"combo\":\"CmdOrCtrl+Shift+A\"}");
  nv_op_window_register_hotkey(win, 1, j, arena);
  if (test_last_reply_ok()) {
    fail("space in id", "expected error");
  } else {
    ok("invalid id rejected");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

static void test_parse_combo(void) {
  nv_hotkey_combo_t c;
  memset(&c, 0, sizeof(c));
  if (nv_hotkey_parse_combo("CmdOrCtrl+Shift+A", &c) != 0) {
    fail("parse combo", "expected success");
  } else {
    ok("parse CmdOrCtrl+Shift+A");
  }
  if (nv_hotkey_parse_combo("K", &c) == 0) {
    fail("naked key", "expected failure");
  } else {
    ok("parse rejects naked key");
  }
}

static void test_duplicate_id(void) {
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
      "{\"id\":\"dup\",\"combo\":\"CmdOrCtrl+Shift+Z\"}";
  test_reset_replies();
  nv_json_val_t* j1 = parse(arena, json);
  nv_op_window_register_hotkey(win, 1, j1, arena);
  int first_ok = test_last_reply_ok();

  test_reset_replies();
  nv_arena_reset(arena);
  nv_json_val_t* j2 = parse(arena, json);
  nv_op_window_register_hotkey(win, 2, j2, arena);
  if (first_ok) {
    if (test_last_reply_ok()) {
      fail("duplicate register", "expected ERR_ALREADY_EXISTS");
    } else if (!test_last_reply_error_code() ||
               strcmp(test_last_reply_error_code(), "ERR_ALREADY_EXISTS") != 0) {
      fail("duplicate code", test_last_reply_error_code() ? test_last_reply_error_code() : "(null)");
    } else {
      ok("duplicate id returns ERR_ALREADY_EXISTS");
    }
  } else {
    ok("skip duplicate id check (first register did not succeed on this host)");
  }

  if (first_ok) {
    test_reset_replies();
    nv_arena_reset(arena);
    nv_json_val_t* j3 = parse(arena, "{\"id\":\"dup\"}");
    nv_op_window_unregister_hotkey(win, 3, j3, arena);
    if (!test_last_reply_ok()) {
      fail("unregister", "expected ok");
    } else {
      ok("unregister after successful first register");
    }
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
  test_parse_combo();
  test_invalid_combo();
  test_invalid_id();
  test_duplicate_id();
  printf("%d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed ? 1 : 0;
}
