#include "nv_op_shell.h"
#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
void nv_shell_test_hook_open_url(void);
void nv_shell_test_hook_open_path(void);
void nv_shell_test_hook_show_in_folder(void);
static int g_hook_open_url;
static int g_hook_open_path;
static int g_hook_show_in_folder;
void nv_shell_test_hook_open_url(void) { g_hook_open_url++; }
void nv_shell_test_hook_open_path(void) { g_hook_open_path++; }
void nv_shell_test_hook_show_in_folder(void) { g_hook_show_in_folder++; }
#endif

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name){ printf("✓ %s\n", name); tests_passed++; }
static void fail(const char* name, const char* why){ printf("✗ %s: %s\n", name, why); tests_failed++; }

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

static int g_open_url_called;
static int test_shell_open_url(const char* url) {
  (void)url;
  g_open_url_called++;
  return 0;
}

static int g_open_path_called;
static int test_shell_open_path(const char* path) {
  (void)path;
  g_open_path_called++;
  return 0;
}

static int g_show_in_folder_called;
static int test_shell_show_in_folder(const char* path) {
  (void)path;
  g_show_in_folder_called++;
  return 0;
}

static char* dup_cstr(const char* s) {
  size_t n = s ? strlen(s) : 0;
  char* out = (char*)malloc(n + 1);
  if (!out) return NULL;
  if (n) memcpy(out, s, n);
  out[n] = '\0';
  return out;
}

static int g_shell_exec_called;
static nv_shell_result_t* test_shell_exec(const char* cmd) {
  (void)cmd;
  g_shell_exec_called++;
  nv_shell_result_t* r = (nv_shell_result_t*)malloc(sizeof(nv_shell_result_t));
  if (!r) return NULL;
  r->exit_code = 0;
  r->out = dup_cstr("hello\n");
  r->err = dup_cstr("");
  r->truncated = 0;
  if (!r->out || !r->err) {
    free(r->out);
    free(r->err);
    free(r);
    return NULL;
  }
  return r;
}

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = { "ShellTest", 640, 480, 0, 0, 1, 0, 0, 0, 0 };
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s){
  return nv_json_parse(arena, s ? s : "null");
}

static void test_op_shell_exec(nv_window_t* win, nv_arena_t* arena) {
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_shell_exec(win, 50, parse(arena, "{}"), arena);
  expect_err_invalid_arg("exec missing cmd");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_shell_exec(win, 51, NULL, arena);
  expect_err_invalid_arg("exec null args");

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_shell_exec(win, 52, parse(arena, "{\"cmd\":42}"), arena);
  expect_err_invalid_arg("exec cmd not string");

#ifndef NV_ALLOW_SHELL_EXEC
  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_shell_exec(win, 53, parse(arena, "{\"cmd\":\"echo a; rm -rf /\"}"), arena);
  if (test_last_reply_ok()) fail("exec sanitize", "expected ERR_PERMISSION");
  else if (!test_last_reply_json_has_error_key()) fail("exec sanitize", "expected JSON with \"error\" key");
  else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(), "ERR_PERMISSION") != 0)
    fail("exec sanitize", "wrong error code");
  else ok("exec sanitize");
#endif

  {
    nv_app_t* app = win ? win->app : NULL;
    if (app) app->platform_api.shell_exec = NULL;
    test_reset_replies();
    nv_arena_reset(arena);
    nv_op_shell_exec(win, 54, parse(arena, "{\"cmd\":\"echo hello\"}"), arena);
    expect_err_not_supported("exec not supported");
    if (app) app->platform_api.shell_exec = test_shell_exec;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_shell_exec(win, 54, parse(arena, "{\"cmd\":\"echo hello\"}"), arena);
  if (!test_last_reply_ok()) {
    fail("exec echo hello", "expected ok reply");
    return;
  }
  {
    const char* js = test_last_reply_json();
    nv_json_val_t* res = js ? nv_json_parse(arena, js) : NULL;
    if (!res) {
      fail("exec echo hello", "expected JSON result");
      return;
    }
    if (nv_json_get_int(res, "exitCode") != 0) {
      fail("exec echo hello", "expected exitCode 0");
      return;
    }
    {
      const char* so = nv_json_get_str(res, "stdout");
      if (!so || strstr(so, "hello") == NULL) {
        fail("exec echo hello", "stdout missing hello");
        return;
      }
    }
    ok("exec echo hello");
  }
}

static void test_shell_ops(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.shell_open_url = test_shell_open_url;
  app.platform_api.shell_open_path = test_shell_open_path;
  app.platform_api.shell_show_in_folder = test_shell_show_in_folder;
  app.platform_api.shell_exec = test_shell_exec;

  nv_window_t* win = make_window(&app);
  if (!win) { fail("make_window", "alloc failed"); return; }
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) { fail("arena", "alloc failed"); nv_window_free(win); return; }

  test_reset_replies();
  nv_arena_reset(arena);
  g_open_url_called = 0;
  nv_op_shell_open_url(win, 1, parse(arena, "{\"url\":\"https://example.com\"}"), arena);
  if (!test_last_reply_ok()) fail("open_url", "expected ok");
  else if (g_open_url_called != 1) fail("open_url", "expected one call");
  else ok("open_url");

  test_reset_replies();
  nv_arena_reset(arena);
  g_open_url_called = 0;
  app.platform_api.shell_open_url = NULL;
  nv_op_shell_open_url(win, 2, parse(arena, "{\"url\":\"https://example.com\"}"), arena);
  expect_err_not_supported("open_url not supported");
  if (g_open_url_called != 0) fail("open_url not supported", "unexpected call");
  app.platform_api.shell_open_url = test_shell_open_url;

  test_reset_replies();
  nv_arena_reset(arena);
  g_open_path_called = 0;
  nv_op_shell_open_path(win, 3, parse(arena, "{\"path\":\"/tmp\"}"), arena);
  if (!test_last_reply_ok()) fail("open_path", "expected ok");
  else if (g_open_path_called != 1) fail("open_path", "expected one call");
  else ok("open_path");

  test_reset_replies();
  nv_arena_reset(arena);
  g_show_in_folder_called = 0;
  nv_op_shell_show_in_folder(win, 4, parse(arena, "{\"path\":\"/tmp/file\"}"), arena);
  if (!test_last_reply_ok()) fail("show_in_folder", "expected ok");
  else if (g_show_in_folder_called != 1) fail("show_in_folder", "expected one call");
  else ok("show_in_folder");

  g_shell_exec_called = 0;
  test_op_shell_exec(win, arena);
  if (g_shell_exec_called == 0) {
    fail("shell_exec dispatch", "expected at least one call");
  }

#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  /* Valid payloads reach the platform dispatch path (strong hook overrides weak default in nv_op_shell). */
  g_hook_open_url = g_hook_open_path = g_hook_show_in_folder = 0;
  test_reset_replies();
  nv_arena_reset(arena);
  g_open_url_called = 0;
  nv_op_shell_open_url(win, 10, parse(arena, "{\"url\":\"https://example.org\"}"), arena);
  if (g_hook_open_url != 1) fail("open_url dispatches hook", "expected hook once");
  else if (!test_last_reply_ok()) fail("open_url dispatches hook", "expected ok");
  else if (g_open_url_called != 1) fail("open_url dispatches hook", "expected one call");
  else ok("open_url dispatches hook");

  g_hook_open_url = g_hook_open_path = g_hook_show_in_folder = 0;
  test_reset_replies();
  nv_arena_reset(arena);
  g_open_path_called = 0;
  nv_op_shell_open_path(win, 11, parse(arena, "{\"path\":\"/tmp\"}"), arena);
  if (g_hook_open_path != 1) fail("open_path dispatches hook", "expected hook once");
  else if (!test_last_reply_ok()) fail("open_path dispatches hook", "expected ok");
  else if (g_open_path_called != 1) fail("open_path dispatches hook", "expected one call");
  else ok("open_path dispatches hook");

  g_hook_open_url = g_hook_open_path = g_hook_show_in_folder = 0;
  test_reset_replies();
  nv_arena_reset(arena);
  g_show_in_folder_called = 0;
  nv_op_shell_show_in_folder(win, 12, parse(arena, "{\"path\":\"/tmp/file\"}"), arena);
  if (g_hook_show_in_folder != 1) fail("show_in_folder dispatches hook", "expected hook once");
  else if (!test_last_reply_ok()) fail("show_in_folder dispatches hook", "expected ok");
  else if (g_show_in_folder_called != 1) fail("show_in_folder dispatches hook", "expected one call");
  else ok("show_in_folder dispatches hook");
#endif

  /* Invalid arguments: ERR_INVALID_ARG and no platform hook (GCC/Clang macOS/Windows). */
  test_reset_replies();
  nv_arena_reset(arena);
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  g_hook_open_url = 0;
#endif
  g_open_url_called = 0;
  nv_op_shell_open_url(win, 20, parse(arena, "{}"), arena);
  expect_err_invalid_arg("open_url missing url");
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  if (g_hook_open_url != 0) fail("open_url missing url (hook)", "hook should not run");
#endif
  if (g_open_url_called != 0) fail("open_url missing url", "should not call handler");

  test_reset_replies();
  nv_arena_reset(arena);
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  g_hook_open_url = 0;
#endif
  g_open_url_called = 0;
  nv_op_shell_open_url(win, 21, parse(arena, "{\"url\":42}"), arena);
  expect_err_invalid_arg("open_url non-string url");
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  if (g_hook_open_url != 0) fail("open_url non-string (hook)", "hook should not run");
#endif
  if (g_open_url_called != 0) fail("open_url non-string url", "should not call handler");

  test_reset_replies();
  nv_arena_reset(arena);
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  g_hook_open_url = 0;
#endif
  g_open_url_called = 0;
  nv_op_shell_open_url(win, 22, NULL, arena);
  expect_err_invalid_arg("open_url null args");
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  if (g_hook_open_url != 0) fail("open_url null args (hook)", "hook should not run");
#endif
  if (g_open_url_called != 0) fail("open_url null args", "should not call handler");

  test_reset_replies();
  nv_arena_reset(arena);
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  g_hook_open_path = 0;
#endif
  nv_op_shell_open_path(win, 23, parse(arena, "{}"), arena);
  expect_err_invalid_arg("open_path missing path");
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  if (g_hook_open_path != 0) fail("open_path missing path (hook)", "hook should not run");
#endif

  test_reset_replies();
  nv_arena_reset(arena);
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  g_hook_show_in_folder = 0;
#endif
  nv_op_shell_show_in_folder(win, 24, parse(arena, "{}"), arena);
  expect_err_invalid_arg("show_in_folder missing path");
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__APPLE__) || defined(_WIN32) || defined(__linux__))
  if (g_hook_show_in_folder != 0) fail("show_in_folder missing path (hook)", "hook should not run");
#endif

  test_op_shell_exec(win, arena);

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void){
  printf("=== nativeview Shell Ops Tests ===\n\n");
  test_shell_ops();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
