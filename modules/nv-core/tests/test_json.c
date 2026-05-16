/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * test_json.c - JSON Builder Tests
 *
 * Tests: object/array creation, serialization, string escaping
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
 * Test 1: Simple Flat Object
 * =============================================================================
 */
void test_simple_object(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("simple_object", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  if (!obj) {
    test_fail("simple_object", "object creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_json_str(obj, "name", "Alice");
  nv_json_int(obj, "age", 30);
  nv_json_bool(obj, "active", 1);
  nv_json_double(obj, "score", 95.5);
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("simple_object", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Check that output contains expected elements */
  if (!strstr(json, "\"name\"") || !strstr(json, "\"Alice\"")) {
    test_fail("simple_object", "missing name field");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "\"age\"") || !strstr(json, "30")) {
    test_fail("simple_object", "missing age field");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "\"active\"") || !strstr(json, "true")) {
    test_fail("simple_object", "missing active field");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("simple_object");
}

/* =============================================================================
 * Test 2: Nested Objects
 * =============================================================================
 */
void test_nested_objects(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("nested_objects", "arena creation failed");
    return;
  }
  
  nv_json_t* person = nv_json_object(arena);
  nv_json_str(person, "name", "Bob");
  
  nv_json_t* address = nv_json_object(arena);
  nv_json_str(address, "city", "NYC");
  nv_json_str(address, "zip", "10001");
  
  nv_json_nest(person, "address", address);
  
  const char* json = nv_json_serialize(person);
  if (!json) {
    test_fail("nested_objects", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "\"address\"") || !strstr(json, "\"city\"") || !strstr(json, "\"NYC\"")) {
    test_fail("nested_objects", "nested structure missing");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("nested_objects");
}

/* =============================================================================
 * Test 3: Array with Mixed Types
 * =============================================================================
 */
void test_array_mixed_types(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("array_mixed_types", "arena creation failed");
    return;
  }
  
  nv_json_t* arr = nv_json_array(arena);
  if (!arr) {
    test_fail("array_mixed_types", "array creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_json_t* num = nv_json_object(arena);
  nv_json_int(num, "x", 1);
  
  nv_json_t* str_item = nv_json_object(arena);
  nv_json_str(str_item, "y", "hello");
  
  nv_json_t* bool_item = nv_json_object(arena);
  nv_json_bool(bool_item, "z", 1);
  
  nv_json_array_push(arr, num);
  nv_json_array_push(arr, str_item);
  nv_json_array_push(arr, bool_item);
  
  const char* json = nv_json_serialize(arr);
  if (!json) {
    test_fail("array_mixed_types", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (json[0] != '[' || json[strlen(json)-1] != ']') {
    test_fail("array_mixed_types", "array brackets missing");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("array_mixed_types");
}

/* =============================================================================
 * Test 4: String Escaping - Quotes
 * =============================================================================
 */
void test_escape_quotes(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("escape_quotes", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "message", "He said \"Hello\"");
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("escape_quotes", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Should contain escaped quotes */
  if (!strstr(json, "\\\"")) {
    test_fail("escape_quotes", "quotes not escaped");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("escape_quotes");
}

/* =============================================================================
 * Test 5: String Escaping - Backslash
 * =============================================================================
 */
void test_escape_backslash(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("escape_backslash", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "path", "C:\\Users\\file.txt");
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("escape_backslash", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Should contain escaped backslashes */
  if (!strstr(json, "\\\\")) {
    test_fail("escape_backslash", "backslashes not escaped");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("escape_backslash");
}

/* =============================================================================
 * Test 6: String Escaping - Newline and Tab
 * =============================================================================
 */
void test_escape_whitespace(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("escape_whitespace", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  nv_json_str(obj, "text", "line1\nline2\ttab");
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("escape_whitespace", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Should contain escaped newline and tab */
  if (!strstr(json, "\\n") || !strstr(json, "\\t")) {
    test_fail("escape_whitespace", "whitespace not escaped");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("escape_whitespace");
}

/* =============================================================================
 * Test 7: String Escaping - Null Character (binary data)
 * =============================================================================
 */
void test_escape_binary(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("escape_binary", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  /* Simulate a string with control character */
  const char* str_with_null = "text\x01\x02end";
  nv_json_str(obj, "data", str_with_null);
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("escape_binary", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Should escape control characters as \uXXXX */
  if (!strstr(json, "\\u")) {
    test_fail("escape_binary", "control characters not escaped");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("escape_binary");
}

/* =============================================================================
 * Test 8: Empty Object Serialization
 * =============================================================================
 */
void test_empty_object(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("empty_object", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  if (!obj) {
    test_fail("empty_object", "object creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* json = nv_json_serialize(obj);
  if (!json || strcmp(json, "{}") != 0) {
    test_fail("empty_object", "empty object not serialized as {}");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("empty_object");
}

/* =============================================================================
 * Test 9: Empty Array Serialization
 * =============================================================================
 */
void test_empty_array(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("empty_array", "arena creation failed");
    return;
  }
  
  nv_json_t* arr = nv_json_array(arena);
  if (!arr) {
    test_fail("empty_array", "array creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* json = nv_json_serialize(arr);
  if (!json || strcmp(json, "[]") != 0) {
    test_fail("empty_array", "empty array not serialized as []");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("empty_array");
}

/* =============================================================================
 * Test 10: IPC Payload (Short Wire Format)
 * =============================================================================
 */
void test_ipc_payload(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("ipc_payload", "arena creation failed");
    return;
  }
  
  /* Simulate JSON-RPC notification with short keys: e=event, d=data */
  nv_json_t* payload = nv_json_object(arena);
  nv_json_str(payload, "e", "window.onMessage");
  
  nv_json_t* data = nv_json_object(arena);
  nv_json_str(data, "msg", "Hello from C");
  nv_json_int(data, "code", 42);
  
  nv_json_nest(payload, "d", data);
  
  const char* json = nv_json_serialize(payload);
  if (!json) {
    test_fail("ipc_payload", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Wire format should be compact (no unnecessary whitespace) */
  if (strstr(json, "  ") || strstr(json, "\n")) {
    test_fail("ipc_payload", "output not compact");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "\"e\"") || !strstr(json, "\"d\"")) {
    test_fail("ipc_payload", "short keys missing");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("ipc_payload");
}

/* =============================================================================
 * Test 11: Multiple Data Types in Object
 * =============================================================================
 */
void test_all_types(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (!arena) {
    test_fail("all_types", "arena creation failed");
    return;
  }
  
  nv_json_t* obj = nv_json_object(arena);
  nv_json_null(obj, "null_val");
  nv_json_bool(obj, "true_val", 1);
  nv_json_bool(obj, "false_val", 0);
  nv_json_int(obj, "int_val", -42);
  nv_json_double(obj, "double_val", 3.14);
  nv_json_str(obj, "str_val", "text");
  
  const char* json = nv_json_serialize(obj);
  if (!json) {
    test_fail("all_types", "serialization failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (!strstr(json, "null") || !strstr(json, "true") || !strstr(json, "false") ||
      !strstr(json, "-42") || !strstr(json, "3.14") || !strstr(json, "text")) {
    test_fail("all_types", "not all types present in output");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("all_types");
}

/* =============================================================================
 * Test 12: NULL-Safe Operations
 * =============================================================================
 */
void test_null_safety(void) {
  /* These should not crash */
  nv_json_str(NULL, "key", "value");
  nv_json_int(NULL, "key", 42);
  nv_json_bool(NULL, "key", 1);
  nv_json_null(NULL, "key");
  nv_json_nest(NULL, "key", NULL);
  nv_json_array_push(NULL, NULL);
  
  const char* result = nv_json_serialize(NULL);
  if (result == NULL) {
    test_fail("null_safety", "NULL serialize should return NULL or 'null'");
    return;
  }
  
  test_ok("null_safety");
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void) {
  printf("=== nativeview JSON Builder Tests ===\n\n");
  printf("Running tests...\n");
  
  test_simple_object();
  test_nested_objects();
  test_array_mixed_types();
  test_escape_quotes();
  test_escape_backslash();
  test_escape_whitespace();
  test_escape_binary();
  test_empty_object();
  test_empty_array();
  test_ipc_payload();
  test_all_types();
  test_null_safety();
  
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", tests_passed, tests_passed + tests_failed);
  
  return (tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
