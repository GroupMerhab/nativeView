#include "nv_op_notification.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include "nv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

#if defined(__APPLE__)
/* Strong mocks override weak nv_mac_notification_* from nv-platform-mac.a */
int nv_mac_notification_show(long long id, const char* title, const char* body, const char* icon_path,
                             nv_window_t* w) {
  (void)id;
  (void)title;
  (void)body;
  (void)icon_path;
  (void)w;
  return 0;
}

int nv_mac_notification_close(long long id) {
  (void)id;
  return 0;
}
#endif

#if defined(__linux__) && !defined(__APPLE__)
int nv_linux_notification_show(long long id, const char *title, const char *body, const char *icon_path,
                               nv_window_t *w) {
  (void)id;
  (void)title;
  (void)body;
  (void)icon_path;
  (void)w;
  return 0;
}

int nv_linux_notification_close(long long id) {
  (void)id;
  return 0;
}
#endif

static int g_nv_send_count = 0;

NV_API void nv_send(nv_window_t* window, const char* event, const char* json) {
  (void)window;
  (void)json;
  (void)event;
  g_nv_send_count++;
}

static void reset_send_mocks(void) {
  g_nv_send_count = 0;
}

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name) {
  printf("✓ %s\n", name);
  tests_passed++;
}
static void fail(const char* name, const char* why) {
  printf("✗ %s: %s\n", name, why);
  tests_failed++;
}

#if !defined(__APPLE__) && !defined(_WIN32) && !(defined(__linux__) && !defined(__APPLE__))
static void expect_err_not_implemented(const char* name) {
  if (test_last_reply_ok()) {
    fail(name, "expected error reply");
    return;
  }
  if (!test_last_reply_json_has_error_key()) {
    fail(name, "expected JSON with \"error\" key");
    return;
  }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_NOT_IMPLEMENTED") != 0) {
    fail(name, "expected ERR_NOT_IMPLEMENTED");
    return;
  }
  ok(name);
}
#endif

static nv_window_t* make_window(void) {
  nv_window_cfg_t cfg = {"NotifTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(NULL, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void test_notification_ops(void) {
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

#if defined(__APPLE__) || defined(_WIN32) || (defined(__linux__) && !defined(__APPLE__))
  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  nv_op_notification_show(win, 1, parse(arena, "{\"id\":1,\"title\":\"Hi\",\"body\":\"Msg\"}"), arena);
  if (!test_last_reply_ok()) {
    fail("show ok", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    const char* js = test_last_reply_json();
    if (!js || strcmp(js, "{}") != 0) {
      fail("show reply shape", js ? js : "(null)");
    } else if (g_nv_send_count != 0) {
      fail("show no push", "nv_send should not fire for notification.show");
    } else {
      ok("show ok and empty result");
    }
  }

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  nv_op_notification_close(win, 2, parse(arena, "{\"id\":1}"), arena);
  if (!test_last_reply_ok()) {
    fail("close ok", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    const char* js = test_last_reply_json();
    if (!js || strcmp(js, "{}") != 0) {
      fail("close reply shape", js ? js : "(null)");
    } else if (g_nv_send_count != 0) {
      fail("close no push", "nv_send should not fire for notification.close");
    } else {
      ok("close ok and empty result");
    }
  }
#else
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_notification_show(win, 1, NULL, arena);
  expect_err_not_implemented("show");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_notification_close(win, 2, parse(arena, "{\"id\":1}"), arena);
  expect_err_not_implemented("close");
#endif

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_notification_close(win, 3, parse(arena, "{\"id\":0}"), arena);
  if (test_last_reply_ok()) {
    fail("close invalid id", "expected ERR_INVALID_ARG");
  } else if (!test_last_reply_json_has_error_key()) {
    fail("close invalid id", "expected JSON with \"error\" key");
  } else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_INVALID_ARG") != 0) {
    fail("close invalid id", "wrong error code");
  } else {
    ok("close invalid id");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
#if defined(_WIN32)
  (void)_putenv("NATIVEVIEW_STUB_NOTIFICATION=1");
#endif
  printf("=== nativeview Notification Ops Tests ===\n\n");
  test_notification_ops();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
