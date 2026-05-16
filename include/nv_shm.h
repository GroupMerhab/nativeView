/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_shm.h - Shared Memory for Fast C↔JS Data Path
 *
 * Creates memory-mapped regions accessible from C; platforms may expose them
 * to JavaScript as SharedArrayBuffer for Atomics.load in requestAnimationFrame.
 * Use case: slider live preview, 60fps live data feeds (sub-millisecond reads).
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_SHM_H
#define NV_SHM_H

#include "nv_util.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque shared memory handle */
typedef struct nv_shm nv_shm_t;

/**
 * Create shared memory region (POSIX: shm_open+mmap, Windows: CreateFileMapping).
 *
 * @param name   Unique name for the region (e.g. "slider_vol"). Used as key.
 * @param size   Size in bytes. Must be multiple of 4 for float access.
 * @return       New handle, or NULL on error.
 */
NV_API nv_shm_t* nv_shm_create(const char* name, size_t size);

/**
 * Destroy shared memory and release resources.
 *
 * @param shm   Handle from nv_shm_create (NULL-safe).
 */
NV_API void nv_shm_destroy(nv_shm_t* shm);

/**
 * Get writable pointer to shared memory.
 *
 * @param shm   Handle
 * @return      Pointer to memory, or NULL if invalid.
 */
NV_API void* nv_shm_ptr(nv_shm_t* shm);

/**
 * Get size in bytes.
 *
 * @param shm   Handle
 * @return      Size passed to nv_shm_create, or 0 if invalid.
 */
NV_API size_t nv_shm_size(nv_shm_t* shm);

/**
 * Write float at byte offset (must be 4-byte aligned).
 *
 * @param shm    Handle
 * @param offset Byte offset (0, 4, 8, ...)
 * @param value  Float value
 * @return       0 on success, -1 on error (bad offset or invalid handle)
 */
NV_API int nv_shm_write_f32(nv_shm_t* shm, size_t offset, float value);

/**
 * Get the name used at creation (for platform to register with JS).
 *
 * @param shm   Handle
 * @return      Name string, or NULL if invalid.
 */
NV_API const char* nv_shm_name(nv_shm_t* shm);

#ifdef __cplusplus
}
#endif

#endif /* NV_SHM_H */
