/* =============================================================================
 * test_op_app.c - App Ops Handler Tests (handshake)
 * =============================================================================
 */
#include "nv_op_app.h"
#include "nv_window_internal.h"
#include "nv_core_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include "nv_ipc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

static char* dup_cstr(const char* s) {
  size_t n = strlen(s) + 1;
  char* out = (char*)malloc(n);
  if (!out) return NULL;
  memcpy(out, s, n);
  return out;
}

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name){ printf("✓ %s\n", name); tests_passed++; }
static void fail(const char* name, const char* why){ printf("✗ %s: %s\n", name, why); tests_failed++; }

static nv_window_t* make_window(nv_app_t* app) {
  nv_window_cfg_t cfg = {
    .title = "AppOpsTest",
    .width = 640,
    .height = 480,
    .min_width = 0,
    .min_height = 0,
    .resizable = 1,
    .frameless = 0,
    .transparent = 0,
    .devtools = 0
  };
  return nv_window_alloc(app, &cfg);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s){
  return nv_json_parse(arena, s ? s : "null");
}

static void test_handshake_match_and_mismatch(void) {
  nv_window_t* win = make_window(NULL);
  if (!win) { fail("make_window", "nv_window_alloc failed"); return; }
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) { fail("arena", "nv_arena_create failed"); nv_window_free(win); return; }

  /* match */
  test_reset_replies();
  nv_arena_reset(arena);
  char buf[128];
  snprintf(buf, sizeof(buf), "{\"jsVersion\":\"%s\",\"wireVersion\":%d}", "0.1.0", NV_WIRE_VERSION);
  nv_json_val_t* args_ok = parse(arena, buf);
  nv_op_app_handshake(win, 1, args_ok, arena);
  if (!test_last_reply_ok()) {
    fail("handshake match", "expected ok");
  } else {
    const char* js = test_last_reply_json();
    nv_arena_t* pa = nv_arena_create(1024);
    nv_json_val_t* v = parse(pa, js ? js : "null");
    const char* cv = nv_json_get_str(v, "cVersion");
    long long wv = nv_json_get_int(v, "wireVersion");
    const char* win_id = nv_json_get_str(v, "windowId");

    if (!cv || cv[0] == '\0') fail("handshake match payload", "missing cVersion");
    else if (wv != NV_WIRE_VERSION) fail("handshake match payload", "wireVersion mismatch");
    else if (!win_id || strcmp(win_id, "main") != 0) fail("handshake match payload", "missing or incorrect windowId (expected 'main')");
    else ok("handshake match payload");
    nv_arena_destroy(pa);
  }

  /* mismatch */
  test_reset_replies();
  nv_arena_reset(arena);
  snprintf(buf, sizeof(buf), "{\"jsVersion\":\"%s\",\"wireVersion\":%d}", "0.1.0", NV_WIRE_VERSION + 1);
  nv_json_val_t* args_bad = parse(arena, buf);
  nv_op_app_handshake(win, 2, args_bad, arena);
  if (test_last_reply_ok()) fail("handshake mismatch", "expected error");
  else if (!test_last_reply_error_code() || strcmp(test_last_reply_error_code(),"ERR_VERSION_MISMATCH")!=0)
    fail("handshake mismatch", "wrong error code");
  else ok("handshake mismatch");

  nv_arena_destroy(arena);
  nv_window_free(win);
}

static int json_field_nonempty(const char* json, const char* key) {
  if (!json || !key) return 0;
  nv_arena_t* a = nv_arena_create(2048);
  if (!a) return 0;
  nv_json_val_t* v = nv_json_parse(a, json);
  const char* s = v ? nv_json_get_str(v, key) : NULL;
  int ok = (s && s[0] != '\0') ? 1 : 0;
  nv_arena_destroy(a);
  return ok;
}

static char* test_get_data_dir(void) { return dup_cstr("/tmp/nativeview_data"); }
static char* test_get_exe_path(void) { return dup_cstr("/tmp/nativeview.exe"); }
static char* test_get_resource_dir(void) { return dup_cstr("/tmp"); }
static char* test_get_locale(void) { return dup_cstr("en-GB"); }
static void test_app_paths_and_locale(void) {
  nv_app_t app;
  memset(&app, 0, sizeof app);
  app.platform_api.platform_name = "test";
  app.platform_api.app_get_data_dir = test_get_data_dir;
  app.platform_api.app_get_exe_path = test_get_exe_path;
  app.platform_api.app_get_resource_dir = test_get_resource_dir;
  app.platform_api.app_get_locale = test_get_locale;

  nv_window_t* win = make_window(&app);
  if (!win) {
    fail("app paths", "nv_window_alloc failed");
    return;
  }
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    fail("app paths", "nv_arena_create failed");
    nv_window_free(win);
    return;
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_app_get_data_dir(win, 10, NULL, arena);
  if (!test_last_reply_ok() || !json_field_nonempty(test_last_reply_json(), "path")) {
    fail("app.getDataDir path", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    ok("app.getDataDir path");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_app_get_exe_path(win, 11, NULL, arena);
  if (!test_last_reply_ok() || !json_field_nonempty(test_last_reply_json(), "path")) {
    fail("app.getExePath path", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    ok("app.getExePath path");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_app_get_resource_dir(win, 12, NULL, arena);
  if (!test_last_reply_ok() || !json_field_nonempty(test_last_reply_json(), "path")) {
    fail("app.getResourceDir path", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    ok("app.getResourceDir path");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_app_get_locale(win, 13, NULL, arena);
  if (!test_last_reply_ok() || !json_field_nonempty(test_last_reply_json(), "locale")) {
    fail("app.getLocale locale", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    ok("app.getLocale locale");
  }

  test_reset_replies();
  nv_arena_reset(arena);
  nv_op_app_get_platform(win, 14, NULL, arena);
  if (!test_last_reply_ok() || !json_field_nonempty(test_last_reply_json(), "platform")) {
    fail("app.getPlatform", test_last_reply_json() ? test_last_reply_json() : "(null)");
  } else {
    ok("app.getPlatform");
  }

  nv_arena_destroy(arena);
  nv_window_free(win);
}

int main(void){
  printf("=== nativeview App Ops Tests ===\n\n");
  test_handshake_match_and_mismatch();
  test_app_paths_and_locale();
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
