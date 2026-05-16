/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/*
 * Test nv_shm create/destroy, write, read-back.
 */
#include "nv_shm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  return 1;
}

int main(void) {
  nv_shm_t* shm = nv_shm_create("test_shm", 16);
  if (!shm) return fail("nv_shm_create");

  if (!nv_shm_ptr(shm)) return fail("nv_shm_ptr NULL");
  if (nv_shm_size(shm) != 16) return fail("nv_shm_size");
  if (!nv_shm_name(shm) || strcmp(nv_shm_name(shm), "test_shm") != 0)
    return fail("nv_shm_name");

  if (nv_shm_write_f32(shm, 0, 1.5f) != 0) return fail("nv_shm_write_f32");
  float* p = (float*)nv_shm_ptr(shm);
  if (p[0] != 1.5f) return fail("read-back 1.5");
  if (nv_shm_write_f32(shm, 4, -2.25f) != 0) return fail("nv_shm_write_f32 offset");
  if (p[1] != -2.25f) return fail("read-back -2.25");

  if (nv_shm_write_f32(shm, 1, 0.0f) != -1) return fail("unaligned offset should fail");
  if (nv_shm_write_f32(NULL, 0, 0.0f) != -1) return fail("NULL handle should fail");

  nv_shm_destroy(shm);
  nv_shm_destroy(NULL);  /* NULL-safe */

  printf("test_shm: PASS\n");
  return 0;
}
