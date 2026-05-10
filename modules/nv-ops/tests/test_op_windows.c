/* =============================================================================
 * test_op_windows.c - Windows Ops Tests
 * =============================================================================
 */
#include "nv.h"
#include "nv_op_windows.h"
#include "nv_window_manager.h"
#include "nv_window_internal.h"
#include "nv_arena.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "test_helpers.h"

// We need to initialize the window manager manually since we don't have an app
NV_INTERNAL void nv_wm_init(void);

static nv_app_t* g_app = NULL;

static int tests_passed = 0, tests_failed = 0;
static void ok(const char* name) { printf("✓ %s\n", name); tests_passed++; }
static void fail(const char* name, const char* why) { 
    printf("✗ %s: %s\n", name, why); 
    tests_failed++; 
}

static nv_window_t* make_window(void) {
    nv_window_cfg_t cfg = {
        .title = "WindowsOpsTest",
        .width = 800,
        .height = 600,
        .resizable = 1
    };
    return nv_window_alloc(g_app, &cfg);
}

static void register_test_window(nv_window_t* win, const char* id) {
    const char* old_id = nv_wm_get_id_by_window(win);
    if (old_id) {
        nv_wm_unregister(old_id);
    }
    nv_wm_register(id, win);
}

static nv_json_val_t* parse(nv_arena_t* arena, const char* s) {
    return nv_json_parse(arena, s ? s : "null");
}

static void test_op_windows_open(void) {
    nv_wm_init();
    nv_window_t* sender = make_window();
    nv_arena_t* arena = nv_arena_create(4096);
    
    // 1. Open with valid args
    test_reset_replies();
    nv_json_val_t* args1 = parse(arena, "{\"id\":\"win1\",\"url\":\"https://example.com\",\"title\":\"Test Window\"}");
    nv_op_windows_open(sender, 1, args1, arena);
    
    if (!test_last_reply_ok()) {
        fail("open valid", "expected OK reply");
    } else {
        ok("open valid");
        // Verify created
        if (nv_wm_get_by_id("win1") == NULL) fail("open valid", "window not in registry");
    }
    
    // 2. Open with duplicate id
    test_reset_replies();
    nv_op_windows_open(sender, 2, args1, arena);
    if (test_last_reply_ok()) {
        fail("open duplicate", "expected error");
    } else if (strcmp(test_last_reply_error_code(), "ERR_ALREADY_EXISTS") != 0) {
        fail("open duplicate", "wrong error code");
    } else {
        ok("open duplicate");
    }
    
    // 3. Open with invalid id
    test_reset_replies();
    nv_json_val_t* args3 = parse(arena, "{\"id\":\"invalid id!\",\"url\":\"https://example.com\"}");
    nv_op_windows_open(sender, 3, args3, arena);
    if (test_last_reply_ok()) {
        fail("open invalid id", "expected error");
    } else if (strcmp(test_last_reply_error_code(), "ERR_INVALID_ARG") != 0) {
        fail("open invalid id", "wrong error code");
    } else {
        ok("open invalid id");
    }

    // Cleanup
    nv_window_t* win1 = nv_wm_get_by_id("win1");
    if (win1) {
        nv_window_destroy(win1);
    }
    nv_arena_destroy(arena);
    nv_window_destroy(sender);
}

static void test_op_windows_close(void) {
    nv_wm_init();
    nv_window_t* sender = make_window();
    nv_arena_t* arena = nv_arena_create(4096);
    
    // Create a dummy window to close
    nv_window_cfg_t cfg = {0};
    nv_window_t* win_to_close = nv_window_alloc(g_app, &cfg);
    register_test_window(win_to_close, "win_close_test");
    
    // 4. Close existing
    test_reset_replies();
    nv_json_val_t* args4 = parse(arena, "{\"id\":\"win_close_test\"}");
    nv_op_windows_close(sender, 4, args4, arena);
    
    if (!test_last_reply_ok()) {
        fail("close existing", "expected OK reply");
    } else {
        if (nv_wm_get_by_id("win_close_test") != NULL) fail("close existing", "window still in registry");
        else ok("close existing");
    }
    
    // 5. Close unknown
    test_reset_replies();
    nv_json_val_t* args5 = parse(arena, "{\"id\":\"unknown_win\"}");
    nv_op_windows_close(sender, 5, args5, arena);
    
    if (test_last_reply_ok()) {
        fail("close unknown", "expected error");
    } else if (strcmp(test_last_reply_error_code(), "ERR_NOT_FOUND") != 0) {
        fail("close unknown", "wrong error code");
    } else {
        ok("close unknown");
    }
    
    nv_arena_destroy(arena);
    nv_window_destroy(sender);
}

static void test_op_windows_focus(void) {
    nv_wm_init();
    nv_window_t* sender = make_window();
    nv_arena_t* arena = nv_arena_create(4096);
    
    // Create a dummy window
    nv_window_cfg_t cfg = {0};
    nv_window_t* win = nv_window_alloc(g_app, &cfg);
    register_test_window(win, "win_focus_test");
    
    test_reset_replies();
    nv_json_val_t* args = parse(arena, "{\"id\":\"win_focus_test\"}");
    nv_op_windows_focus(sender, 1, args, arena);
    
    if (!test_last_reply_ok()) {
        fail("focus", "expected OK reply");
    } else {
        ok("focus");
    }
    
    nv_wm_unregister("win_focus_test");
    nv_window_destroy(win);
    nv_arena_destroy(arena);
    nv_window_destroy(sender);
}

static void test_op_windows_list(void) {
    nv_wm_init();
    nv_window_t* sender = make_window();
    nv_arena_t* arena = nv_arena_create(4096);
    
    // Clear registry first (hacky but needed for clean test)
    // Actually we can just check if our window is in the list
    nv_window_cfg_t cfg = { .title="ListTest" };
    nv_window_t* win = nv_window_alloc(g_app, &cfg);
    register_test_window(win, "win_list_test");
    
    test_reset_replies();
    nv_op_windows_list(sender, 1, NULL, arena);
    
    if (!test_last_reply_ok()) {
        fail("list", "expected OK reply");
    } else {
        // Check if output contains our window
        if (test_last_reply_json() && strstr(test_last_reply_json(), "win_list_test")) {
            ok("list");
        } else {
            fail("list", "window not found in list");
        }
    }
    
    nv_wm_unregister("win_list_test");
    nv_window_destroy(win);
    nv_arena_destroy(arena);
    nv_window_destroy(sender);
}

static void test_op_windows_get_info(void) {
    nv_wm_init();
    nv_window_t* sender = make_window();
    nv_arena_t* arena = nv_arena_create(4096);
    
    nv_window_cfg_t cfg = { .title="InfoTest", .width=100, .height=100 };
    nv_window_t* info_win = nv_window_alloc(g_app, &cfg);
    register_test_window(info_win, "win_info_test");
    
    test_reset_replies();
    nv_json_val_t* args = parse(arena, "{\"id\":\"win_info_test\"}");
    nv_op_windows_get_info(sender, 1, args, arena);
    
    if (!test_last_reply_ok()) {
        fail("get_info", "expected OK reply");
    } else {
        if (test_last_reply_json() && strstr(test_last_reply_json(), "InfoTest") && strstr(test_last_reply_json(), "\"width\":100")) {
            ok("get_info");
        } else {
            fail("get_info", "incorrect info");
        }
    }
    
    // 6. Set Title
    test_reset_replies();
    nv_json_val_t* args6 = parse(arena, "{\"id\":\"win_info_test\",\"title\":\"New Title\"}");
    nv_op_windows_set_title(sender, 6, args6, arena);
    
    if (!test_last_reply_ok()) {
        fail("set_title", "expected OK reply");
    } else {
        ok("set_title");
    }
    
    // Cleanup
    nv_wm_unregister("win_info_test");
    nv_window_destroy(info_win);
    nv_arena_destroy(arena);
    nv_window_destroy(sender);
}

int main(void) {
    printf("=== nativeview Windows Ops Tests ===\n\n");
    
    // Create app context for tests
    g_app = nv_app_create();
    if (!g_app) {
        printf("Failed to create app context\n");
        return 1;
    }
    
    nv_wm_init(); // Initialize registry
    
    test_op_windows_open();
    test_op_windows_close();
    test_op_windows_focus();
    test_op_windows_list();
    test_op_windows_get_info();
    
    nv_app_destroy(g_app);
    
    printf("\n=== Summary ===\n");
    printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
    
    return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
