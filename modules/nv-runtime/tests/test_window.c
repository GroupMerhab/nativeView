/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * test_window.c - Internal Window API Tests
 *
 * Focus: nv_window_alloc/free and internal accessors from nv_window_internal.h
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#include "nv_window_internal.h"
#include "nv_ipc_internal.h"
#include "nv_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =============================================================================
 * Test Infrastructure
 * =============================================================================
 */

static int tests_passed = 0;
static int tests_failed = 0;

static void test_ok(const char* name) {
  printf("✓ %s\n", name);
  tests_passed++;
}

static void test_fail(const char* name, const char* reason) {
  printf("✗ %s: %s\n", name, reason);
  tests_failed++;
}

/* =============================================================================
 * Test 1: alloc with valid config → all fields correct
 * =============================================================================
 */

static void test_window_alloc_valid_config(void) {
  nv_window_cfg_t cfg = {
    .title = "Internal Test",
    .width = 1024,
    .height = 768,
    .min_width = 320,
    .min_height = 240,
    .resizable = 1,
    .frameless = 0,
    .transparent = 0,
    .devtools = 0
  };

  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (!win) {
    test_fail("alloc_valid_config", "nv_window_alloc returned NULL");
    return;
  }

  if (win->app != NULL) {
    test_fail("alloc_valid_config", "app should be NULL for this test");
    nv_window_free(win);
    return;
  }

  if (strcmp(win->cfg.title, "Internal Test") != 0 ||
      win->cfg.width != 1024 || win->cfg.height != 768 ||
      win->cfg.min_width != 320 || win->cfg.min_height != 240 ||
      win->cfg.resizable != 1 || win->cfg.frameless != 0 ||
      win->cfg.transparent != 0 || win->cfg.devtools != 0) {
    test_fail("alloc_valid_config", "cfg fields not copied/validated correctly");
    nv_window_free(win);
    return;
  }

  if (!win->arena) {
    test_fail("alloc_valid_config", "arena was not created");
    nv_window_free(win);
    return;
  }

  if (!win->ipc) {
    test_fail("alloc_valid_config", "ipc state was not created");
    nv_window_free(win);
    return;
  }

  if (win->platform != NULL) {
    test_fail("alloc_valid_config", "platform handle should start as NULL");
    nv_window_free(win);
    return;
  }

  if (win->visible != 0) {
    test_fail("alloc_valid_config", "visible should default to 0 (hidden)");
    nv_window_free(win);
    return;
  }

  nv_window_free(win);
  test_ok("alloc_valid_config");
}

/* =============================================================================
 * Test 2: alloc with NULL title → defaults to "App"
 * =============================================================================
 */

static void test_window_alloc_null_title(void) {
  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = NULL;
  cfg.width = 800;
  cfg.height = 600;

  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (!win) {
    test_fail("alloc_null_title", "nv_window_alloc returned NULL");
    return;
  }

  if (strcmp(win->cfg.title, "App") != 0) {
    test_fail("alloc_null_title", "title not defaulted to \"App\"");
    nv_window_free(win);
    return;
  }

  nv_window_free(win);
  test_ok("alloc_null_title");
}

/* =============================================================================
 * Test 3: alloc with zero size → defaults to 800x600
 * =============================================================================
 */

static void test_window_alloc_zero_size(void) {
  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "Test";
  cfg.width = 0;
  cfg.height = 0;

  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (!win) {
    test_fail("alloc_zero_size", "nv_window_alloc returned NULL");
    return;
  }

  if (win->cfg.width != 800 || win->cfg.height != 600) {
    test_fail("alloc_zero_size", "dimensions not defaulted to 800x600");
    nv_window_free(win);
    return;
  }

  nv_window_free(win);
  test_ok("alloc_zero_size");
}

/* =============================================================================
 * Test 4: platform handle set/get roundtrip
 * =============================================================================
 */

static void test_window_platform_handle_roundtrip(void) {
  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "Platform";
  cfg.width = 800;
  cfg.height = 600;

  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (!win) {
    test_fail("platform_roundtrip", "nv_window_alloc returned NULL");
    return;
  }

  if (nv_window_get_platform(win) != NULL) {
    test_fail("platform_roundtrip", "initial platform handle should be NULL");
    nv_window_free(win);
    return;
  }

  void* fake_handle = (void*)0xDEADBEEF;
  nv_window_set_platform(win, fake_handle);

  if (nv_window_get_platform(win) != fake_handle) {
    test_fail("platform_roundtrip", "platform handle get/set mismatch");
    nv_window_free(win);
    return;
  }

  nv_window_free(win);
  test_ok("platform_roundtrip");
}

/* =============================================================================
 * Test 5: ipc state non-null after alloc
 * =============================================================================
 */

static void test_window_ipc_non_null(void) {
  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "IPC";
  cfg.width = 800;
  cfg.height = 600;

  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (!win) {
    test_fail("ipc_non_null", "nv_window_alloc returned NULL");
    return;
  }

  nv_ipc_state_t* ipc = nv_window_get_ipc(win);
  if (!ipc) {
    test_fail("ipc_non_null", "nv_window_get_ipc returned NULL");
    nv_window_free(win);
    return;
  }

  nv_window_free(win);
  test_ok("ipc_non_null");
}

/* =============================================================================
 * Test 6: free does not crash on valid pointer (and NULL-safe)
 * =============================================================================
 */

static void test_window_free_safety(void) {
  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "Free";
  cfg.width = 800;
  cfg.height = 600;

  nv_window_t* win = nv_window_alloc(NULL, &cfg);
  if (!win) {
    test_fail("free_safety", "nv_window_alloc returned NULL");
    return;
  }

  /* Free should not crash for valid pointer */
  nv_window_free(win);

  /* And must be NULL-safe */
  nv_window_free(NULL);

  test_ok("free_safety");
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void) {
  printf("=== nativeview Internal Window Tests ===\n\n");
  printf("Running tests...\n");

  test_window_alloc_valid_config();
  test_window_alloc_null_title();
  test_window_alloc_zero_size();
  test_window_platform_handle_roundtrip();
  test_window_ipc_non_null();
  test_window_free_safety();

  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);

  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
