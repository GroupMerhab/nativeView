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

/* Test suite for arena memory allocator */

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
 * Test 1: NULL arena safety
 * =============================================================================
 */
void test_null_arena(void) {
  void* ptr = nv_arena_alloc(NULL, 100);
  if (ptr != NULL) {
    test_fail("null_arena", "expected NULL from NULL arena");
    return;
  }
  
  nv_arena_reset(NULL);  /* Should not crash */
  nv_arena_destroy(NULL);  /* Should not crash */
  
  test_ok("null_arena");
}

/* =============================================================================
 * Test 2: Create and destroy arena
 * =============================================================================
 */
void test_create_destroy(void) {
  nv_arena_t* arena = nv_arena_create(1024);
  if (arena == NULL) {
    test_fail("create_destroy", "arena creation failed");
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("create_destroy");
}

/* =============================================================================
 * Test 3: Normal allocation and use
 * =============================================================================
 */
void test_normal_alloc(void) {
  nv_arena_t* arena = nv_arena_create(1024);
  if (arena == NULL) {
    test_fail("normal_alloc", "arena creation failed");
    return;
  }
  
  void* ptr1 = nv_arena_alloc(arena, 64);
  if (ptr1 == NULL) {
    test_fail("normal_alloc", "first allocation returned NULL");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Verify pointer is properly aligned (8 bytes) */
  if (((uintptr_t)ptr1 & 7) != 0) {
    test_fail("normal_alloc", "pointer not 8-byte aligned");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Write and read back */
  *(int*)ptr1 = 42;
  if (*(int*)ptr1 != 42) {
    test_fail("normal_alloc", "write/read failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("normal_alloc");
}

/* =============================================================================
 * Test 4: Multiple sequential allocations
 * =============================================================================
 */
void test_sequential_allocs(void) {
  nv_arena_t* arena = nv_arena_create(512);
  if (arena == NULL) {
    test_fail("sequential", "arena creation failed");
    return;
  }
  
  void* ptrs[5];
  for (int i = 0; i < 5; i++) {
    ptrs[i] = nv_arena_alloc(arena, 64);
    if (ptrs[i] == NULL) {
      test_fail("sequential", "allocation failed");
      nv_arena_destroy(arena);
      return;
    }
    
    /* Verify non-overlapping allocations */
    for (int j = 0; j < i; j++) {
      if (ptrs[i] == ptrs[j]) {
        test_fail("sequential", "duplicate pointer returned");
        nv_arena_destroy(arena);
        return;
      }
    }
    
    /* Write distinct values */
    *(int*)ptrs[i] = i * 100;
  }
  
  /* Verify values are preserved */
  for (int i = 0; i < 5; i++) {
    if (*(int*)ptrs[i] != i * 100) {
      test_fail("sequential", "value corruption");
      nv_arena_destroy(arena);
      return;
    }
  }
  
  nv_arena_destroy(arena);
  test_ok("sequential_allocs");
}

/* =============================================================================
 * Test 5: Arena reset and reuse
 * =============================================================================
 */
void test_reset_reuse(void) {
  nv_arena_t* arena = nv_arena_create(256);
  if (arena == NULL) {
    test_fail("reset_reuse", "arena creation failed");
    return;
  }
  
  /* First cycle: allocate and fill */
  void* ptr1 = nv_arena_alloc(arena, 100);
  if (ptr1 == NULL) {
    test_fail("reset_reuse", "first allocation failed");
    nv_arena_destroy(arena);
    return;
  }
  *(int*)ptr1 = 111;
  
  /* Reset */
  nv_arena_reset(arena);
  
  /* Second cycle: allocate and fill (should reuse same location) */
  void* ptr2 = nv_arena_alloc(arena, 100);
  if (ptr2 == NULL) {
    test_fail("reset_reuse", "allocation after reset failed");
    nv_arena_destroy(arena);
    return;
  }
  
  if (ptr2 != ptr1) {
    test_fail("reset_reuse", "reset did not reuse memory");
    nv_arena_destroy(arena);
    return;
  }
  
  *(int*)ptr2 = 222;
  if (*(int*)ptr2 != 222) {
    test_fail("reset_reuse", "write/read after reset failed");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("reset_reuse");
}

/* =============================================================================
 * Test 6: Overflow returns NULL without crash
 * =============================================================================
 */
void test_overflow_safe(void) {
  nv_arena_t* arena = nv_arena_create(100);
  if (arena == NULL) {
    test_fail("overflow_safe", "arena creation failed");
    return;
  }
  
  /* Allocate most of the arena */
  void* ptr1 = nv_arena_alloc(arena, 80);
  if (ptr1 == NULL) {
    test_fail("overflow_safe", "first allocation failed");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Try to allocate beyond capacity - should return NULL */
  void* ptr2 = nv_arena_alloc(arena, 100);
  if (ptr2 != NULL) {
    test_fail("overflow_safe", "overflow should return NULL, not allocation");
    nv_arena_destroy(arena);
    return;
  }
  
  /* Arena should still be valid for smaller allocation */
  void* ptr3 = nv_arena_alloc(arena, 10);
  if (ptr3 == NULL) {
    test_fail("overflow_safe", "arena corrupted after overflow attempt");
    nv_arena_destroy(arena);
    return;
  }
  
  nv_arena_destroy(arena);
  test_ok("overflow_safe");
}

/* =============================================================================
 * Test 7: Pool arena with multiple chunks
 * =============================================================================
 */
void test_pool_multiple_chunks(void) {
  /* Each chunk is 100 bytes, 10 chunks available.
     Each allocation of 30 bytes will be aligned to 32 bytes.
     So we can fit about 3 allocations per chunk.
     10 allocations should span 4 chunks. */
  nv_arena_t* arena = nv_arena_create_pool_growable(100, 10);
  if (arena == NULL) {
    test_fail("pool_chunks", "pool creation failed");
    return;
  }
  
  /* Allocate across multiple chunks */
  void* ptrs[10];
  for (int i = 0; i < 10; i++) {
    ptrs[i] = nv_arena_alloc(arena, 30);
    if (ptrs[i] == NULL) {
      test_fail("pool_chunks", "allocation failed");
      nv_arena_destroy(arena);
      return;
    }
    
    *(int*)ptrs[i] = i * 10;
  }
  
  /* Verify all values preserved (proves multiple chunks work) */
  for (int i = 0; i < 10; i++) {
    if (*(int*)ptrs[i] != i * 10) {
      test_fail("pool_chunks", "value corruption in pool");
      nv_arena_destroy(arena);
      return;
    }
  }
  
  nv_arena_destroy(arena);
  test_ok("pool_multiple_chunks");
}

/* =============================================================================
 * Test 8: Pool arena growth (growable never exhausts; allocates more chunks)
 * =============================================================================
 */
void test_pool_growth(void) {
  nv_arena_t* arena = nv_arena_create_pool_growable(100, 2);
  if (arena == NULL) {
    test_fail("pool_growth", "pool creation failed");
    return;
  }
  /* Fill 2 chunks then allocate a third - growable arena should succeed */
  void* ptr1 = nv_arena_alloc(arena, 80);
  void* ptr2 = nv_arena_alloc(arena, 80);
  void* ptr3 = nv_arena_alloc(arena, 80);
  if (ptr1 == NULL || ptr2 == NULL || ptr3 == NULL) {
    test_fail("pool_growth", "allocation failed (growable should not exhaust)");
    nv_arena_destroy(arena);
    return;
  }
  nv_arena_destroy(arena);
  test_ok("pool_growth");
}

/* =============================================================================
 * Test 9: Alignment enforcement
 * =============================================================================
 */
void test_alignment(void) {
  nv_arena_t* arena = nv_arena_create(512);
  if (arena == NULL) {
    test_fail("alignment", "arena creation failed");
    return;
  }
  
  /* Test various sizes */
  int sizes[] = {1, 7, 8, 9, 15, 16, 17, 63, 64, 65};
  for (int i = 0; i < 10; i++) {
    void* ptr = nv_arena_alloc(arena, sizes[i]);
    if (ptr == NULL) {
      test_fail("alignment", "allocation failed");
      nv_arena_destroy(arena);
      return;
    }
    
    if (((uintptr_t)ptr & 7) != 0) {
      test_fail("alignment", "non-aligned pointer returned");
      nv_arena_destroy(arena);
      return;
    }
  }
  
  nv_arena_destroy(arena);
  test_ok("alignment");
}

/* =============================================================================
 * Test 10: nv_arena_str_dup
 * =============================================================================
 */
void test_str_dup(void) {
  nv_arena_t* arena = nv_arena_create(256);
  char* a;
  char* b;
  char* c;

  if (arena == NULL) {
    test_fail("str_dup", "arena creation failed");
    return;
  }

  if (nv_arena_str_dup(NULL, "x") != NULL) {
    test_fail("str_dup", "expected NULL for NULL arena");
    nv_arena_destroy(arena);
    return;
  }

  a = nv_arena_str_dup(arena, "hello");
  b = nv_arena_str_dup(arena, NULL);
  c = nv_arena_str_dup(arena, "");
  if (!a || strcmp(a, "hello") != 0) {
    test_fail("str_dup", "dup normal string");
    nv_arena_destroy(arena);
    return;
  }
  if (!b || strcmp(b, "") != 0) {
    test_fail("str_dup", "dup NULL source -> empty");
    nv_arena_destroy(arena);
    return;
  }
  if (!c || c[0] != '\0') {
    test_fail("str_dup", "dup empty");
    nv_arena_destroy(arena);
    return;
  }

  nv_arena_destroy(arena);
  test_ok("str_dup");
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */
int main(void) {
  printf("=== nativeview Arena Allocator Tests ===\n\n");
  
  printf("Running tests...\n");
  test_null_arena();
  test_create_destroy();
  test_normal_alloc();
  test_sequential_allocs();
  test_reset_reuse();
  test_overflow_safe();
  test_pool_multiple_chunks();
  test_pool_growth();
  test_alignment();
  test_str_dup();
  
  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", test_passed, test_count);
  
  return (test_passed == test_count) ? 0 : 1;
}

