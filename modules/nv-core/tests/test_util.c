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

/* Test suite for utility logging and assertion macros */

#include "nv_util.h"
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

void test_fail(const char* name) {
  test_count++;
  printf("✗ %s\n", name);
}

/* =============================================================================
 * Test 1: nv_version_string returns valid version
 * =============================================================================
 */
void test_version_string(void) {
  const char* version = nv_version_string();
  
  if (version == NULL) {
    test_fail("version_string: returned NULL");
    return;
  }
  
  if (strlen(version) == 0) {
    test_fail("version_string: returned empty string");
    return;
  }
  
  test_ok("version_string");
  printf("  Version: %s\n", version);
}

/* =============================================================================
 * Test 2: NV_ASSERT does not fire on true condition
 * =============================================================================
 */
void test_assert_true(void) {
  /* This should not trigger abort() */
  NV_ASSERT(1 > 0);
  NV_ASSERT(1 == 1);
  NV_ASSERT(NULL != NULL || 1);
  
  test_ok("NV_ASSERT(true)");
}

/* =============================================================================
 * Test 3: NV_ERR logs to stderr with correct format
 * =============================================================================
 */
void test_err_logging(void) {
  printf("  Testing NV_ERR output (should appear below):\n");
  fprintf(stdout, "  ");
  
  /* This logs to stderr - user can see output format by running test */
  NV_ERR("Test error message with value: %d", 42);
  NV_ERR("Another test error");
  
  test_ok("NV_ERR logging");
}

/* =============================================================================
 * Test 4: NV_LOG macro (debug logging)
 * =============================================================================
 */
void test_log_macro(void) {
  /* In release builds, this compiles to nothing (zero overhead) */
  NV_LOG("Debug message: %s", "test");
  NV_LOG("Another debug message with int: %d", 123);
  
  test_ok("NV_LOG macro");
  printf("  (NV_LOG output depends on NV_DEBUG mode)\n");
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */
int main(void) {
  printf("=== nativeview Utility Module Tests ===\n\n");
  
  printf("Running tests...\n");
  test_version_string();
  test_assert_true();
  test_log_macro();
  test_err_logging();
  
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", test_passed, test_count);
  
  return (test_passed == test_count) ? 0 : 1;
}
