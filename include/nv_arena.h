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

/* Fast bump-pointer arena allocator for IPC and message processing */

#ifndef NV_ARENA_H
#define NV_ARENA_H

#include <stddef.h>
#include "nv_util.h"

/**
 * Opaque arena allocator handle.
 * Arenas are NOT thread-safe; use separate arenas per thread or provide
 * external synchronization.
 */
typedef struct nv_arena nv_arena_t;

/* =============================================================================
 * Fixed Arena API
 * =============================================================================
 *
 * Single pre-allocated buffer with bump-pointer allocation.
 * Suitable for window-local allocation during message processing.
 */

/**
 * Create a fixed-size arena allocator.
 * Allocates a contiguous buffer of exactly `capacity` bytes and initializes
 * the bump pointer to the beginning. Returns NULL on allocation failure.
 *
 * @param capacity  Size in bytes of the arena buffer
 * @return          Pointer to new arena, or NULL on failure
 */
NV_API nv_arena_t* nv_arena_create(size_t capacity);

/**
 * Allocate memory from arena.
 * Allocates `size` bytes with 8-byte alignment. On overflow (insufficient
 * space remaining), logs error via NV_ERR and returns NULL without crashing.
 *
 * @param arena  Arena to allocate from
 * @param size   Number of bytes to allocate
 * @return       Pointer to allocated memory (8-byte aligned), or NULL on failure
 */
NV_API void* nv_arena_alloc(nv_arena_t* arena, size_t size);

/**
 * Reset arena to empty state.
 * Resets the bump pointer to the beginning of the buffer, allowing reuse
 * of all previously allocated space. Useful for message-by-message cycles.
 *
 * @param arena  Arena to reset
 */
NV_API void nv_arena_reset(nv_arena_t* arena);

/**
 * Destroy arena and free underlying buffer.
 * After this call, the arena pointer is invalid and must not be used.
 *
 * @param arena  Arena to destroy (NULL is safe)
 */
NV_API void nv_arena_destroy(nv_arena_t* arena);

/**
 * Copy a null-terminated string into the arena (including the terminator).
 *
 * @param arena  Arena to allocate from (NULL → NULL)
 * @param str    Source string; NULL is treated as empty string
 * @return       Pointer to copy in arena, or NULL on overflow / NULL arena
 */
NV_API char* nv_arena_str_dup(nv_arena_t* arena, const char* str);

/* =============================================================================
 * Pool Arena API
 * =============================================================================
 *
 * Growable arena with chunk-based allocation.
 * Allocates fixed-size chunks on demand up to a maximum limit.
 * Suitable for unbounded message sizes with safety limits.
 */

/**
 * Create a growable pool arena.
 * Maintains chunks of size `chunk_size`; reallocates the chunks array when full,
 * allowing unbounded growth.
 *
 * @param chunk_size         Size of each chunk in bytes
 * @param initial_max_chunks Initial capacity (0 = use 4)
 * @return                   Pointer to new pool arena, or NULL on failure
 */
NV_API nv_arena_t* nv_arena_create_pool_growable(size_t chunk_size, size_t initial_max_chunks);

#endif /* NV_ARENA_H */
