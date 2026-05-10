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

/* Safe dynamic string builder backed by arena allocator */

#ifndef NV_STR_H
#define NV_STR_H

#include <stddef.h>
#include "nv_arena.h"

/**
 * Opaque string builder handle.
 * Backed by an nv_arena_t; all allocations are zero-copy and come from
 * the arena. No internal malloc/free.
 */
typedef struct nv_str nv_str_t;

/* =============================================================================
 * String Builder API
 * =============================================================================
 */

/**
 * Create a new string builder.
 * Allocates initial buffer (typically 256 bytes) from the provided arena.
 * The string is empty (length 0) but ready to append.
 *
 * @param arena  Arena to allocate string buffer from
 * @return       Pointer to new string builder, or NULL on failure
 */
NV_API nv_str_t* nv_str_create(nv_arena_t* arena);

/**
 * Append a null-terminated string.
 * On overflow (insufficient space remaining in buffer), logs error via NV_ERR
 * and truncates gracefully without modifying allocated memory beyond bounds.
 * Always null-terminates the result.
 *
 * @param s     String builder (NULL-safe: does nothing)
 * @param data  String to append (NULL-safe: does nothing)
 */
NV_API void nv_str_append(nv_str_t* s, const char* data);

/**
 * Append a fixed-length byte buffer.
 * Useful for appending data with embedded nulls or unknown length.
 * On overflow, logs error and truncates without crashing.
 *
 * @param s     String builder (NULL-safe: does nothing)
 * @param data  Bytes to append
 * @param len   Number of bytes to append
 */
NV_API void nv_str_append_len(nv_str_t* s, const char* data, size_t len);

/**
 * Append formatted output (printf-style).
 * Uses vsnprintf internally; on overflow, output is truncated and error logged.
 * Always null-terminates the result.
 *
 * @param s     String builder (NULL-safe: does nothing)
 * @param fmt   Format string (NULL-safe: does nothing)
 * @param ...   Variable arguments per format string
 */
NV_API void nv_str_appendf(nv_str_t* s, const char* fmt, ...);

/**
 * Get pointer to null-terminated string data.
 * Pointer is valid for the lifetime of the string or until append operations.
 * Result is always null-terminated.
 *
 * @param s  String builder
 * @return   Pointer to string data, or "" (empty string) if s is NULL
 */
NV_API const char* nv_str_cstr(const nv_str_t* s);

/**
 * Get current string length (excluding null terminator).
 *
 * @param s  String builder
 * @return   Number of characters (0 if s is NULL)
 */
NV_API size_t nv_str_len(const nv_str_t* s);

/**
 * Reset string to empty state.
 * Resets length to 0 and null-terminates. Buffer capacity is preserved
 * and ready for reuse.
 *
 * @param s  String builder (NULL-safe: does nothing)
 */
NV_API void nv_str_reset(nv_str_t* s);

#endif /* NV_STR_H */
