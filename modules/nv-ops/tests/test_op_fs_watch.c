/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "nv_op_fs.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

#if defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
static long long g_mac_last_start_id = -1;
static char g_mac_last_start_path[512];
static int g_mac_start_calls = 0;
static int g_mac_stop_calls = 0;
int nv_mac_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  (void)w;
  g_mac_start_calls++;
  g_mac_last_start_id = id;
  if (path) {
    strncpy(g_mac_last_start_path, path, sizeof(g_mac_last_start_path) - 1);
    g_mac_last_start_path[sizeof(g_mac_last_start_path) - 1] = '\0';
  } else {
    g_mac_last_start_path[0] = '\0';
  }
  return 0;
}
void nv_mac_fs_watch_stop(long long id) {
  (void)id;
  g_mac_stop_calls++;
}
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && !defined(__APPLE__)
static long long g_linux_last_start_id = -1;
static char g_linux_last_start_path[512];
static int g_linux_start_calls = 0;
static int g_linux_stop_calls = 0;
int nv_linux_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  (void)w;
  g_linux_start_calls++;
  g_linux_last_start_id = id;
  if (path) {
    strncpy(g_linux_last_start_path, path, sizeof(g_linux_last_start_path) - 1);
    g_linux_last_start_path[sizeof(g_linux_last_start_path) - 1] = '\0';
  } else {
    g_linux_last_start_path[0] = '\0';
  }
  return 0;
}
void nv_linux_fs_watch_stop(long long id) {
  (void)id;
  g_linux_stop_calls++;
}
#elif defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
static long long g_win_last_start_id = -1;
static char g_win_last_start_path[512];
static int g_win_start_calls = 0;
static int g_win_stop_calls = 0;
int nv_win_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  (void)w;
  g_win_start_calls++;
  g_win_last_start_id = id;
  if (path) {
    strncpy(g_win_last_start_path, path, sizeof(g_win_last_start_path) - 1);
    g_win_last_start_path[sizeof(g_win_last_start_path) - 1] = '\0';
  } else {
    g_win_last_start_path[0] = '\0';
  }
  return 0;
}
void nv_win_fs_watch_stop(long long id) {
  (void)id;
  g_win_stop_calls++;
}
#endif

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name) {
  printf("✓ %s\n", name);
  tests_passed++;
}
static void fail(const char* name, const char* why) {
  printf("✗ %s: %s\n", name, why ? why : "");
  tests_failed++;
}

static nv_window_t* make_window(void) {
  nv_window_cfg_t cfg = {"FsWatchTest", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(NULL, &cfg);
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
  nv_window_t* win = make_window();
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

#if defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
  g_mac_start_calls = 0;
  g_mac_stop_calls = 0;
  g_mac_last_start_id = -1;
  g_mac_last_start_path[0] = '\0';

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 1, parse(arena, "{\"id\":7,\"path\":\"/tmp\"}"), arena);
  expect_ok("watch reply ok");
  if (g_mac_start_calls != 1) fail("mac start calls", "expected 1");
  else if (g_mac_last_start_id != 7) fail("mac start id", "wrong id");
  else if (strcmp(g_mac_last_start_path, "/tmp") != 0) fail("mac start path", g_mac_last_start_path);
  else ok("mac watch_start hook");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_unwatch(win, 2, parse(arena, "{\"id\":7}"), arena);
  expect_ok("unwatch reply ok");
  if (g_mac_stop_calls != 1) fail("mac stop calls", "expected 1");
  else ok("mac watch_stop hook");

#elif (defined(__GNUC__) || defined(__clang__)) && defined(__linux__) && !defined(__APPLE__)
  g_linux_start_calls = 0;
  g_linux_stop_calls = 0;
  g_linux_last_start_id = -1;
  g_linux_last_start_path[0] = '\0';

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 1, parse(arena, "{\"id\":9,\"path\":\"/tmp\"}"), arena);
  expect_ok("watch reply ok");
  if (g_linux_start_calls != 1) fail("linux start calls", "expected 1");
  else if (g_linux_last_start_id != 9) fail("linux start id", "wrong id");
  else if (strcmp(g_linux_last_start_path, "/tmp") != 0)
    fail("linux start path", g_linux_last_start_path);
  else ok("linux watch_start hook");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_unwatch(win, 2, parse(arena, "{\"id\":9}"), arena);
  expect_ok("unwatch reply ok");
  if (g_linux_stop_calls != 1) fail("linux stop calls", "expected 1");
  else ok("linux watch_stop hook");

#elif defined(_WIN32) && (defined(__GNUC__) || defined(__clang__))
  g_win_start_calls = 0;
  g_win_stop_calls = 0;
  g_win_last_start_id = -1;
  g_win_last_start_path[0] = '\0';

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 1, parse(arena, "{\"id\":3,\"path\":\"C:\\\\Windows\\\\Temp\"}"), arena);
  expect_ok("watch reply ok");
  if (g_win_start_calls != 1) fail("win start calls", "expected 1");
  else if (g_win_last_start_id != 3) fail("win start id", "wrong id");
  else ok("win watch_start hook");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_unwatch(win, 2, parse(arena, "{\"id\":3}"), arena);
  expect_ok("unwatch reply ok");
  if (g_win_stop_calls != 1) fail("win stop calls", "expected 1");
  else ok("win watch_stop hook");

#else
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 1, parse(arena, "{\"id\":0,\"path\":\"/tmp\"}"), arena);
  if (test_last_reply_ok()) fail("invalid id", "expected error");
  else ok("invalid id rejected");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_fs_watch(win, 2, parse(arena, "{\"id\":1,\"path\":\"../bad\"}"), arena);
  if (test_last_reply_ok()) fail("traversal", "expected error");
  else ok("traversal rejected");
#endif

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
