/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "nv_util.h"
#include "util/nv_log.h"

NV_INTERNAL void nv_log_debug(const char* file, int line, const char* fmt, ...) {
#ifdef NV_DEBUG
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "[NV DEBUG %s:%d] ", file, line);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(args);
#else
  (void)file; (void)line; (void)fmt;
#endif
}

NV_INTERNAL void nv_log_error(const char* file, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "[NV ERROR %s:%d] ", file, line);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(args);
}

NV_INTERNAL void nv_assert_fail(const char* expr, const char* file, int line) {
  nv_log_error(file, line, "Assertion failed: %s", expr);
  abort();
}
