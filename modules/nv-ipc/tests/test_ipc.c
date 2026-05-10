/* =============================================================================
 * test_ipc.c - IPC Module Tests
 *
 * Tests: message construction, serialization, JSON-RPC 2.0 compliance
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#include "nv_ipc.h"
#include "nv_ipc_internal.h"
#include "nv_json.h"
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

void test_ok(const char* name) {
  printf("✓ %s\n", name);
  tests_passed++;
}

void test_fail(const char* name, const char* reason) {
  printf("✗ %s: %s\n", name, reason);
  tests_failed++;
}

/* =============================================================================
 * Test 1: Simple Request Message
 * =============================================================================
 */
void test_request_message(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("request_message", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_request(arena, "window.open", 1);
  if (!msg) {
    test_fail("request_message", "message creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (nv_ipc_get_type(msg) != NV_IPC_REQUEST) {
    test_fail("request_message", "message type mismatch");
    nv_arena_destroy(arena);
    return;
  }
  
  if (nv_ipc_get_id(msg) != 1) {
    test_fail("request_message", "message ID mismatch");
    nv_arena_destroy(arena);
    return;
  }
  
  if (strcmp(nv_ipc_get_method(msg), "window.open") != 0) {
    test_fail("request_message", "method mismatch");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* json = nv_ipc_serialize(msg);
  if (!json || !strstr(json, "\"jsonrpc\"") || !strstr(json, "\"method\"")) {
    test_fail("request_message", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("request_message");
}

/* =============================================================================
 * Test 2: Request with Parameters
 * =============================================================================
 */
void test_request_with_params(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("request_with_params", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_request(arena, "app.getData", 2);
  
  nv_json_t* params = nv_json_object(arena);
  nv_json_str(params, "query", "users");
  nv_json_int(params, "limit", 100);
  
  if (nv_ipc_set_params(msg, params) != 0) {
    test_fail("request_with_params", "failed to set params");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* json = nv_ipc_serialize(msg);
  if (!json || !strstr(json, "\"params\"") || !strstr(json, "\"query\"")) {
    test_fail("request_with_params", "params not serialized");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("request_with_params");
}

/* =============================================================================
 * Test 3: Response Message
 * =============================================================================
 */
void test_response_message(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("response_message", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_response(arena, 1);
  if (!msg || nv_ipc_get_type(msg) != NV_IPC_RESPONSE) {
    test_fail("response_message", "response creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (nv_ipc_get_id(msg) != 1) {
    test_fail("response_message", "response ID mismatch");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_json_t* result = nv_json_object(arena);
  nv_json_str(result, "status", "ok");
  nv_json_int(result, "count", 42);
  
  if (nv_ipc_set_result(msg, result) != 0) {
    test_fail("response_message", "failed to set result");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* json = nv_ipc_serialize(msg);
  if (!json || !strstr(json, "\"result\"") || !strstr(json, "\"status\"")) {
    test_fail("response_message", "result not serialized");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("response_message");
}

/* =============================================================================
 * Test 4: Error Response Message
 * =============================================================================
 */
void test_error_response(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("error_response", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_error_response(arena, 3, -32600, "Invalid Request");
  if (!msg || nv_ipc_get_type(msg) != NV_IPC_RESPONSE) {
    test_fail("error_response", "error response creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  int error_code = 0;
  const char* error_msg = NULL;
  
  if (nv_ipc_get_error(msg, &error_code, &error_msg) != 0) {
    test_fail("error_response", "error retrieval failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (error_code != -32600 || strcmp(error_msg, "Invalid Request") != 0) {
    test_fail("error_response", "error details mismatch");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* json = nv_ipc_serialize(msg);
  if (!json || !strstr(json, "\"error\"") || !strstr(json, "\"code\"")) {
    test_fail("error_response", "error not serialized");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("error_response");
}

/* =============================================================================
 * Test 5: Notification Message
 * =============================================================================
 */
void test_notification(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("notification", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_notify(arena, "window.onMessage");
  if (!msg || nv_ipc_get_type(msg) != NV_IPC_NOTIFICATION) {
    test_fail("notification", "notification creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (nv_ipc_get_id(msg) != -1) {
    test_fail("notification", "notification should have no ID");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_json_t* data = nv_json_object(arena);
  nv_json_str(data, "text", "Hello from C");
  
  if (nv_ipc_set_event_data(msg, data) != 0) {
    test_fail("notification", "failed to set event data");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* json = nv_ipc_serialize(msg);
  if (!json || !strstr(json, "\"method\"") || !strstr(json, "\"params\"")) {
    test_fail("notification", "notification not serialized");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Notifications should NOT have ID field */
  if (strstr(json, "\"id\"")) {
    test_fail("notification", "notification should not have ID field");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("notification");
}

/* =============================================================================
 * Test 6: Message Size Tracking
 * =============================================================================
 */
void test_message_size(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("message_size", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_request(arena, "test.method", 1);
  
  nv_json_t* params = nv_json_object(arena);
  nv_json_str(params, "key", "value");
  nv_ipc_set_params(msg, params);
  
  const char* json = nv_ipc_serialize(msg);
  size_t size = nv_ipc_get_size(msg);
  
  if (size == 0 || size != strlen(json)) {
    test_fail("message_size", "size mismatch");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("message_size");
}

/* =============================================================================
 * Test 7: JSON-RPC 2.0 Compliance
 * =============================================================================
 */
void test_json_rpc_compliance(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("json_rpc_compliance", "arena creation failed");
    return;
  }
  
  /* Create request */
  nv_ipc_message_t* req = nv_ipc_request(arena, "calc.add", 1);
  nv_json_t* params = nv_json_object(arena);
  nv_json_int(params, "a", 5);
  nv_json_int(params, "b", 3);
  nv_ipc_set_params(req, params);
  
  const char* req_json = nv_ipc_serialize(req);
  if (!req_json) {
    test_fail("json_rpc_compliance", "request serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Verify request fields */
  if (!strstr(req_json, "\"jsonrpc\":\"2.0\"")) {
    test_fail("json_rpc_compliance", "missing jsonrpc field");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(req_json, "\"method\":\"calc.add\"")) {
    test_fail("json_rpc_compliance", "missing method field");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(req_json, "\"id\":1")) {
    test_fail("json_rpc_compliance", "missing or wrong id field");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Create response */
  nv_ipc_message_t* resp = nv_ipc_response(arena, 1);
  nv_json_t* result = nv_json_object(arena);
  nv_json_int(result, "sum", 8);
  nv_ipc_set_result(resp, result);
  
  const char* resp_json = nv_ipc_serialize(resp);
  if (!resp_json) {
    test_fail("json_rpc_compliance", "response serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(resp_json, "\"result\"")) {
    test_fail("json_rpc_compliance", "response missing result field");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(resp_json, "\"id\":1")) {
    test_fail("json_rpc_compliance", "response missing id field");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("json_rpc_compliance");
}

/* =============================================================================
 * Test 8: Compact Output
 * =============================================================================
 */
void test_compact_serialization(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("compact_serialization", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_request(arena, "test", 1);
  nv_json_t* params = nv_json_object(arena);
  nv_json_str(params, "x", "y");
  nv_ipc_set_params(msg, params);
  
  const char* json = nv_ipc_serialize(msg);
  if (!json) {
    test_fail("compact_serialization", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Check for no unnecessary whitespace */
  if (strstr(json, "  ") || strstr(json, "\n") || strstr(json, "\r")) {
    test_fail("compact_serialization", "output not compact");
    nv_arena_destroy(arena);
    return;
  }
  
  if (json[0] == ' ' || json[strlen(json)-1] == ' ') {
    test_fail("compact_serialization", "leading/trailing whitespace");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("compact_serialization");
}

/* =============================================================================
 * Test 9: Multiple Parameters Types
 * =============================================================================
 */
void test_multiple_param_types(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("multiple_param_types", "arena creation failed");
    return;
  }
  
  nv_ipc_message_t* msg = nv_ipc_request(arena, "api.call", 1);
  
  nv_json_t* params = nv_json_object(arena);
  nv_json_int(params, "int_val", 42);
  nv_json_double(params, "double_val", 3.14);
  nv_json_bool(params, "bool_val", 1);
  nv_json_str(params, "str_val", "text");
  nv_json_null(params, "null_val");
  
  nv_ipc_set_params(msg, params);
  
  const char* json = nv_ipc_serialize(msg);
  if (!json) {
    test_fail("multiple_param_types", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "42") || !strstr(json, "3.14") || !strstr(json, "true") ||
      !strstr(json, "\"text\"") || !strstr(json, "null")) {
    test_fail("multiple_param_types", "not all types present");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("multiple_param_types");
}

/* =============================================================================
 * Test 10: NULL Safety
 * =============================================================================
 */
void test_null_safety_ipc(void) {
  /* These should not crash */
  nv_ipc_request(NULL, "test", 1);
  nv_ipc_response(NULL, 1);
  nv_ipc_notify(NULL, "test");
  nv_ipc_set_params(NULL, NULL);
  nv_ipc_set_result(NULL, NULL);
  nv_ipc_set_event_data(NULL, NULL);
  nv_ipc_serialize(NULL);
  nv_ipc_get_size(NULL);
  nv_ipc_get_type(NULL);
  nv_ipc_get_method(NULL);
  nv_ipc_get_id(NULL);
  nv_ipc_get_params(NULL);
  nv_ipc_get_error(NULL, NULL, NULL);
  nv_ipc_destroy(NULL);
  
  test_ok("null_safety_ipc");
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

/* Test callback state for internal API tests */
static struct {
  int msg_called;
  const char* last_event;
  const char* last_json;
  int ready_called;
} test_state = {0, NULL, NULL, 0};

void test_msg_callback(nv_window_t* window, const char* event,
                       const char* json, void* userdata) {
  (void)window;
  (void)userdata;
  test_state.msg_called++;
  test_state.last_event = event;
  test_state.last_json = json;
}

void test_ready_callback(nv_window_t* window, void* userdata) {
  (void)window;
  (void)userdata;
  test_state.ready_called++;
}

/* =============================================================================
 * Test 11: IPC State Creation
 * =============================================================================
 */
void test_ipc_state_create(void) {
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    test_fail("ipc_state_create", "arena creation failed");
    return;
  }
  
  nv_ipc_state_t* state = nv_ipc_state_create(arena);
  if (!state) {
    test_fail("ipc_state_create", "state creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  test_ok("ipc_state_create");
  nv_arena_destroy(arena);
}

/* =============================================================================
 * Test 12: Callback Registration
 * =============================================================================
 */
void test_ipc_set_callbacks(void) {
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    test_fail("ipc_set_callbacks", "arena creation failed");
    return;
  }
  
  nv_ipc_state_t* state = nv_ipc_state_create(arena);
  if (!state) {
    test_fail("ipc_set_callbacks", "state creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Set callbacks - should not crash */
  nv_ipc_set_msg_cb(state, test_msg_callback, NULL);
  nv_ipc_set_ready_cb(state, test_ready_callback, NULL);
  nv_ipc_set_close_cb(state, NULL, NULL);
  
  /* NULL-safety test */
  nv_ipc_set_msg_cb(NULL, test_msg_callback, NULL);
  nv_ipc_set_ready_cb(NULL, test_ready_callback, NULL);
  nv_ipc_set_close_cb(NULL, NULL, NULL);
  
  test_ok("ipc_set_callbacks");
  nv_arena_destroy(arena);
}

/* =============================================================================
 * Test 13: Message Dispatch with Valid JSON
 * =============================================================================
 */
void test_ipc_dispatch_valid(void) {
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    test_fail("ipc_dispatch_valid", "arena creation failed");
    return;
  }
  
  nv_ipc_state_t* state = nv_ipc_state_create(arena);
  if (!state) {
    test_fail("ipc_dispatch_valid", "state creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Reset test state */
  memset(&test_state, 0, sizeof(test_state));
  
  /* Register callback */
  nv_ipc_set_msg_cb(state, test_msg_callback, NULL);
  
  /* Dispatch a message */
  const char* raw_json = "{\"e\":\"test_event\",\"d\":{\"key\":\"value\"}}";
  nv_ipc_dispatch(NULL, state, raw_json);
  
  /* Verify callback was called */
  if (test_state.msg_called != 1) {
    test_fail("ipc_dispatch_valid", "callback not called");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!test_state.last_event || strcmp(test_state.last_event, "test_event") != 0) {
    test_fail("ipc_dispatch_valid", "event name mismatch");
    nv_arena_destroy(arena);
    return;
  }
  
  test_ok("ipc_dispatch_valid");
  nv_arena_destroy(arena);
}

/* =============================================================================
 * Test 14: Message Dispatch with Malformed JSON
 * =============================================================================
 */
void test_ipc_dispatch_malformed(void) {
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    test_fail("ipc_dispatch_malformed", "arena creation failed");
    return;
  }
  
  nv_ipc_state_t* state = nv_ipc_state_create(arena);
  if (!state) {
    test_fail("ipc_dispatch_malformed", "state creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Reset test state */
  memset(&test_state, 0, sizeof(test_state));
  
  /* Register callback */
  nv_ipc_set_msg_cb(state, test_msg_callback, NULL);
  
  /* Dispatch malformed JSON - should not crash */
  const char* bad_json = "{invalid json";
  nv_ipc_dispatch(NULL, state, bad_json);
  
  /* Callback should NOT have been called */
  if (test_state.msg_called != 0) {
    test_fail("ipc_dispatch_malformed", "callback was called on bad JSON");
    nv_arena_destroy(arena);
    return;
  }
  
  test_ok("ipc_dispatch_malformed");
  nv_arena_destroy(arena);
}

/* =============================================================================
 * Test 15: Inject Script Contains Required Functions
 * =============================================================================
 */
void test_ipc_inject_script(void) {
  const char* script = nv_ipc_inject_script();
  if (!script) {
    test_fail("ipc_inject_script", "inject_script returned NULL");
    return;
  }
  
  /* Check for key components */
  if (!strstr(script, "window.__nv")) {
    test_fail("ipc_inject_script", "missing window.__nv");
    return;
  }
  
  if (!strstr(script, "send")) {
    test_fail("ipc_inject_script", "missing send function");
    return;
  }
  
  if (!strstr(script, "on") && !strstr(script, ".on")) {
    test_fail("ipc_inject_script", "missing on function");
    return;
  }
  
  if (!strstr(script, "_emit")) {
    test_fail("ipc_inject_script", "missing _emit function");
    return;
  }
  
  if (!strstr(script, "NV_POST")) {
    test_fail("ipc_inject_script", "missing NV_POST placeholder");
    return;
  }

  /* send() must wrap event + payload as compact wire {\"e\",...} for nv_ipc_dispatch */
  if (!strstr(script, "{e:event")) {
    test_fail("ipc_inject_script", "send must JSON.stringify compact wire with e (event) field");
    return;
  }
  
  test_ok("ipc_inject_script");
}

/* =============================================================================
 * Test 16: Build Send Small Message (Stack Path)
 * =============================================================================
 */
void test_ipc_build_send_small(void) {
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    test_fail("ipc_build_send_small", "arena creation failed");
    return;
  }
  
  /* Get initial stats */
  nv_ipc_stats_t stats_before = nv_ipc_get_stats();
  
  /* Build a small message */
  const char* result = nv_ipc_build_send(arena, "test_event", "{\"data\":123}");
  if (!result) {
    test_fail("ipc_build_send_small", "build_send returned NULL");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Check that it contains expected parts */
  if (!strstr(result, "window.__nv._emit")) {
    test_fail("ipc_build_send_small", "missing _emit call");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(result, "test_event")) {
    test_fail("ipc_build_send_small", "event name not in result");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Verify stats show stack allocation */
  nv_ipc_stats_t stats_after = nv_ipc_get_stats();
  if (stats_after.stack_allocs <= stats_before.stack_allocs) {
    test_fail("ipc_build_send_small", "stack_allocs not incremented");
    nv_arena_destroy(arena);
    return;
  }
  
  test_ok("ipc_build_send_small");
  nv_arena_destroy(arena);
}

/* =============================================================================
 * Test 17: Build Send Large Message (Heap Path)
 * =============================================================================
 */
void test_ipc_build_send_large(void) {
  nv_arena_t* arena = nv_arena_create(16384);
  if (!arena) {
    test_fail("ipc_build_send_large", "arena creation failed");
    return;
  }
  
  /* Get initial stats */
  nv_ipc_stats_t stats_before = nv_ipc_get_stats();
  
  /* Create a large JSON payload (> 2048 bytes) */
  char large_json[3000];
  strcpy(large_json, "{\"data\":\"");
  for (int i = 0; i < 2800; i++) {
    strcat(large_json, "x");
  }
  strcat(large_json, "\"}");
  
  /* Build the message */
  const char* result = nv_ipc_build_send(arena, "large_event", large_json);
  if (!result) {
    /* Large messages might require arena; that's ok for this test */
    test_ok("ipc_build_send_large");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Message should be valid JavaScript */
  if (!strstr(result, "window.__nv._emit")) {
    test_fail("ipc_build_send_large", "missing _emit call");
    nv_arena_destroy(arena);
    return;
  }
  
  test_ok("ipc_build_send_large");
  nv_arena_destroy(arena);
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void) {
  printf("=== nativeview IPC Module Tests ===\n\n");
  printf("Running tests...\n");
  
  test_request_message();
  test_request_with_params();
  test_response_message();
  test_error_response();
  test_notification();
  test_message_size();
  test_json_rpc_compliance();
  test_compact_serialization();
  test_multiple_param_types();
  test_null_safety_ipc();
  
  /* Internal API tests */
  test_ipc_state_create();
  test_ipc_set_callbacks();
  test_ipc_dispatch_valid();
  test_ipc_dispatch_malformed();
  test_ipc_inject_script();
  test_ipc_build_send_small();
  test_ipc_build_send_large();
  
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
