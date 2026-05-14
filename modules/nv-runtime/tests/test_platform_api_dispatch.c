#include "nv_core_internal.h"
#include "nv_hotkey.h"
#include "nv_window_internal.h"
#include "nv_window_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_create_calls = 0;
static int g_destroy_calls = 0;
static int g_show_calls = 0;
static int g_hide_calls = 0;
static int g_set_title_calls = 0;
static int g_eval_calls = 0;
static int g_hotkey_reg_calls = 0;
static int g_hotkey_unreg_calls = 0;

static const char* g_last_title = NULL;
static const char* g_last_js = NULL;

static void t_window_create(nv_window_t* w) {
  g_create_calls++;
  nv_window_set_platform(w, (void*)0x1);
}

static void t_window_destroy(nv_window_t* w) {
  (void)w;
  g_destroy_calls++;
}

static void t_window_show(nv_window_t* w) {
  (void)w;
  g_show_calls++;
}

static void t_window_hide(nv_window_t* w) {
  (void)w;
  g_hide_calls++;
}

static void t_window_set_title(nv_window_t* w, const char* title) {
  (void)w;
  g_set_title_calls++;
  g_last_title = title;
}

static void t_window_eval_js(nv_window_t* w, const char* js) {
  (void)w;
  g_eval_calls++;
  g_last_js = js;
}

static int t_hotkey_register(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                             void (*cb)(long long handle, void* ctx), void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  g_hotkey_reg_calls++;
  return 0;
}

static void t_hotkey_unregister(long long handle) {
  (void)handle;
  g_hotkey_unreg_calls++;
}

static int tests_passed = 0;
static int tests_failed = 0;

static void ok(const char* name) {
  printf("PASS %s\n", name);
  tests_passed++;
}

static void fail(const char* name, const char* why) {
  printf("FAIL %s: %s\n", name, why);
  tests_failed++;
}

static void reset_counters(void) {
  g_create_calls = 0;
  g_destroy_calls = 0;
  g_show_calls = 0;
  g_hide_calls = 0;
  g_set_title_calls = 0;
  g_eval_calls = 0;
  g_hotkey_reg_calls = 0;
  g_hotkey_unreg_calls = 0;
  g_last_title = NULL;
  g_last_js = NULL;
}

static void test_window_dispatch(void) {
  reset_counters();

  nv_wm_init();

  nv_app_t* app = nv_app_alloc();
  if (!app) {
    fail("window_dispatch", "nv_app_alloc");
    return;
  }

  app->platform_api.window_create = t_window_create;
  app->platform_api.window_destroy = t_window_destroy;
  app->platform_api.window_show = t_window_show;
  app->platform_api.window_hide = t_window_hide;
  app->platform_api.window_set_title = t_window_set_title;
  app->platform_api.window_eval_js = t_window_eval_js;
  app->platform_api.hotkey_register = t_hotkey_register;
  app->platform_api.hotkey_unregister = t_hotkey_unregister;

  nv_window_cfg_t cfg = {
      .title = "DispatchTest",
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

  nv_window_t* w = nv_window_alloc(app, &cfg);
  if (!w) {
    nv_app_free(app);
    fail("window_dispatch", "nv_window_alloc");
    return;
  }
  if (g_create_calls != 1) {
    nv_window_destroy(w);
    nv_app_free(app);
    fail("window_dispatch", "expected window_create to be called once");
    return;
  }

  nv_window_show(w);
  nv_window_hide(w);
  if (g_show_calls != 1 || g_hide_calls != 1) {
    nv_window_destroy(w);
    nv_app_free(app);
    fail("window_dispatch", "show/hide dispatch mismatch");
    return;
  }

  nv_window_set_title(w, "NewTitle");
  if (g_set_title_calls != 1 || !g_last_title || strcmp(g_last_title, "NewTitle") != 0) {
    nv_window_destroy(w);
    nv_app_free(app);
    fail("window_dispatch", "set_title dispatch mismatch");
    return;
  }

  nv_eval_js(w, "1+1");
  if (g_eval_calls != 1 || !g_last_js || strcmp(g_last_js, "1+1") != 0) {
    nv_window_destroy(w);
    nv_app_free(app);
    fail("window_dispatch", "eval_js dispatch mismatch");
    return;
  }

  int hk_rc = nv_window_register_hotkey(w, "hk1", "CmdOrCtrl+Shift+K");
  if (hk_rc != NV_HOTKEY_OK || g_hotkey_reg_calls != 1) {
    nv_window_destroy(w);
    nv_app_free(app);
    fail("window_dispatch", "hotkey register dispatch mismatch");
    return;
  }

  nv_window_unregister_hotkey(w, "hk1");
  if (g_hotkey_unreg_calls != 1) {
    nv_window_destroy(w);
    nv_app_free(app);
    fail("window_dispatch", "hotkey unregister dispatch mismatch");
    return;
  }

  app->platform_api.hotkey_register = NULL;
  int hk_ns = nv_window_register_hotkey(w, "hk2", "CmdOrCtrl+Shift+J");
  if (hk_ns != NV_HOTKEY_ERR_NOT_SUPPORTED) {
    nv_window_destroy(w);
    nv_app_free(app);
    fail("window_dispatch", "expected hotkey not supported when hook is missing");
    return;
  }

  nv_window_destroy(w);
  if (g_destroy_calls != 1) {
    nv_app_free(app);
    fail("window_dispatch", "expected window_destroy to be called once");
    return;
  }

  nv_app_free(app);
  ok("window_dispatch");
}

int main(void) {
  test_window_dispatch();
  printf("%d passed, %d failed\n", tests_passed, tests_failed);
  return tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

