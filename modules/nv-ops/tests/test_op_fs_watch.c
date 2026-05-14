/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "nv_op_fs.h"
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
  printf("✗ %s: %s\n", name, why ? why : "");
  tests_failed++;
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

static long long g_last_start_id = -1;
static char g_last_start_path[512];
static int g_start_calls = 0;
static int g_stop_calls = 0;

static int test_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  (void)w;
  g_start_calls++;
  g_last_start_id = id;
  if (path) {
    strncpy(g_last_start_path, path, sizeof(g_last_start_path) - 1);
    g_last_start_path[sizeof(g_last_start_path) - 1] = '\0';
  } else {
    g_last_start_path[0] = '\0';
  }
  return 0;
}

static void test_fs_watch_stop(long long id) {
  (void)id;
  g_stop_calls++;
}

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = {"FsWatchTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
  return nv_json_parse(arena, s ? s : "null");
}

static void expect_ok(const char* name) {
  if (!test_last_reply_ok()) {
    fail(name, test_last_reply_json() ? test_last_reply_json() : "(null reply)");
    return;
  }
  ok(name);
}

static void test_fs_watch_hooks(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.fs_watch_start = test_fs_watch_start;
  app.platform_api.fs_watch_stop = test_fs_watch_stop;

  nv_window_t* win = make_window(&app);
  if (!win) {
    fail("make_window", "alloc failed");
    return;
  }
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    fail("arena", "alloc failed");
    nv_window_free(win);
    return;
  }

  g_start_calls = 0;
  g_stop_calls = 0;
  g_last_start_id = -1;
  g_last_start_path[0] = '\0';

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 1, parse(arena, "{\"id\":7,\"path\":\"/tmp\"}"), arena);
  expect_ok("watch reply ok");
  if (g_start_calls != 1) fail("watch start calls", "expected 1");
  else if (g_last_start_id != 7) fail("watch start id", "wrong id");
  else if (strcmp(g_last_start_path, "/tmp") != 0) fail("watch start path", g_last_start_path);
  else ok("watch_start dispatch");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_unwatch(win, 2, parse(arena, "{\"id\":7}"), arena);
  expect_ok("unwatch reply ok");
  if (g_stop_calls != 1) fail("watch stop calls", "expected 1");
  else ok("watch_stop dispatch");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 3, parse(arena, "{\"id\":0,\"path\":\"/tmp\"}"), arena);
  expect_err("invalid id rejected", "ERR_INVALID_ARG");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 4, parse(arena, "{\"id\":1,\"path\":\"../bad\"}"), arena);
  expect_err("traversal rejected", "ERR_PERMISSION");

  app.platform_api.fs_watch_start = NULL;
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 5, parse(arena, "{\"id\":2,\"path\":\"/tmp\"}"), arena);
  expect_err("watch not supported", "ERR_NOT_SUPPORTED");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void) {
  printf("=== nativeview fs.watch op tests ===\n\n");
  test_fs_watch_hooks();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
