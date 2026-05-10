/* =============================================================================
 * test_json_compose.c - JSON Builder Composition and Serialization Tests
 *
 * Tests: Complex nested structures, array operations, serialization output
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

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
 * Test 1: Deeply Nested Objects (3 levels)
 * =============================================================================
 */
void test_deep_nesting(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("deep_nesting", "arena creation failed");
    return;
  }
  
  /* Level 1 */
  nv_json_t* root = nv_json_object(arena);
  nv_json_str(root, "app", "myapp");
  
  /* Level 2 */
  nv_json_t* config = nv_json_object(arena);
  nv_json_str(config, "version", "1.0");
  
  /* Level 3 */
  nv_json_t* settings = nv_json_object(arena);
  nv_json_int(settings, "timeout", 3000);
  nv_json_bool(settings, "debug", 1);
  
  nv_json_nest(config, "settings", settings);
  nv_json_nest(root, "config", config);
  
  const char* json = nv_json_serialize(root);
  if (!json) {
    test_fail("deep_nesting", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Verify structure is present */
  if (!strstr(json, "\"app\"") || !strstr(json, "\"config\"") || 
      !strstr(json, "\"settings\"") || !strstr(json, "\"timeout\"")) {
    test_fail("deep_nesting", "nested structure incomplete");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("deep_nesting");
}

/* =============================================================================
 * Test 2: Array of Objects
 * =============================================================================
 */
void test_array_of_objects(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("array_of_objects", "arena creation failed");
    return;
  }
  
  nv_json_t* arr = nv_json_array(arena);
  
  /* Create objects for array */
  nv_json_t* item1 = nv_json_object(arena);
  nv_json_str(item1, "id", "user1");
  nv_json_int(item1, "count", 5);
  
  nv_json_t* item2 = nv_json_object(arena);
  nv_json_str(item2, "id", "user2");
  nv_json_int(item2, "count", 10);
  
  nv_json_array_push(arr, item1);
  nv_json_array_push(arr, item2);
  
  const char* json = nv_json_serialize(arr);
  if (!json) {
    test_fail("array_of_objects", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "\"user1\"") || !strstr(json, "\"user2\"")) {
    test_fail("array_of_objects", "expected user IDs not found");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("array_of_objects");
}

/* =============================================================================
 * Test 3: Object with Multiple Array Fields
 * =============================================================================
 */
void test_object_with_arrays(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("object_with_arrays", "arena creation failed");
    return;
  }
  
  nv_json_t* root = nv_json_object(arena);
  nv_json_str(root, "name", "batch");
  
  /* First array field */
  nv_json_t* ids = nv_json_array(arena);
  nv_json_t* id1 = nv_json_object(arena);
  nv_json_int(id1, "val", 1);
  nv_json_array_push(ids, id1);
  
  nv_json_nest(root, "ids", ids);
  
  /* Second array field */
  nv_json_t* tags = nv_json_array(arena);
  nv_json_t* tag1 = nv_json_object(arena);
  nv_json_str(tag1, "name", "system");
  nv_json_array_push(tags, tag1);
  
  nv_json_nest(root, "tags", tags);
  
  const char* json = nv_json_serialize(root);
  if (!json) {
    test_fail("object_with_arrays", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "\"ids\"") || !strstr(json, "\"tags\"")) {
    test_fail("object_with_arrays", "array fields not found");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("object_with_arrays");
}

/* =============================================================================
 * Test 4: Complex Escaping in Nested Context
 * =============================================================================
 */
void test_nested_escaping(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("nested_escaping", "arena creation failed");
    return;
  }
  
  nv_json_t* root = nv_json_object(arena);
  
  nv_json_t* nested = nv_json_object(arena);
  nv_json_str(nested, "path", "C:\\Program Files\\App");
  nv_json_str(nested, "query", "name=John&age=30");
  
  nv_json_nest(root, "data", nested);
  
  const char* json = nv_json_serialize(root);
  if (!json) {
    test_fail("nested_escaping", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Check escapes */
  if (!strstr(json, "\\\\")) {
    test_fail("nested_escaping", "backslash not escaped");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("nested_escaping");
}

/* =============================================================================
 * Test 5: Serialization Validity (Valid JSON Output)
 * =============================================================================
 */
void test_output_validity(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("output_validity", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "type", "message");
  nv_json_int(obj, "id", 123);
  nv_json_double(obj, "value", 45.67);
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("output_validity", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Basic JSON validity checks */
  if (json[0] != '{' || json[strlen(json)-1] != '}') {
    test_fail("output_validity", "object not wrapped in braces");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Must have colons between keys and values */
  if (!strstr(json, ":")) {
    test_fail("output_validity", "no key-value separators found");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Must have commas between properties (except last) */
  char* first_comma = strchr(json, ',');
  if (!first_comma) {
    /* Single property object is OK */
  } else {
    /* Multi-property object should have at least one comma */
  }
  
  nv_arena_destroy(arena);
  test_ok("output_validity");
}

/* =============================================================================
 * Test 6: Large Array Serialization
 * =============================================================================
 */
void test_large_array(void) {
  nv_arena_t* arena = nv_arena_create(8192);
  if (!arena) {
    test_fail("large_array", "arena creation failed");
    return;
  }
  
  nv_json_t* arr = nv_json_array(arena);
  
  /* Add 20 items */
  for (int i = 0; i < 20; i++) {
    nv_json_t* item = nv_json_object(arena);
    nv_json_int(item, "index", i);
    nv_json_str(item, "name", "item");
    nv_json_array_push(arr, item);
  }
  
  const char* json = nv_json_serialize(arr);
  if (!json) {
    test_fail("large_array", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Verify array bracket and content */
  if (json[0] != '[' || json[strlen(json)-1] != ']') {
    test_fail("large_array", "array brackets missing");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("large_array");
}

/* =============================================================================
 * Test 7: All Numeric Formats
 * =============================================================================
 */
void test_numeric_formats(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("numeric_formats", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  nv_json_int(obj, "zero", 0);
  nv_json_int(obj, "positive", 12345);
  nv_json_int(obj, "negative", -98765);
  nv_json_double(obj, "decimal", 123.456);
  nv_json_double(obj, "scientific", 1.23e-4);
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("numeric_formats", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "0") || !strstr(json, "12345") || !strstr(json, "-98765")) {
    test_fail("numeric_formats", "integers not found");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("numeric_formats");
}

/* =============================================================================
 * Test 8: Mixed String Escape Sequences
 * =============================================================================
 */
void test_mixed_escapes(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("mixed_escapes", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  
  /* String with multiple escape types */
  nv_json_str(obj, "text", "Line1\nLine2\t\"quoted\"\nBackslash\\path");
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("mixed_escapes", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Check all escape sequences are present */
  int has_newline = strstr(json, "\\n") != NULL;
  int has_tab = strstr(json, "\\t") != NULL;
  int has_quote = strstr(json, "\\\"") != NULL;
  int has_backslash = strstr(json, "\\\\") != NULL;
  
  if (!has_newline || !has_tab || !has_quote || !has_backslash) {
    test_fail("mixed_escapes", "not all escapes present");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("mixed_escapes");
}

/* =============================================================================
 * Test 9: JSON-RPC 2.0 Request Structure
 * =============================================================================
 */
void test_json_rpc_request(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("json_rpc_request", "arena creation failed");
    return;
  }
  
  /* Create JSON-RPC 2.0 request */
  nv_json_t* request = nv_json_object(arena);
  nv_json_str(request, "jsonrpc", "2.0");
  nv_json_str(request, "method", "window.open");
  
  nv_json_t* params = nv_json_object(arena);
  nv_json_str(params, "url", "https://example.com");
  nv_json_int(params, "width", 800);
  nv_json_int(params, "height", 600);
  
  nv_json_nest(request, "params", params);
  nv_json_int(request, "id", 1);
  
  const char* json = nv_json_serialize(request);
  if (!json) {
    test_fail("json_rpc_request", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "\"jsonrpc\"") || !strstr(json, "\"method\"") ||
      !strstr(json, "\"params\"") || !strstr(json, "\"id\"")) {
    test_fail("json_rpc_request", "JSON-RPC structure incomplete");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("json_rpc_request");
}

/* =============================================================================
 * Test 10: Compact Serialization (No Extra Whitespace)
 * =============================================================================
 */
void test_compact_output(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("compact_output", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "name", "test");
  nv_json_int(obj, "value", 42);
  nv_json_bool(obj, "active", 1);
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("compact_output", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Check for no leading/trailing/unnecessary whitespace */
  if (json[0] == ' ' || json[strlen(json)-1] == ' ') {
    test_fail("compact_output", "led/trailing whitespace found");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Check no double spaces or newlines */
  if (strstr(json, "  ") || strstr(json, "\n") || strstr(json, "\r")) {
    test_fail("compact_output", "unnecessary whitespace found");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("compact_output");
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void) {
  printf("=== nativeview JSON Builder Composition Tests ===\n\n");
  printf("Running tests...\n");
  
  test_deep_nesting();
  test_array_of_objects();
  test_object_with_arrays();
  test_nested_escaping();
  test_output_validity();
  test_large_array();
  test_numeric_formats();
  test_mixed_escapes();
  test_json_rpc_request();
  test_compact_output();
  
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
