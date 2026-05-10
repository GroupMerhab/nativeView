/*
 * Benchmark: nv_shm write latency vs theoretical IPC round-trip.
 *
 * Measures: nv_shm_write_f32() call latency (C→memory).
 * Comparison: postMessage round-trip is typically 1–4 ms; shm write is ~0.001 ms.
 *
 * Apache 2.0 - See LICENSE for details
 */
#include "nv_shm.h"
#include "nv_util.h"
#include <stdio.h>
#include <stdlib.h>

#define ITER 10000
#define WARMUP 100

int main(void) {
  nv_shm_t* shm = nv_shm_create("bench_shm", 16);
  if (!shm) {
    fprintf(stderr, "nv_shm_create failed\n");
    return 1;
  }

  /* Warmup */
  for (int i = 0; i < WARMUP; i++)
    nv_shm_write_f32(shm, 0, (float)i);

  uint64_t t0 = nv_bench_now();
  for (int i = 0; i < ITER; i++)
    nv_shm_write_f32(shm, 0, (float)i);
  uint64_t dt = nv_bench_now() - t0;

  double avg_us = (double)dt / (double)ITER;
  printf("nv_shm_write_f32: %d iterations, %llu us total, %.3f us/op\n",
         ITER, (unsigned long long)dt, avg_us);
  printf("(postMessage round-trip typically 1000–4000 us)\n");

  nv_shm_destroy(shm);
  return 0;
}
