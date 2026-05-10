/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Test suite for string builder */

#include "nv_str.h"
#include "nv_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_count = 0;
int test_passed = 0;

void test_ok(const char* name) {
  test_count++;
  test_passed++;
  printf("✓ %s\n", name);
}

void test_fail(const char* name, const char* reason) {
  test_count++;
  printf("✗ %s: %s\n", name, reason);
}

/* =============================================================================
 * Test 1: Normal append and read
 * =============================================================================
 */
void test_normal_append(void) {
  nv_arena_t* arena = nv_arena_create(1024);
  if (arena == NULL) {
    test_fail("normal_append", "arena creation failed");
    return;
  }
  
  nv_str_t* s = nv_str_create(arena);
  if (s == NULL) {
    test_fail("normal_append", "string creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_str_append(s, "Hello");
  nv_str_append(s, " ");
  nv_str_append(s, "World");
  
  const char* result = nv_str_cstr(s);
  if (strcmp(result, "Hello World") != 0) {
    test_fail("normal_append", "incorrect string value");
    nv_arena_destroy(arena);
    return;
  }
  
  size_t len = nv_str_len(s);
  if (len != 11) {
    test_fail("normal_append", "incorrect string length");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("normal_append");
}

/* =============================================================================
 * Test 2: appendf with format args
 * =============================================================================
 */
void test_appendf(void) {
  nv_arena_t* arena = nv_arena_create(1024);
  if (arena == NULL) {
    test_fail("appendf", "arena creation failed");
    return;
  }
  
  nv_str_t* s = nv_str_create(arena);
  if (s == NULL) {
    test_fail("appendf", "string creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_str_append(s, "Value: ");
  nv_str_appendf(s, "%d", 42);
  nv_str_appendf(s, ", Float: %.2f", 3.14159);
  nv_str_appendf(s, ", Hex: 0x%x", 255);
  
  const char* result = nv_str_cstr(s);
  const char* expected = "Value: 42, Float: 3.14, Hex: 0xff";
  
  if (strcmp(result, expected) != 0) {
    test_fail("appendf", "format string mismatch");
    printf("  Expected: %s\n", expected);
    printf("  Got:      %s\n", result);
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("appendf");
}

/* =============================================================================
 * Test 3: append_len with embedded nulls
 * =============================================================================
 */
void test_append_with_nulls(void) {
  nv_arena_t* arena = nv_arena_create(1024);
  if (arena == NULL) {
    test_fail("append_nulls", "arena creation failed");
    return;
  }
  
  nv_str_t* s = nv_str_create(arena);
  if (s == NULL) {
    test_fail("append_nulls", "string creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Create data with embedded null */
  char data[] = "ABC\x00XYZ";
  nv_str_append_len(s, data, 7);  /* 7 bytes: ABC + null + XYZ */
  
  size_t len = nv_str_len(s);
  if (len != 7) {
    test_fail("append_nulls", "length should be 7 (including embedded null)");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* result = nv_str_cstr(s);
  
  /* Verify the data is correctly preserved */
  if (result[0] != 'A' || result[3] != '\0' || result[4] != 'X') {
    test_fail("append_nulls", "data corruption with embedded null");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("append_with_nulls");
}

/* =============================================================================
 * Test 4: Reset and reuse
 * =============================================================================
 */
void test_reset_reuse(void) {
  nv_arena_t* arena = nv_arena_create(1024);
  if (arena == NULL) {
    test_fail("reset_reuse", "arena creation failed");
    return;
  }
  
  nv_str_t* s = nv_str_create(arena);
  if (s == NULL) {
    test_fail("reset_reuse", "string creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* First use */
  nv_str_append(s, "First");
  if (nv_str_len(s) != 5) {
    test_fail("reset_reuse", "first string length incorrect");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Reset */
  nv_str_reset(s);
  if (nv_str_len(s) != 0) {
    test_fail("reset_reuse", "length not zero after reset");
    nv_arena_destroy(arena);
    return;
  }
  
  if (strcmp(nv_str_cstr(s), "") != 0) {
    test_fail("reset_reuse", "string not empty after reset");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Second use (reuse buffer) */
  nv_str_append(s, "Second");
  if (strcmp(nv_str_cstr(s), "Second") != 0) {
    test_fail("reset_reuse", "reused string incorrect");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("reset_reuse");
}

/* =============================================================================
 * Test 5: Empty string edge case
 * =============================================================================
 */
void test_empty_string(void) {
  nv_arena_t* arena = nv_arena_create(1024);
  if (arena == NULL) {
    test_fail("empty_string", "arena creation failed");
    return;
  }
  
  nv_str_t* s = nv_str_create(arena);
  if (s == NULL) {
    test_fail("empty_string", "string creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Should be empty after creation */
  if (nv_str_len(s) != 0) {
    test_fail("empty_string", "initial string not empty");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* result = nv_str_cstr(s);
  if (result == NULL || result[0] != '\0') {
    test_fail("empty_string", "cstr not properly null-terminated");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Test append NULL data (should be safe) */
  nv_str_append(s, NULL);
  if (nv_str_len(s) != 0) {
    test_fail("empty_string", "NULL append should do nothing");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("empty_string");
}

/* =============================================================================
 * Test 6: Truncation on overflow (safe handling)
 * =============================================================================
 */
void test_overflow_safe(void) {
  /* Create arena large enough for string object + initial buffer,
     but too small for large appends */
  nv_arena_t* arena = nv_arena_create(512);
  if (arena == NULL) {
    test_fail("overflow_safe", "arena creation failed");
    return;
  }
  
  nv_str_t* s = nv_str_create(arena);
  if (s == NULL) {
    test_fail("overflow_safe", "string creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* First, have some data in the string */
  nv_str_append(s, "Start");
  
  /* Try to append a very large string that will exceed arena space */
  char large_data[300];
  memset(large_data, 'A', sizeof(large_data) - 1);
  large_data[299] = '\0';
  
  /* This should fail gracefully, not crash */
  nv_str_append(s, large_data);
  
  /* String should still be valid (the "Start" part should remain)*/
  const char* result = nv_str_cstr(s);
  if (result == NULL) {
    test_fail("overflow_safe", "cstr returned NULL after overflow");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Should still contain at least "Start" */
  if (strstr(result, "Start") == NULL) {
    test_fail("overflow_safe", "original content lost after overflow");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Should not have crashed */
  nv_arena_destroy(arena);
  test_ok("overflow_safe");
}

/* =============================================================================
 * Test 7: NULL safety
 * =============================================================================
 */
void test_null_safety(void) {
  /* All these should be safe (no-ops) */
  nv_str_append(NULL, "test");
  nv_str_append(NULL, NULL);
  nv_str_append_len(NULL, "test", 4);
  nv_str_appendf(NULL, "format");
  nv_str_reset(NULL);
  
  /* These should return safe values */
  const char* empty_str = nv_str_cstr(NULL);
  if (empty_str == NULL || strcmp(empty_str, "") != 0) {
    test_fail("null_safety", "cstr(NULL) should return empty string");
    return;
  }
  
  size_t len = nv_str_len(NULL);
  if (len != 0) {
    test_fail("null_safety", "len(NULL) should return 0");
    return;
  }
  
  test_ok("null_safety");
}

/* =============================================================================
 * Test 8: Large string building
 * =============================================================================
 */
void test_large_build(void) {
  nv_arena_t* arena = nv_arena_create(4096);
  if (arena == NULL) {
    test_fail("large_build", "arena creation failed");
    return;
  }
  
  nv_str_t* s = nv_str_create(arena);
  if (s == NULL) {
    test_fail("large_build", "string creation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Build a moderately large string */
  for (int i = 0; i < 100; i++) {
    nv_str_appendf(s, "Line%d;", i);
  }
  
  size_t len = nv_str_len(s);
  if (len == 0) {
    test_fail("large_build", "string is empty");
    nv_arena_destroy(arena);
    return;
  }
  
  const char* result = nv_str_cstr(s);
  if (strstr(result, "Line0;") == NULL || strstr(result, "Line99;") == NULL) {
    test_fail("large_build", "string content verification failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("large_build");
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */
int main(void) {
  printf("=== nativeview String Builder Tests ===\n\n");
  
  printf("Running tests...\n");
  test_normal_append();
  test_appendf();
  test_append_with_nulls();
  test_reset_reuse();
  test_empty_string();
  test_overflow_safe();
  test_null_safety();
  test_large_build();
  
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", test_passed, test_count);
  
  return (test_passed == test_count) ? 0 : 1;
}

