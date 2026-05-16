/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * test_json_parse.c - JSON Parser Tests
 *
 * Test parsing of JSON strings from JS→C IPC messages.
 * Tests: flat objects, nested structures, arrays, malformed input, numbers.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#include "nv_json.h"
#include "nv_arena.h"
#include "nv_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* =============================================================================
 * Test Framework
 * =============================================================================
 */

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg) do { \
  if (!(cond)) { \
    NV_ERR("Test assertion failed: %s", msg); \
    return 0; \
  } \
} while(0)

static int run_test(const char* name, int (*test_fn)(void)) {
  g_tests_run++;
  if (test_fn()) {
    g_tests_passed++;
    printf("✓ %s\n", name);
    return 1;
  } else {
    printf("✗ %s\n", name);
    return 0;
  }
}

/* =============================================================================
 * Tests
 * =============================================================================
 */

static int test_flat_object_all_types(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"str\":\"hello\",\"int\":42,\"dbl\":3.14,\"bool\":true,\"null_val\":null}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  TEST_ASSERT(nv_json_is_object(obj), "is object");
  
  const char* str_val = nv_json_get_str(obj, "str");
  TEST_ASSERT(str_val != NULL && strcmp(str_val, "hello") == 0, "string value");
  
  long long int_val = nv_json_get_int(obj, "int");
  TEST_ASSERT(int_val == 42, "integer value");
  
  double dbl_val = nv_json_get_double(obj, "dbl");
  TEST_ASSERT(dbl_val > 3.13 && dbl_val < 3.15, "double value");
  
  int bool_val = nv_json_get_bool(obj, "bool");
  TEST_ASSERT(bool_val == 1, "boolean value");
  
  nv_arena_destroy(arena);
  return 1;
}


static int test_nested_objects(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"outer\":{\"inner\":\"value\",\"number\":123}}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  
  nv_json_val_t* outer = nv_json_get_obj(obj, "outer");
  TEST_ASSERT(outer != NULL, "get nested object");
  TEST_ASSERT(nv_json_is_object(outer), "nested is object");
  
  const char* inner_val = nv_json_get_str(outer, "inner");
  TEST_ASSERT(inner_val != NULL && strcmp(inner_val, "value") == 0, "nested string");
  
  long long inner_num = nv_json_get_int(outer, "number");
  TEST_ASSERT(inner_num == 123, "nested number");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_nested_objects_and_arrays(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"data\":{\"items\":[1,2,3]}}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  
  nv_json_val_t* data = nv_json_get_obj(obj, "data");
  TEST_ASSERT(data != NULL, "get nested object");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_array_of_primitives(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "[1, 2, 3, 4, 5]";
  nv_json_val_t* arr = nv_json_parse(arena, json);
  TEST_ASSERT(arr != NULL, "parse successful");
  TEST_ASSERT(nv_json_is_array(arr), "is array");
  TEST_ASSERT(nv_json_array_len(arr) == 5, "array length");
  
  nv_json_val_t* elem0 = nv_json_array_get(arr, 0);
  TEST_ASSERT(elem0 != NULL && nv_json_is_number(elem0), "first element");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_array_of_strings(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "[\"one\", \"two\", \"three\"]";
  nv_json_val_t* arr = nv_json_parse(arena, json);
  TEST_ASSERT(arr != NULL, "parse successful");
  TEST_ASSERT(nv_json_is_array(arr), "is array");
  TEST_ASSERT(nv_json_array_len(arr) == 3, "array length");
  
  nv_json_val_t* elem0 = nv_json_array_get(arr, 0);
  TEST_ASSERT(elem0 != NULL && nv_json_is_string(elem0), "string element");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_empty_object(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  TEST_ASSERT(nv_json_is_object(obj), "is object");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_empty_array(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "[]";
  nv_json_val_t* arr = nv_json_parse(arena, json);
  TEST_ASSERT(arr != NULL, "parse successful");
  TEST_ASSERT(nv_json_is_array(arr), "is array");
  TEST_ASSERT(nv_json_array_len(arr) == 0, "length is 0");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_integer(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "42";
  nv_json_val_t* val = nv_json_parse(arena, json);
  TEST_ASSERT(val != NULL, "parse successful");
  TEST_ASSERT(nv_json_is_number(val), "is number");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_negative_integer(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"val\":-100}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  
  long long val = nv_json_get_int(obj, "val");
  TEST_ASSERT(val == -100, "negative integer");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_float_number(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"val\":3.14159}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  
  double val = nv_json_get_double(obj, "val");
  TEST_ASSERT(val > 3.14 && val < 3.15, "float number");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_scientific_notation(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"val\":1.5e-10}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  
  double val = nv_json_get_double(obj, "val");
  TEST_ASSERT(val > 0 && val < 1e-9, "scientific notation");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_escaped_strings(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"str\":\"line1\\nline2\\t\\\"quoted\\\"\"}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  
  const char* str = nv_json_get_str(obj, "str");
  TEST_ASSERT(str != NULL, "get escaped string");
  TEST_ASSERT(strchr(str, '\n') != NULL, "newline escape");
  TEST_ASSERT(strchr(str, '\t') != NULL, "tab escape");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_malformed_missing_brace(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"key\":\"value\"";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj == NULL, "malformed input returns NULL");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_malformed_trailing_comma(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "[1, 2, 3,]";
  nv_json_val_t* arr = nv_json_parse(arena, json);
  TEST_ASSERT(arr == NULL, "trailing comma returns NULL");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_malformed_unterminated_string(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"key\":\"unterminated";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj == NULL, "unterminated string returns NULL");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_malformed_extra_trailing_content(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "[1, 2] extra";
  nv_json_val_t* arr = nv_json_parse(arena, json);
  TEST_ASSERT(arr == NULL, "trailing content returns NULL");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_null_arena(void) {
  const char* json = "{}";
  nv_json_val_t* obj = nv_json_parse(NULL, json);
  TEST_ASSERT(obj == NULL, "NULL arena returns NULL");
  return 1;
}

static int test_null_input(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  nv_json_val_t* obj = nv_json_parse(arena, NULL);
  TEST_ASSERT(obj == NULL, "NULL input returns NULL");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_whitespace_handling(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  /* Parser is lenient - accepts surrounding whitespace */
  const char* json = "  {\n  \"key\"  :  \"value\"  \n}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "whitespace in valid positions");
  TEST_ASSERT(nv_json_is_object(obj), "is object");
  
  const char* str = nv_json_get_str(obj, "key");
  TEST_ASSERT(str != NULL && strcmp(str, "value") == 0, "value retrieved");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_boolean_values(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "{\"t\":true,\"f\":false}";
  nv_json_val_t* obj = nv_json_parse(arena, json);
  TEST_ASSERT(obj != NULL, "parse successful");
  
  int t = nv_json_get_bool(obj, "t");
  int f = nv_json_get_bool(obj, "f");
  TEST_ASSERT(t == 1, "true value");
  TEST_ASSERT(f == 0, "false value");
  
  nv_arena_destroy(arena);
  return 1;
}

static int test_null_value(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  TEST_ASSERT(arena != NULL, "arena creation");
  
  const char* json = "null";
  nv_json_val_t* val = nv_json_parse(arena, json);
  TEST_ASSERT(val != NULL, "parse null successful");
  
  nv_arena_destroy(arena);
  return 1;
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void) {
  printf("\n=== nativeview JSON Parser Tests ===\n\n");
  printf("Running tests...\n");
  
  run_test("flat_object_all_types", test_flat_object_all_types);
  run_test("nested_objects", test_nested_objects);
  run_test("nested_objects_and_arrays", test_nested_objects_and_arrays);
  run_test("array_of_primitives", test_array_of_primitives);
  run_test("array_of_strings", test_array_of_strings);
  run_test("empty_object", test_empty_object);
  run_test("empty_array", test_empty_array);
  run_test("integer", test_integer);
  run_test("negative_integer", test_negative_integer);
  run_test("float_number", test_float_number);
  run_test("scientific_notation", test_scientific_notation);
  run_test("escaped_strings", test_escaped_strings);
  run_test("malformed_missing_brace", test_malformed_missing_brace);
  run_test("malformed_trailing_comma", test_malformed_trailing_comma);
  run_test("malformed_unterminated_string", test_malformed_unterminated_string);
  run_test("malformed_extra_trailing_content", test_malformed_extra_trailing_content);
  run_test("null_arena", test_null_arena);
  run_test("null_input", test_null_input);
  run_test("whitespace_handling", test_whitespace_handling);
  run_test("boolean_values", test_boolean_values);
  run_test("null_value", test_null_value);
  
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n\n", g_tests_passed, g_tests_run);
  
  return g_tests_passed == g_tests_run ? 0 : 1;
}

