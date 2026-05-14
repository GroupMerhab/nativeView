#include "nv_op_notification.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include "nv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

enum { NV_RC_NOT_SUPPORTED = -100 };

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

static void expect_err_not_supported(const char* name) {
  if (test_last_reply_ok()) {
    fail(name, "expected error reply");
    return;
  }
  if (!test_last_reply_json_has_error_key()) {
    fail(name, "expected JSON with \"error\" key");
    return;
  }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_NOT_SUPPORTED") != 0) {
    fail(name, "expected ERR_NOT_SUPPORTED");
    return;
  }
  ok(name);
}

static void expect_err_permission(const char* name) {
  if (test_last_reply_ok()) {
    fail(name, "expected error reply");
    return;
  }
  if (!test_last_reply_json_has_error_key()) {
    fail(name, "expected JSON with \"error\" key");
    return;
  }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_PERMISSION") != 0) {
    fail(name, "expected ERR_PERMISSION");
    return;
  }
  ok(name);
}

static void expect_err_io(const char* name) {
  if (test_last_reply_ok()) {
    fail(name, "expected error reply");
    return;
  }
  if (!test_last_reply_json_has_error_key()) {
    fail(name, "expected JSON with \"error\" key");
    return;
  }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_IO") != 0) {
    fail(name, "expected ERR_IO");
    return;
  }
  ok(name);
}

static int g_show_called = 0;
static int g_close_called = 0;
static int g_show_rc = 0;
static int g_close_rc = 0;

static int test_notification_show(long long id, const char* title, const char* body,
                                  const char* icon_path, nv_window_t* w) {
  (void)id;
  (void)title;
  (void)body;
  (void)icon_path;
  (void)w;
  g_show_called++;
  return g_show_rc;
}

static int test_notification_close(long long id) {
  (void)id;
  g_close_called++;
  return g_close_rc;
}

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = {"NotifTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void test_notification_ops(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.notification_show = test_notification_show;
  app.platform_api.notification_close = test_notification_close;

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
  reset_send_mocks();
  nv_arena_reset(arena);
  g_show_called = 0;
  g_show_rc = 0;
  nv_op_notification_show(win, 1, parse(arena, "{\"id\":1,\"title\":\"Hi\",\"body\":\"Msg\"}"), arena);
  if (!test_last_reply_ok()) {
    fail("show ok", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    const char* js = test_last_reply_json();
    if (!js || strcmp(js, "{}") != 0) {
      fail("show reply shape", js ? js : "(null)");
    } else if (g_nv_send_count != 0) {
      fail("show no push", "nv_send should not fire for notification.show");
    } else if (g_show_called != 1) {
      fail("show called", "expected one call");
    } else {
      ok("show ok and empty result");
    }
  }

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_close_called = 0;
  g_close_rc = 0;
  nv_op_notification_close(win, 2, parse(arena, "{\"id\":1}"), arena);
  if (!test_last_reply_ok()) {
    fail("close ok", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    const char* js = test_last_reply_json();
    if (!js || strcmp(js, "{}") != 0) {
      fail("close reply shape", js ? js : "(null)");
    } else if (g_nv_send_count != 0) {
      fail("close no push", "nv_send should not fire for notification.close");
    } else if (g_close_called != 1) {
      fail("close called", "expected one call");
    } else {
      ok("close ok and empty result");
    }
  }

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_show_called = 0;
  g_show_rc = NV_RC_NOT_SUPPORTED;
  nv_op_notification_show(win, 10, parse(arena, "{\"id\":1,\"title\":\"Hi\",\"body\":\"Msg\"}"), arena);
  expect_err_not_supported("show not supported");
  if (g_show_called != 1) fail("show not supported called", "expected one call");

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_show_called = 0;
  g_show_rc = -1;
  nv_op_notification_show(win, 11, parse(arena, "{\"id\":1,\"title\":\"Hi\",\"body\":\"Msg\"}"), arena);
  expect_err_permission("show permission");
  if (g_show_called != 1) fail("show permission called", "expected one call");

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_show_called = 0;
  g_show_rc = -2;
  nv_op_notification_show(win, 12, parse(arena, "{\"id\":1,\"title\":\"Hi\",\"body\":\"Msg\"}"), arena);
  expect_err_io("show io");
  if (g_show_called != 1) fail("show io called", "expected one call");

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_close_called = 0;
  g_close_rc = NV_RC_NOT_SUPPORTED;
  nv_op_notification_close(win, 13, parse(arena, "{\"id\":1}"), arena);
  expect_err_not_supported("close not supported");
  if (g_close_called != 1) fail("close not supported called", "expected one call");

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_close_called = 0;
  g_close_rc = -2;
  nv_op_notification_close(win, 14, parse(arena, "{\"id\":1}"), arena);
  expect_err_io("close io");
  if (g_close_called != 1) fail("close io called", "expected one call");

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_show_called = 0;
  app.platform_api.notification_show = NULL;
  nv_op_notification_show(win, 15, parse(arena, "{\"id\":1,\"title\":\"Hi\",\"body\":\"Msg\"}"), arena);
  expect_err_not_supported("show missing vtable");
  if (g_show_called != 0) fail("show missing vtable called", "unexpected call");
  app.platform_api.notification_show = test_notification_show;

  test_reset_replies();
  reset_send_mocks();
  nv_arena_reset(arena);
  g_close_called = 0;
  app.platform_api.notification_close = NULL;
  nv_op_notification_close(win, 16, parse(arena, "{\"id\":1}"), arena);
  expect_err_not_supported("close missing vtable");
  if (g_close_called != 0) fail("close missing vtable called", "unexpected call");
  app.platform_api.notification_close = test_notification_close;

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
  printf("=== nativeview Notification Ops Tests ===\n\n");
  test_notification_ops();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
