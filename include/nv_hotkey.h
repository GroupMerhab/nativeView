/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_hotkey.h — System-wide (global) hotkeys per window
 *
 * Shortcuts are registered with the OS and fire even when the window is not
 * focused. On Linux under Wayland there is no portable global-hotkey API;
 * nv_window_register_hotkey returns NV_HOTKEY_ERR_NOT_SUPPORTED when the
 * GDK backend is Wayland.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_HOTKEY_H
#define NV_HOTKEY_H

#include "nv_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque window handle (same as nv.h). */
typedef struct nv_window nv_window_t;

/** Success */
#define NV_HOTKEY_OK 0
/** NULL window, NULL id/combo, malformed id, or unparsable combo */
#define NV_HOTKEY_ERR_INVALID -1
/** Same window already has a hotkey with this id */
#define NV_HOTKEY_ERR_ALREADY_EXISTS -2
/** Platform does not support global hotkeys (e.g. Linux Wayland) */
#define NV_HOTKEY_ERR_NOT_SUPPORTED -3
/** OS refused registration (e.g. hotkey owned by another app) */
#define NV_HOTKEY_ERR_PLATFORM -4

/**
 * Register a global hotkey for \p w.
 *
 * @param w     Window used for routing events (NULL-safe → NV_HOTKEY_ERR_INVALID)
 * @param id    Non-empty identifier [A-Za-z0-9_.-] (NULL-safe)
 * @param combo Accelerator string, e.g. "CmdOrCtrl+Shift+K", "Alt+F4"
 * @return      NV_HOTKEY_* code
 */
NV_API int nv_window_register_hotkey(nv_window_t* w, const char* id,
                                     const char* combo);

/**
 * Unregister a hotkey by id (NULL-safe; unknown id is a no-op).
 */
NV_API void nv_window_unregister_hotkey(nv_window_t* w, const char* id);

#ifdef __cplusplus
}
#endif

#endif /* NV_HOTKEY_H */
