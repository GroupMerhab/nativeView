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

/* Internal utilities */

/* Enable POSIX realtime functions (clock_gettime) on glibc 2.34+ */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "nv_util.h"
#include "util/nv_log.h"
#include "nv_version_generated.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

/* =============================================================================
 * Version Information
 * =============================================================================
 */

NV_API const char* nv_version_string(void) {
  return NV_VERSION_STRING;
}

NV_API void nv_get_version_info(NvVersionInfo* out_info) {
  if (!out_info) return;
  out_info->major = NV_VERSION_MAJOR;
  out_info->minor = NV_VERSION_MINOR;
  out_info->patch = NV_VERSION_PATCH;
  out_info->build_sha = NV_BUILD_GIT_SHA;
}

NV_API uint64_t nv_bench_now(void) {
#if defined(_WIN32)
  static LARGE_INTEGER freq = {0};
  LARGE_INTEGER counter;
  if (!freq.QuadPart) {
    QueryPerformanceFrequency(&freq);
  }
  QueryPerformanceCounter(&counter);
  long double seconds = (long double)counter.QuadPart / (long double)freq.QuadPart;
  return (uint64_t)(seconds * 1000000000.0L);
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}
