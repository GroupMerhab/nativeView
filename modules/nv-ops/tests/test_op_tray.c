#include "nv_op_tray.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(__APPLE__)
#include <unistd.h>
#endif

#include "test_helpers.h"

/* Strong tray hook overrides for unit tests (overrides weak platform hooks). */
#if (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && !defined(__APPLE__)
int nv_linux_tray_create(long long id, const char* icon_path, const char* tooltip, nv_window_t* w) {
  (void)id;
  (void)icon_path;
  (void)tooltip;
  (void)w;
  return 0;
}
int nv_linux_tray_destroy(long long id) {
  (void)id;
  return 0;
}
int nv_linux_tray_set_icon(long long id, const char* icon_path) {
  (void)id;
  (void)icon_path;
  return 0;
}
int nv_linux_tray_set_tooltip(long long id, const char* tooltip) {
  (void)id;
  (void)tooltip;
  return 0;
}
int nv_linux_tray_set_menu(long long id, const char** labels, const long long* item_ids, int count) {
  (void)id;
  (void)labels;
  (void)item_ids;
  (void)count;
  return 0;
}
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__APPLE__)
int nv_mac_tray_create(long long id, const char* icon_path, const char* tooltip, nv_window_t* w) {
  (void)id;
  (void)icon_path;
  (void)tooltip;
  (void)w;
  return 0;
}
int nv_mac_tray_destroy(long long id) {
  (void)id;
  return 0;
}
int nv_mac_tray_set_icon(long long id, const char* icon_path) {
  (void)id;
  (void)icon_path;
  return 0;
}
int nv_mac_tray_set_tooltip(long long id, const char* tooltip) {
  (void)id;
  (void)tooltip;
  return 0;
}
int nv_mac_tray_set_menu(long long id, const char** labels, const long long* item_ids, int count) {
  (void)id;
  (void)labels;
  (void)item_ids;
  (void)count;
  return 0;
}
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

static void expect_ok(const char* name) {
  if (!test_last_reply_ok()) {
    fail(name, test_last_reply_json() ? test_last_reply_json() : "(null reply)");
    return;
  }
  ok(name);
}

static nv_window_t* make_window(void) {
  nv_window_cfg_t cfg = {"TrayTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(NULL, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

#if defined(__APPLE__)
static const char* mac_icon_path_for_test(void) {
  static const char* const candidates[] = {
      "/System/Library/CoreServices/Finder.app/Contents/Resources/Finder.icns",
      "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/ExecutableBinaryIcon.icns",
      NULL};
  for (size_t i = 0; candidates[i]; i++) {
    if (access(candidates[i], R_OK) == 0) {
      return candidates[i];
    }
  }
  return NULL;
}
#endif

static void test_tray_ops(void) {
  nv_window_t* win = make_window();
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
  nv_op_tray_create(win, 1, parse(arena, "{\"id\":1}"), arena);
  expect_ok("create");

#if defined(__APPLE__)
  {
    const char* ic = mac_icon_path_for_test();
    if (ic) {
      char buf[512];
      snprintf(buf, sizeof(buf), "{\"id\":1,\"path\":\"%s\"}", ic);
      test_reset_replies();
      nv_arena_reset(arena);
      nv_op_tray_set_icon(win, 2, parse(arena, buf), arena);
      expect_ok("set_icon");
    } else {
      printf("  (skip set_icon: no readable system icon path)\n");
    }
  }
#elif defined(_WIN32)
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_set_icon(win, 2, parse(arena, "{\"id\":1,\"path\":\"C:\\\\fake.ico\"}"), arena);
  expect_ok("set_icon");
#else
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_set_icon(win, 2, parse(arena, "{\"id\":1,\"path\":\"/tmp/icon.png\"}"), arena);
  expect_ok("set_icon");
#endif

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_set_tooltip(win, 3, parse(arena, "{\"id\":1,\"text\":\"Tip\"}"), arena);
  expect_ok("set_tooltip");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_set_menu(win, 4, parse(arena, "{\"id\":1,\"items\":[]}"), arena);
  expect_ok("set_menu");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_destroy(win, 5, parse(arena, "{\"id\":1}"), arena);
  expect_ok("destroy");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_tray_destroy(win, 6, parse(arena, "{\"id\":0}"), arena);
  if (test_last_reply_ok()) {
    fail("destroy invalid id", "expected ERR_INVALID_ARG");
  } else if (!test_last_reply_json_has_error_key()) {
    fail("destroy invalid id", "expected JSON with \"error\" key");
  } else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_INVALID_ARG") != 0) {
    fail("destroy invalid id", "wrong error code");
  } else {
    ok("destroy invalid id");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
#ifdef _WIN32
  /* Allow nv_win_tray_* without a platform HWND (nv_window_alloc-only tests). */
  static char nv_tray_headless_env[] = "NV_TRAY_UNIT_HEADLESS=1";
  (void)putenv(nv_tray_headless_env);
#endif
  printf("=== nativeview Tray Ops Tests ===\n\n");
  test_tray_ops();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
