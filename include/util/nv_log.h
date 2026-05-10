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

#ifndef NV_LOG_H
#define NV_LOG_H

#include <stddef.h>

/* Visibility macros when not defined by build.
 *
 * NV_SHARED_BUILD — defined by the nativeview_shared CMake target when
 *                   compiling the shared library itself (exports symbols).
 * NV_SHARED       — defined by consumers who link against the shared DLL/SO
 *                   (imports symbols on Windows).
 * Neither defined  — static linking, no decoration needed.
 */
#ifndef NV_API
#  if defined(NV_SHARED_BUILD)
#    if defined(_WIN32) || defined(__CYGWIN__)
#      define NV_API __declspec(dllexport)
#    else
#      define NV_API __attribute__((visibility("default")))
#    endif
#  elif defined(NV_SHARED)
#    if defined(_WIN32) || defined(__CYGWIN__)
#      define NV_API __declspec(dllimport)
#    else
#      define NV_API
#    endif
#  else
#    define NV_API
#  endif
#endif
#ifndef NV_INTERNAL
#define NV_INTERNAL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Internal log/assert implementation (defined in nv_log.c) */
void nv_log_debug(const char* file, int line, const char* fmt, ...);
void nv_log_error(const char* file, int line, const char* fmt, ...);
void nv_assert_fail(const char* expr, const char* file, int line);

/* Macros for call sites */
#define NV_LOG(fmt, ...)   nv_log_debug(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NV_ERR(fmt, ...)   nv_log_error(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define NV_ASSERT(expr)    do { if (!(expr)) nv_assert_fail(#expr, __FILE__, __LINE__); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NV_LOG_H */
