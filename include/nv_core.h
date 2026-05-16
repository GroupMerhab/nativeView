/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_core.h - Core Library Management
 *
 * Library initialization, shutdown, and global state management.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_CORE_H
#define NV_CORE_H

#include "nv_arena.h"
#include "nv_window.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Types
 * =============================================================================
 */

/* Global nativeview context */
typedef struct nv_context nv_context_t;

/* =============================================================================
 * Lifecycle
 * =============================================================================
 */

/**
 * Initialize nativeview library and create a global context.
 *
 * @return Global context, NULL on initialization error
 * @note Call this before using any other nativeview APIs
 */
NV_API nv_context_t* nv_init(void);

/**
 * Shutdown nativeview library and release all resources.
 *
 * @param ctx Global context
 * @note All windows must be closed before calling this
 */
NV_API void nv_shutdown(nv_context_t* ctx);

/* =============================================================================
 * Default Window Creation
 * =============================================================================
 */

/**
 * Create main window for application.
 *
 * @param ctx Global context
 * @param title Window title
 * @param width Initial width in pixels
 * @param height Initial height in pixels
 * @return Window handle, NULL on error
 */
NV_API nv_window_t* nv_create_window(nv_context_t* ctx, const char* title, int width, int height);

/**
 * Create main window with full configuration.
 *
 * @param ctx Global context
 * @param config Window configuration
 * @return Window handle, NULL on error
 */
NV_API nv_window_t* nv_create_window_with_config(nv_context_t* ctx, const nv_window_config_t* config);

/* =============================================================================
 * Event Loop
 * =============================================================================
 */

/**
 * Run the event loop (blocking call).
 * 
 * Returns when the last window is closed or exit_loop is called.
 *
 * @param ctx Global context
 * @return 0 on normal exit, -1 on error
 */
NV_API int nv_run(nv_context_t* ctx);

/**
 * Exit the event loop (call from callback/message handler).
 *
 * @param ctx Global context
 */
NV_API void nv_exit_loop(nv_context_t* ctx);

/* =============================================================================
 * Version Information
 * =============================================================================
 */

/**
 * Get library version string.
 *
 * @return Version string (e.g., "0.1.0")
 */
NV_API const char* nv_get_version(void);

/**
 * Check if running in debug mode.
 *
 * @return 1 if debug mode, 0 otherwise
 */
NV_API int nv_is_debug(void);

#ifdef __cplusplus
}
#endif

#endif /* NV_CORE_H */
