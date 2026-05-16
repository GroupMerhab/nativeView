/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#include "nv_op_dialog.h"
#include "nv_window_internal.h"
#include "nv_core_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name){ printf("✓ %s\n", name); tests_passed++; }
static void fail(const char* name, const char* why){ printf("✗ %s: %s\n", name, why); tests_failed++; }

static void expect_err(const char* name, const char* code) {
  if (test_last_reply_ok()) { fail(name, "expected error reply"); return; }
  if (!test_last_reply_json_has_error_key()) { fail(name, "expected JSON with \"error\" key"); return; }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), code) != 0) {
    fail(name, "wrong error code");
    return;
  }
  ok(name);
}

static void test_dialog_open_file_async(int allow_multiple, nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb) {
  (void)allow_multiple;
  if (ctx && cb) cb(ctx, 1, NULL);
}
static void test_dialog_save_file_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb) {
  if (ctx && cb) cb(ctx, 1, NULL);
}
static void test_dialog_open_folder_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb) {
  if (ctx && cb) cb(ctx, 1, NULL);
}
static void test_dialog_message_async(const char* title, const char* body, const char* type,
                                      const char** buttons, size_t btn_count, nv_dialog_ctx_t* ctx,
                                      nv_dialog_cb_t cb) {
  (void)title;
  (void)body;
  (void)type;
  (void)buttons;
  (void)btn_count;
  if (ctx && cb) {
    int* idx = (int*)malloc(sizeof(int));
    if (idx) *idx = 0;
    cb(ctx, 0, (void*)idx);
  }
}
static void test_dialog_confirm_async(const char* title, const char* body, nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb) {
  (void)title;
  (void)body;
  if (ctx && cb) {
    int* v = (int*)malloc(sizeof(int));
    if (v) *v = 0;
    cb(ctx, 0, (void*)v);
  }
}

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = { "DialogTest", 640, 480, 0, 0, 1, 0, 0, 0, 0 };
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s){
  return nv_json_parse(arena, s ? s : "null");
}

static void expect_invalid_args(const char* name) {
  if (test_last_reply_ok()) { fail(name, "expected error reply"); return; }
  if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_INVALID_ARGS") != 0) {
    fail(name, "expected ERR_INVALID_ARGS");
    return;
  }
  ok(name);
}

static void test_dialog_ops(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.dialog_open_file_async = test_dialog_open_file_async;
  app.platform_api.dialog_save_file_async = test_dialog_save_file_async;
  app.platform_api.dialog_open_folder_async = test_dialog_open_folder_async;
  app.platform_api.dialog_message_async = test_dialog_message_async;
  app.platform_api.dialog_confirm_async = test_dialog_confirm_async;

  nv_window_t* win = make_window(&app);
  if (!win) { fail("make_window", "alloc failed"); return; }
  nv_arena_t* arena = nv_arena_create(65536);
  if (!arena) { fail("arena", "alloc failed"); nv_window_free(win); return; }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_open_file(win, 1, NULL, arena);
  if (!test_last_reply_ok()) fail("open_file", "expected ok"); else ok("open_file");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_save_file(win, 2, NULL, arena);
  if (!test_last_reply_ok()) fail("save_file", "expected ok"); else ok("save_file");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_open_folder(win, 3, NULL, arena);
  if (!test_last_reply_ok()) fail("open_folder", "expected ok"); else ok("open_folder");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_message(win, 4, parse(arena, "{\"title\":\"t\",\"body\":\"m\"}"), arena);
  if (!test_last_reply_ok()) fail("message", "expected ok"); else ok("message");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_confirm(win, 5, parse(arena, "{\"title\":\"t\",\"body\":\"b\"}"), arena);
  if (!test_last_reply_ok()) fail("confirm", "expected ok"); else ok("confirm");

  app.platform_api.dialog_open_file_async = NULL;
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_open_file(win, 10, NULL, arena);
  expect_err("open_file not supported", "ERR_NOT_SUPPORTED");
  app.platform_api.dialog_open_file_async = test_dialog_open_file_async;

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_message(win, 6, parse(arena, "{\"title\":\"only\"}"), arena);
  expect_invalid_args("message_missing_body");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_message(win, 7, parse(arena, "{\"body\":\"only\"}"), arena);
  expect_invalid_args("message_missing_title");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_confirm(win, 8, parse(arena, "{\"title\":\"only\"}"), arena);
  expect_invalid_args("confirm_missing_body");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_dialog_confirm(win, 9, parse(arena, "{\"body\":\"only\"}"), arena);
  expect_invalid_args("confirm_missing_title");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void){
  printf("=== nativeview Dialog Ops Tests ===\n\n");
  test_dialog_ops();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
