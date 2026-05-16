/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_ipc_internal.h - Internal IPC Logic
 *
 * Platform-independent message routing: JS↔C callbacks and JS injection.
 * Not part of public API. Used by platform backends.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_IPC_INTERNAL_H
#define NV_IPC_INTERNAL_H

#include "nv_util.h"
#include "nv_arena.h"
#include "nv.h"
#include "nv_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Opaque IPC State
 * =============================================================================
 */

/** Opaque IPC state for per-window message handling. */
typedef struct nv_ipc_state nv_ipc_state_t;

/* =============================================================================
 * Statistics
 * =============================================================================
 */

/** Message building statistics (for benchmarking). */
typedef struct {
  size_t stack_allocs;  /**< Messages built on stack. */
  size_t heap_allocs;   /**< Messages allocated from arena. */
} nv_ipc_stats_t;

/* =============================================================================
 * IPC State Lifecycle
 * =============================================================================
 */

/**
 * Create IPC state.
 *
 * @param arena  Arena for allocations (required)
 * @return       New IPC state, or NULL on error
 */
NV_INTERNAL nv_ipc_state_t* nv_ipc_state_create(nv_arena_t* arena);

/**
 * Get the pool arena from IPC state.
 *
 * @param state IPC state
 * @return      Pool arena (reset on each dispatch), or NULL
 */
NV_INTERNAL nv_arena_t* nv_ipc_get_pool_arena(nv_ipc_state_t* state);

/* =============================================================================
 * Callback Registration
 * =============================================================================
 */

/**
 * Register message callback.
 *
 * @param state    IPC state (NULL-safe)
 * @param callback Invoked on nv_ipc_dispatch (may be NULL)
 * @param userdata Context pointer
 */
NV_INTERNAL void nv_ipc_set_msg_cb(nv_ipc_state_t* state, nv_msg_cb_t callback,
                                    void* userdata);

/**
 * Register ready callback.
 *
 * @param state    IPC state (NULL-safe)
 * @param callback Invoked when page ready (may be NULL)
 * @param userdata Context pointer
 */
NV_INTERNAL void nv_ipc_set_ready_cb(nv_ipc_state_t* state,
                                      nv_ready_cb_t callback, void* userdata);

/**
 * Register close callback.
 *
 * @param state    IPC state (NULL-safe)
 * @param callback Invoked on window close (may be NULL)
 * @param userdata Context pointer
 */
NV_INTERNAL void nv_ipc_set_close_cb(nv_ipc_state_t* state, nv_close_cb_t callback,
                                      void* userdata);

NV_INTERNAL int nv_ipc_has_close_cb(nv_ipc_state_t* state);

/**
 * Invoke the registered close callback (if any) for a given IPC state.
 *
 * This helper allows callers to trigger the callback without knowing the
 * internals of nv_ipc_state_t, keeping the type opaque.
 */
NV_INTERNAL void nv_ipc_invoke_close_cb(nv_window_t* window, nv_ipc_state_t* state);

/* =============================================================================
 * Message Dispatch
 * =============================================================================
 */

/**
 * Dispatch incoming message from JavaScript.
 *
 * @param window   Window that received message (for callbacks)
 * @param state    IPC state (NULL-safe)
 * @param raw_json Raw JSON payload from JS: {"e":"event","d":{...}}
 *
 * Parses JSON, extracts event name and data, calls registered callback.
 * Gracefully handles malformed input (logs error, doesn't crash).
 */
NV_INTERNAL void nv_ipc_dispatch(nv_window_t* window, nv_ipc_state_t* state,
                                  const char* raw_json);

/**
 * Invoke ready callback for a window.
 */
NV_INTERNAL void nv_ipc_invoke_ready(nv_window_t* window, nv_ipc_state_t* state);

/* =============================================================================
 * JavaScript Support
 * =============================================================================
 */

/**
 * Get JavaScript bootstrap script.
 *
 * @return Static string containing window.__nv IIFE definition.
 *
 * Injected on every page load. Defines:
 * - window.__nv.send(event, data) → posts to native
 * - window.__nv.on(event, fn)     → registers listener
 * - window.__nv._emit(event, json) → called by native to deliver messages
 */
NV_INTERNAL const char* nv_ipc_inject_script(void);

/**
 * Build outgoing message for native→JavaScript delivery.
 *
 * @param arena  Arena for allocations (may be NULL if stack path is used)
 * @param event  Event name
 * @param json   JSON payload (must be valid JSON)
 * @return       Safe to pass to nv_eval_js: "window.__nv._emit(...)"
 *
 * OPTIMIZATION: if result fits in 2048 bytes, builds on stack without arena.
 * Otherwise allocates from arena. Updates stats accordingly.
 */
NV_INTERNAL const char* nv_ipc_build_send(nv_arena_t* arena, const char* event,
                                           const char* json);

/* =============================================================================
 * Statistics
 * =============================================================================
 */

/**
 * Get message building statistics.
 *
 * @return Stats: count of stack vs heap allocations
 */
NV_INTERNAL nv_ipc_stats_t nv_ipc_get_stats(void);

/* =============================================================================
 * Operation registry (for nv-ops; same layout as nv_ipc.c op table)
 * =============================================================================
 */
typedef void (*nv_ipc_op_fn_t)(nv_window_t*, int, const nv_json_val_t*, nv_arena_t*);
typedef struct {
  const char* name;
  nv_ipc_op_fn_t handler;
} nv_ipc_op_entry_t;

NV_INTERNAL void nv_ipc_register_ops(const nv_ipc_op_entry_t* entries, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* NV_IPC_INTERNAL_H */
