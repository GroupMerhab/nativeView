#include "nv_op_ipc_bus.h"
#include "nv_window_manager.h"
#include "nv_window.h"
#include "nv_json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "test_helpers.h"

// Mock helpers
static char last_push_event[64];
static char last_push_payload[1024];
static int push_count = 0;

// Override nv_send for push
NV_API void nv_send(nv_window_t* window, const char* event, const char* json_str) {
    (void)window;
    push_count++;
    strncpy(last_push_event, event, sizeof(last_push_event)-1);
    strncpy(last_push_payload, json_str, sizeof(last_push_payload)-1);
}

// Global app for tests
static nv_app_t* g_app = NULL;

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int current_test_failed = 0;

static void test_start(const char* name) {
    printf("Running %s...\n", name);
    tests_run++;
    current_test_failed = 0;
}

static void test_assert(int condition, const char* msg) {
    if (!condition) {
        printf("FAILED: %s\n", msg);
        tests_failed++;
        current_test_failed = 1;
    } else {
        // printf("PASSED: %s\n", msg);
    }
}

static void test_end(void) {
    if (current_test_failed == 0) {
        tests_passed++;
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
}

static void setUp(void) {
    // Reset mocks
    test_reset_replies();
    last_push_event[0] = 0;
    last_push_payload[0] = 0;
    push_count = 0;
    
    // Clear registry
    nv_wm_entry_t entries[NV_MAX_WINDOWS];
    size_t count = NV_MAX_WINDOWS;
    nv_wm_list(entries, &count);
    for (size_t i=0; i<count; i++) {
        if (entries[i].active) nv_wm_unregister(entries[i].id);
    }
}

static nv_window_t* make_window(const char* id) {
    nv_window_cfg_t cfg = {0};
    cfg.title = "Test";
    cfg.width = 800;
    cfg.height = 600;
    nv_window_t* w = nv_window_create(g_app, &cfg);
    
    // Remove auto-assigned ID so we can use the test ID
    const char* old_id = nv_wm_get_id_by_window(w);
    if (old_id) {
        nv_wm_unregister(old_id);
    }
    
    nv_wm_register(id, w);
    return w;
}

// Tests

static void test_ipc_bus_send_to_specific_id(void) {
    setUp();
    test_start("test_ipc_bus_send_to_specific_id");
    
    nv_window_t* sender = make_window("sender");
    nv_window_t* target = make_window("target");
    
    nv_arena_t* arena = nv_arena_create(16384);
    
    // Args: {to: "target", event: "ping", data: {foo: "bar"}}
    nv_json_val_t* args = nv_json_parse(arena, "{\"to\":\"target\",\"event\":\"ping\",\"data\":{\"foo\":\"bar\"}}");
    
    nv_op_ipc_bus_send(sender, 1, args, arena);
    
    // Verify reply
    test_assert(test_last_reply_ok(), "reply type ok");
    test_assert(strstr(test_last_reply_json(), "\"delivered\":1") != NULL, "delivered 1");
    
    // Verify push
    test_assert(push_count == 1, "push count 1");
    test_assert(strcmp(last_push_event, "ipc_bus.message") == 0, "event ipc_bus.message");
    test_assert(strstr(last_push_payload, "\"from\":\"sender\"") != NULL, "payload from sender");
    test_assert(strstr(last_push_payload, "\"event\":\"ping\"") != NULL, "payload event ping");
    test_assert(strstr(last_push_payload, "\"data\":{\"foo\":\"bar\"}") != NULL, "payload data");
    
    test_end();
    nv_arena_destroy(arena);
}

static void test_ipc_bus_send_broadcast(void) {
    setUp();
    test_start("test_ipc_bus_send_broadcast");
    
    nv_window_t* sender = make_window("sender");
    make_window("w1");
    make_window("w2");
    
    nv_arena_t* arena = nv_arena_create(16384);
    
    // Args: {to: "*", event: "hello", data: {msg: "world"}}
    nv_json_val_t* args = nv_json_parse(arena, "{\"to\":\"*\",\"event\":\"hello\",\"data\":{\"msg\":\"world\"}}");
    
    nv_op_ipc_bus_send(sender, 1, args, arena);
    
    // Verify reply: delivered 2 (sender excluded)
    test_assert(test_last_reply_ok(), "reply type ok");
    test_assert(strstr(test_last_reply_json(), "\"delivered\":2") != NULL, "delivered 2");
    
    // Verify push count
    test_assert(push_count == 2, "push count 2");
    
    test_end();
    nv_arena_destroy(arena);
}

static void test_ipc_bus_send_unknown_target(void) {
    setUp();
    test_start("test_ipc_bus_send_unknown_target");
    
    nv_window_t* sender = make_window("sender");
    
    nv_arena_t* arena = nv_arena_create(16384);
    
    nv_json_val_t* args = nv_json_parse(arena, "{\"to\":\"missing\",\"event\":\"ping\",\"data\":{}}");
    
    nv_op_ipc_bus_send(sender, 1, args, arena);
    
    // Verify error
    test_assert(!test_last_reply_ok(), "reply type error");
    test_assert(strcmp(test_last_reply_error_code(), "ERR_NOT_FOUND") == 0, "error code ERR_NOT_FOUND");
    
    // Verify no push
    test_assert(push_count == 0, "push count 0");
    
    test_end();
    nv_arena_destroy(arena);
}

static void test_ipc_bus_broadcast_self_exclusion(void) {
    setUp();
    test_start("test_ipc_bus_broadcast_self_exclusion");
    
    nv_window_t* sender = make_window("sender");
    
    nv_arena_t* arena = nv_arena_create(16384);
    
    // Broadcast with only sender exists
    nv_json_val_t* args = nv_json_parse(arena, "{\"to\":\"*\",\"event\":\"ping\",\"data\":{}}");
    
    nv_op_ipc_bus_send(sender, 1, args, arena);
    
    // Verify delivered 0
    test_assert(test_last_reply_ok(), "reply type ok");
    test_assert(strstr(test_last_reply_json(), "\"delivered\":0") != NULL, "delivered 0");
    
    test_assert(push_count == 0, "push count 0");
    
    test_end();
    nv_arena_destroy(arena);
}

static void test_ipc_bus_unknown_sender(void) {
    setUp();
    test_start("test_ipc_bus_unknown_sender");
    
    // Create window but don't register it
    nv_window_cfg_t cfg = {0};
    cfg.width = 100;
    cfg.height = 100;
    nv_window_t* sender = nv_window_create(g_app, &cfg);
    
    // Explicitly unregister to simulate unknown sender (since nv_window_create now auto-registers)
    const char* sender_id = nv_wm_get_id_by_window(sender);
    if (sender_id) {
        nv_wm_unregister(sender_id);
    }
    
    nv_window_t* target = make_window("target");
    
    nv_arena_t* arena = nv_arena_create(16384);
    
    nv_json_val_t* args = nv_json_parse(arena, "{\"to\":\"target\",\"event\":\"ping\",\"data\":{}}");
    
    nv_op_ipc_bus_send(sender, 1, args, arena);
    
    // Verify push has from="unknown"
    test_assert(push_count == 1, "push count 1");
    test_assert(strstr(last_push_payload, "\"from\":\"unknown\"") != NULL, "payload from unknown");
    
    test_end();
    nv_arena_destroy(arena);
    nv_window_destroy(sender); // Manual cleanup since not in registry
}

static void test_ipc_bus_data_forwarding(void) {
    setUp();
    test_start("test_ipc_bus_data_forwarding");
    
    nv_window_t* sender = make_window("sender");
    nv_window_t* target = make_window("target");
    
    nv_arena_t* arena = nv_arena_create(16384);
    
    // Complex data
    nv_json_val_t* args = nv_json_parse(arena, "{\"to\":\"target\",\"event\":\"ping\",\"data\":{\"a\":1,\"b\":[true,null],\"c\":{\"d\":\"e\"}}}");
    
    nv_op_ipc_bus_send(sender, 1, args, arena);
    
    // Verify push payload
    test_assert(strstr(last_push_payload, "\"data\":{\"a\":1,\"b\":[true,null],\"c\":{\"d\":\"e\"}}") != NULL, "payload data forwarding");
    
    test_end();
    nv_arena_destroy(arena);
}

int main(void) {
    // Init global app for tests
    g_app = nv_app_create();
    
    printf("Running IPC Bus Tests\n");
    test_ipc_bus_send_to_specific_id();
    test_ipc_bus_send_broadcast();
    test_ipc_bus_send_unknown_target();
    test_ipc_bus_broadcast_self_exclusion();
    test_ipc_bus_unknown_sender();
    test_ipc_bus_data_forwarding();
    
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_failed == 0) ? 0 : 1;
}
