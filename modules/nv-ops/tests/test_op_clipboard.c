#include "nv_op_clipboard.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include "test_helpers.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char k_stub_png_b64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";

static int tests_passed = 0, tests_failed = 0;
static void ok(const char *name) {
  printf("✓ %s\n", name);
  tests_passed++;
}
static void fail(const char *name, const char *why) {
  printf("✗ %s: %s\n", name, why);
  tests_failed++;
}

static void expect_err_invalid_arg(const char* name) {
  if (test_last_reply_ok()) { fail(name, "expected error reply"); return; }
  if (!test_last_reply_json_has_error_key()) { fail(name, "expected JSON with \"error\" key"); return; }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_INVALID_ARG") != 0) {
    fail(name, "expected ERR_INVALID_ARG");
    return;
  }
  ok(name);
}

static void expect_err_not_supported(const char* name) {
  if (test_last_reply_ok()) { fail(name, "expected error reply"); return; }
  if (!test_last_reply_json_has_error_key()) { fail(name, "expected JSON with \"error\" key"); return; }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_NOT_SUPPORTED") != 0) {
    fail(name, "expected ERR_NOT_SUPPORTED");
    return;
  }
  ok(name);
}

static int g_read_text_called = 0;
static char* test_clipboard_read_text(void) {
  const char* s = "clipboard_stub_read_text";
  size_t n = strlen(s) + 1u;
  char* out = (char*)malloc(n);
  if (!out) return NULL;
  memcpy(out, s, n);
  g_read_text_called++;
  return out;
}

static int g_write_text_called = 0;
static int test_clipboard_write_text(const char* text) {
  (void)text;
  g_write_text_called++;
  return 0;
}

static int g_clear_called = 0;
static void test_clipboard_clear(void) {
  g_clear_called++;
}

static int g_has_text_called = 0;
static int test_clipboard_has_text(void) {
  g_has_text_called++;
  return 1;
}

static int g_read_image_called = 0;
static char* test_clipboard_read_image(int* w, int* h) {
  const char* s = k_stub_png_b64;
  size_t n = strlen(s) + 1u;
  char* out = (char*)malloc(n);
  if (!out) return NULL;
  if (w) *w = 1;
  if (h) *h = 1;
  memcpy(out, s, n);
  g_read_image_called++;
  return out;
}

static int g_write_image_called = 0;
static int test_clipboard_write_image(const char* base64_png) {
  (void)base64_png;
  g_write_image_called++;
  return 0;
}

static int g_has_image_called = 0;
static int test_clipboard_has_image(void) {
  g_has_image_called++;
  return 1;
}

static nv_window_t *make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = {"ClipboardTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t *parse(nv_arena_t *arena, const char *s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void expect_image_read_ok(const char *name) {
  const char *j = test_last_reply_json();
  if (!test_last_reply_ok()) {
    fail(name, "expected ok");
    return;
  }
  if (!j || strstr(j, "\"data\"") == NULL || strstr(j, "\"width\"") == NULL ||
      strstr(j, "\"height\"") == NULL || strstr(j, "\"format\"") == NULL) {
    fail(name, "expected width/height/format/data in JSON");
    return;
  }
  ok(name);
}

static void test_clipboard_ops(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.clipboard_read_text = test_clipboard_read_text;
  app.platform_api.clipboard_write_text = test_clipboard_write_text;
  app.platform_api.clipboard_clear = test_clipboard_clear;
  app.platform_api.clipboard_has_text = test_clipboard_has_text;
  app.platform_api.clipboard_read_image = test_clipboard_read_image;
  app.platform_api.clipboard_write_image = test_clipboard_write_image;
  app.platform_api.clipboard_has_image = test_clipboard_has_image;

  nv_window_t *win = make_window(&app);
  if (!win) {
    fail("make_window", "alloc failed");
    return;
  }
  nv_arena_t *arena = nv_arena_create(65536);
  if (!arena) {
    fail("arena", "alloc failed");
    nv_window_free(win);
    return;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  g_read_text_called = 0;
  nv_op_clipboard_read_text(win, 1, NULL, arena);
  if (!test_last_reply_ok()) fail("read_text", "expected ok");
  else if (g_read_text_called != 1) fail("read_text", "expected one call");
  else ok("read_text");

  test_reset_replies();
  nv_arena_reset(arena);
  g_write_text_called = 0;
  nv_op_clipboard_write_text(win, 2, parse(arena, "{\"text\":\"hello\"}"), arena);
  if (!test_last_reply_ok()) fail("write_text", "expected ok");
  else if (g_write_text_called != 1) fail("write_text", "expected one call");
  else ok("write_text");

  test_reset_replies();
  nv_arena_reset(arena);
  g_read_image_called = 0;
  nv_op_clipboard_read_image(win, 3, NULL, arena);
  expect_image_read_ok("read_image");
  if (g_read_image_called != 1) fail("read_image", "expected one call");

  test_reset_replies();
  nv_arena_reset(arena);
  {
    char payload[512];
    (void)snprintf(payload, sizeof payload, "{\"format\":\"png\",\"data\":\"%s\"}", k_stub_png_b64);
    g_write_image_called = 0;
    nv_op_clipboard_write_image(win, 4, parse(arena, payload), arena);
  }
  if (!test_last_reply_ok()) fail("write_image", "expected ok");
  else if (g_write_image_called != 1) fail("write_image", "expected one call");
  else ok("write_image");

  test_reset_replies();
  nv_arena_reset(arena);
  g_clear_called = 0;
  nv_op_clipboard_clear(win, 5, NULL, arena);
  if (!test_last_reply_ok()) fail("clear", "expected ok");
  else if (g_clear_called != 1) fail("clear", "expected one call");
  else ok("clear");

  test_reset_replies();
  nv_arena_reset(arena);
  g_has_text_called = 0;
  nv_op_clipboard_has_text(win, 6, NULL, arena);
  if (!test_last_reply_ok()) fail("has_text", "expected ok");
  else if (g_has_text_called != 1) fail("has_text", "expected one call");
  else ok("has_text");

  test_reset_replies();
  nv_arena_reset(arena);
  g_has_image_called = 0;
  nv_op_clipboard_has_image(win, 7, NULL, arena);
  if (!test_last_reply_ok()) {
    fail("has_image", "expected ok");
  } else if (!test_last_reply_json() || strstr(test_last_reply_json(), "true") == NULL) {
    fail("has_image", "expected true");
  } else if (g_has_image_called != 1) {
    fail("has_image", "expected one call");
  } else ok("has_image");

  test_reset_replies();
  nv_arena_reset(arena);
  g_read_text_called = 0;
  app.platform_api.clipboard_read_text = NULL;
  nv_op_clipboard_read_text(win, 10, NULL, arena);
  expect_err_not_supported("read_text not supported");
  if (g_read_text_called != 0) fail("read_text not supported", "unexpected call");
  app.platform_api.clipboard_read_text = test_clipboard_read_text;

  test_reset_replies();
  nv_arena_reset(arena);
  g_write_text_called = 0;
  app.platform_api.clipboard_write_text = NULL;
  nv_op_clipboard_write_text(win, 11, parse(arena, "{\"text\":\"x\"}"), arena);
  expect_err_not_supported("write_text not supported");
  if (g_write_text_called != 0) fail("write_text not supported", "unexpected call");
  app.platform_api.clipboard_write_text = test_clipboard_write_text;

  test_reset_replies();
  nv_arena_reset(arena);
  g_read_image_called = 0;
  app.platform_api.clipboard_read_image = NULL;
  nv_op_clipboard_read_image(win, 12, NULL, arena);
  expect_err_not_supported("read_image not supported");
  if (g_read_image_called != 0) fail("read_image not supported", "unexpected call");
  app.platform_api.clipboard_read_image = test_clipboard_read_image;

  test_reset_replies();
  nv_arena_reset(arena);
  g_write_image_called = 0;
  app.platform_api.clipboard_write_image = NULL;
  {
    char payload[512];
    (void)snprintf(payload, sizeof payload, "{\"format\":\"png\",\"data\":\"%s\"}", k_stub_png_b64);
    nv_op_clipboard_write_image(win, 13, parse(arena, payload), arena);
  }
  expect_err_not_supported("write_image not supported");
  if (g_write_image_called != 0) fail("write_image not supported", "unexpected call");
  app.platform_api.clipboard_write_image = test_clipboard_write_image;

  test_reset_replies();
  nv_arena_reset(arena);
  g_clear_called = 0;
  app.platform_api.clipboard_clear = NULL;
  nv_op_clipboard_clear(win, 14, NULL, arena);
  expect_err_not_supported("clear not supported");
  if (g_clear_called != 0) fail("clear not supported", "unexpected call");
  app.platform_api.clipboard_clear = test_clipboard_clear;

  test_reset_replies();
  nv_arena_reset(arena);
  g_has_text_called = 0;
  app.platform_api.clipboard_has_text = NULL;
  nv_op_clipboard_has_text(win, 15, NULL, arena);
  expect_err_not_supported("has_text not supported");
  if (g_has_text_called != 0) fail("has_text not supported", "unexpected call");
  app.platform_api.clipboard_has_text = test_clipboard_has_text;

  test_reset_replies();
  nv_arena_reset(arena);
  g_has_image_called = 0;
  app.platform_api.clipboard_has_image = NULL;
  nv_op_clipboard_has_image(win, 16, NULL, arena);
  expect_err_not_supported("has_image not supported");
  if (g_has_image_called != 0) fail("has_image not supported", "unexpected call");
  app.platform_api.clipboard_has_image = test_clipboard_has_image;

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_write_text(win, 20, parse(arena, "{}"), arena);
  expect_err_invalid_arg("write_text missing text");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_write_text(win, 21, NULL, arena);
  expect_err_invalid_arg("write_text null args");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_write_image(win, 22, parse(arena, "{\"format\":\"jpg\",\"data\":\"x\"}"), arena);
  expect_err_invalid_arg("write_image bad format");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_clipboard_write_image(win, 23, parse(arena, "{\"format\":\"png\",\"data\":\"not-base64\"}"), arena);
  expect_err_invalid_arg("write_image invalid base64");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
  printf("=== nativeview Clipboard Ops Tests ===\n\n");
  test_clipboard_ops();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
