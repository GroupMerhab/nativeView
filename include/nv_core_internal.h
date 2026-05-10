/* =============================================================================
 * nv_core_internal.h - Internal App Lifecycle
 *
 * Owns nv_app_t struct layout and platform-independent lifecycle helpers.
 * Platform backends provide nv_app_platform_* implementations.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_CORE_INTERNAL_H
#define NV_CORE_INTERNAL_H

#include "nv_util.h"
#include "nv.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Internal nv_app Structure
 * =============================================================================
 */

/**
 * Internal application structure.
 * Opaque to public API (only forward-declared in nv.h).
 */
struct nv_app {
  void* platform;  /* Platform-specific app state (owned by backend) */
  int   running;   /* 1 while event loop is active */
  /* future: window list, single-instance lock */
};

/* =============================================================================
 * Internal API
 * =============================================================================
 */

/**
 * Allocate and initialize an nv_app instance.
 *
 * - Sets platform = NULL
 * - Sets running = 0
 */
NV_INTERNAL nv_app_t* nv_app_alloc(void);

/**
 * Free nv_app instance.
 * Does NOT call platform quit; caller is responsible for nv_app_platform_quit.
 */
NV_INTERNAL void nv_app_free(nv_app_t* app);

/**
 * Get platform-specific app state.
 */
NV_INTERNAL void* nv_app_get_platform(nv_app_t* app);

/**
 * Set platform-specific app state (ownership transferred to app).
 */
NV_INTERNAL void nv_app_set_platform(nv_app_t* app, void* platform);

/* =============================================================================
 * Platform Hooks (implemented by backends)
 * =============================================================================
 */

/**
 * Initialize platform-specific app state.
 * Called once after nv_app_alloc().
 */
NV_INTERNAL void nv_app_platform_init(nv_app_t* app);

/**
 * Run platform event loop (blocking).
 * Expected to return when nv_app_platform_quit is requested.
 */
NV_INTERNAL void nv_app_platform_run(nv_app_t* app);

/**
 * Request platform event loop to quit.
 * May be called from any thread, depending on backend constraints.
 */
NV_INTERNAL void nv_app_platform_quit(nv_app_t* app);

#ifdef __cplusplus
}
#endif

#endif /* NV_CORE_INTERNAL_H */

/* =============================================================================
 * nv_core_internal.h - Internal App Lifecycle
 *
 * Owns nv_app_t struct layout and platform-independent lifecycle helpers.
 * Platform backends provide nv_app_platform_* implementations.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_CORE_INTERNAL_H
#define NV_CORE_INTERNAL_H

#include "nv_util.h"
#include "nv.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Internal nv_app Structure
 * =============================================================================
 */

/**
 * Internal application structure.
 * Opaque to public API (only forward-declared in nv.h).
 */
struct nv_app {
  void* platform;  /* Platform-specific app state (owned by backend) */
  int   running;   /* 1 while event loop is active */
  /* future: window list, single-instance lock */
};

/* =============================================================================
 * Internal API
 * =============================================================================
 */

/**
 * Allocate and initialize an nv_app instance.
 *
 * - Sets platform = NULL
 * - Sets running = 0
 */
NV_INTERNAL nv_app_t* nv_app_alloc(void);

/**
 * Free nv_app instance.
 * Does NOT call platform quit; caller is responsible for nv_app_platform_quit.
 */
NV_INTERNAL void nv_app_free(nv_app_t* app);

/**
 * Get platform-specific app state.
 */
NV_INTERNAL void* nv_app_get_platform(nv_app_t* app);

/**
 * Set platform-specific app state (ownership transferred to app).
 */
NV_INTERNAL void nv_app_set_platform(nv_app_t* app, void* platform);

/* =============================================================================
 * Platform Hooks (implemented by backends)
 * =============================================================================
 */

/**
 * Initialize platform-specific app state.
 * Called once after nv_app_alloc().
 */
NV_INTERNAL void nv_app_platform_init(nv_app_t* app);

/**
 * Run platform event loop (blocking).
 * Expected to return when nv_app_platform_quit is requested.
 */
NV_INTERNAL void nv_app_platform_run(nv_app_t* app);

/**
 * Request platform event loop to quit.
 * May be called from any thread, depending on backend constraints.
 */
NV_INTERNAL void nv_app_platform_quit(nv_app_t* app);

#ifdef __cplusplus
}
#endif

#endif /* NV_CORE_INTERNAL_H */

