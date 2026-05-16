/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_window_internal.h - Internal Window Management
 *
 * Window struct layout and platform-independent state management.
 * Not part of public API. Used by platform backends.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_WINDOW_INTERNAL_H
#define NV_WINDOW_INTERNAL_H

#include "nv_util.h"
#include "nv_arena.h"
#include "nv_ipc_internal.h"
#include "nv.h"
#include "nv_menu.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Window Structure (opaque in public API)
 * =============================================================================
 */

/**
 * Internal window structure.
 * Allocated per-window, owns all resources.
 * Platform backend stores native handles via platform pointer.
 *
 * NOTE: This struct is intentionally NOT exposed in `nv.h`. The public
 * `nv_window_t` is an opaque forward declaration there.
 */
/** Parsed accelerator for global hotkeys (internal). */
struct nv_hotkey_combo {
  uint32_t mod_flags; /**< NV_HOTKEY_MOD_* bitmask */
  int is_fn;          /**< 1 = function key, 0 = letter/digit/named key */
  int fn_index;       /**< 1–24 when is_fn */
  char key_char;      /**< lowercase letter a–z, digit 0–9, or 0 when is_fn / named */
  int special;        /**< NV_HOTKEY_SPECIAL_* when !is_fn && key_char==0 */
};
typedef struct nv_hotkey_combo nv_hotkey_combo_t;

#define NV_HOTKEY_MOD_SHIFT 1u
#define NV_HOTKEY_MOD_CTRL 2u
#define NV_HOTKEY_MOD_ALT 4u
#define NV_HOTKEY_MOD_META 8u /**< Command (mac) / Win key (Windows) / Super (X11) */
#define NV_HOTKEY_MOD_CMD_OR_CTRL 16u /**< Maps to Command on macOS, Ctrl on Windows/Linux */

#define NV_HOTKEY_SPECIAL_NONE 0
#define NV_HOTKEY_SPECIAL_SPACE 1
#define NV_HOTKEY_SPECIAL_TAB 2
#define NV_HOTKEY_SPECIAL_RETURN 3
#define NV_HOTKEY_SPECIAL_ESCAPE 4

typedef struct nv_hotkey_slot {
  long long handle;
  const char* id; /**< arena-backed */
} nv_hotkey_slot_t;

struct nv_window {
  nv_app_t*        app;       /* Parent application context (may be NULL) */
  nv_window_cfg_t  cfg;       /* Validated copy of user config */
  nv_ipc_state_t*  ipc;       /* Owned IPC state for message routing */
  nv_arena_t*      arena;     /* Per-window pool arena (4KB chunks, max 16) */
  void*            platform;  /* Platform-specific handle (opaque, owned) */
  int              visible;   /* 1 = visible, 0 = hidden */
  /** WebView context menu from `window.setContextMenu` (arena-backed; 0 = none). */
  nv_menu_item_t*  ctx_menu_items;
  int              ctx_menu_count;
  nv_hotkey_slot_t* hotkey_slots;
  int hotkey_slot_count;
};

/* =============================================================================
 * Window Lifecycle (Internal API)
 * =============================================================================
 */

/**
 * Allocate and initialize window from configuration.
 *
 * CONFIG VALIDATION (performed on an internal copy of config):
 * - NULL title        → default "App",      NV_LOG warning
 * - width/height <= 0 → default 800x600,    NV_LOG warning
 * - min_width > width → clamp to width,     NV_LOG warning
 * - min_height > height → clamp to height,  NV_LOG warning
 *
 * RESOURCE OWNERSHIP:
 * - Creates per-window pool arena (4KB chunks, max 16 chunks)
 * - Creates IPC state via nv_ipc_state_create
 * - Platform handle left as NULL (set by platform backend)
 *
 * @param app    Parent application context
 * @param config Window configuration
 * @return       New window handle, NULL on error
 */
NV_INTERNAL nv_window_t* nv_window_alloc(nv_app_t* app,
                                          const nv_window_cfg_t* config);

/**
 * Destroy window and free all owned resources.
 *
 * Cleanup order:
 * 1. IPC state freed (platform uses it for last message)
 * 2. Pool arena freed
 * 3. Window struct freed
 *
 * @param window Window to destroy (NULL-safe)
 */
NV_INTERNAL void nv_window_free(nv_window_t* window);

/* =============================================================================
 * Platform Handle Access
 * =============================================================================
 */

/**
 * Get platform-specific handle from window.
 *
 * @param window Window handle
 * @return       Platform handle (opaque void*), NULL if not set
 */
NV_INTERNAL void* nv_window_get_platform(nv_window_t* window);

/**
 * Set platform-specific handle in window.
 * Called by platform backend during creation.
 *
 * @param window   Window handle
 * @param platform Opaque platform handle (ownership transferred to window)
 */
NV_INTERNAL void nv_window_set_platform(nv_window_t* window, void* platform);

/* =============================================================================
 * Resource Access
 * =============================================================================
 */

/**
 * Get IPC state from window.
 * Used by platform backends to dispatch incoming messages.
 *
 * @param window Window handle
 * @return       IPC state (owned by window, valid lifetime of window)
 */
NV_INTERNAL nv_ipc_state_t* nv_window_get_ipc(nv_window_t* window);

/** Application menu: nv_mac_menu.m, nv_linux.c, or no-op stubs in nv_win.c */
NV_INTERNAL void nv_mac_app_set_menu(const nv_menu_item_t* items, int count);
NV_INTERNAL void nv_linux_app_set_menu(nv_window_t* w, const nv_menu_item_t* items, int count);

/**
 * Build JSON `{"id":…,"path":…,"type":…}` and `nv_send(w, "fs.changed", payload)`.
 * Used by platform file watchers (NULL-safe on w/path/type).
 */
NV_INTERNAL void nv_fs_changed_emit(nv_window_t* w, long long id, const char* path,
                                    const char* type);

/** Per-window WebView context menu; \p items may be NULL when \p count is 0. */
NV_INTERNAL void nv_mac_window_set_context_menu(nv_window_t* w, const nv_menu_item_t* items,
                                                 int count);
NV_INTERNAL void nv_win_window_set_context_menu(nv_window_t* w, const nv_menu_item_t* items,
                                                int count);
NV_INTERNAL void nv_linux_window_set_context_menu(nv_window_t* w, const nv_menu_item_t* items,
                                                   int count);

/**
 * Parse accelerator string (e.g. "CmdOrCtrl+Shift+K") into \p out.
 * NULL \p combo or \p out → -1; unknown tokens → -1. Returns 0 on success.
 */
NV_INTERNAL int nv_hotkey_parse_combo(const char* combo, nv_hotkey_combo_t* out);

/** Unregister all global hotkeys bound to \p w (NULL-safe). */
NV_INTERNAL void nv_hotkey_detach_for_window(nv_window_t* w);

/** Linux Wayland / unsupported display: maps to `ERR_NOT_SUPPORTED` in ops. */
#define NV_HOTKEY_RC_NOT_SUPPORTED (-100)

/**
 * Global hotkey: \p spec is the parsed combo; \p cb fires on the UI thread with
 * \p handle and \p ctx. Each OS backend provides a strong definition; other
 * targets resolve weak stubs in `nv_hotkey_stubs.c`.
 */
NV_INTERNAL int nv_mac_register_hotkey(nv_window_t* w, long long handle,
                                       const nv_hotkey_combo_t* spec,
                                       void (*cb)(long long handle, void* ctx), void* ctx);
NV_INTERNAL void nv_mac_unregister_hotkey(long long handle);

NV_INTERNAL int nv_win_register_hotkey(nv_window_t* w, long long handle,
                                       const nv_hotkey_combo_t* spec,
                                       void (*cb)(long long handle, void* ctx), void* ctx);
NV_INTERNAL void nv_win_unregister_hotkey(long long handle);

NV_INTERNAL int nv_linux_register_hotkey(nv_window_t* w, long long handle,
                                       const nv_hotkey_combo_t* spec,
                                       void (*cb)(long long handle, void* ctx), void* ctx);
NV_INTERNAL void nv_linux_unregister_hotkey(long long handle);

/**
 * Get window's pool arena.
 * Used for temporary allocations during I/O.
 *
 * @param window Window handle
 * @return       Pool arena (owned by window, valid lifetime of window)
 */
NV_INTERNAL nv_arena_t* nv_window_get_arena(nv_window_t* window);

/* =============================================================================
 * Platform Functions (declared in platform backends, called from core)
 * =============================================================================
 */

NV_INTERNAL void nv_window_platform_create(nv_window_t* window);
NV_INTERNAL void nv_window_platform_destroy(nv_window_t* window);
NV_INTERNAL void nv_window_platform_show(nv_window_t* window);
NV_INTERNAL void nv_window_platform_hide(nv_window_t* window);
NV_INTERNAL void nv_window_platform_set_modal(nv_window_t* window, int enable);
NV_INTERNAL void nv_window_platform_set_title(nv_window_t* window, const char* title);
NV_INTERNAL void nv_window_platform_set_size(nv_window_t* window, int width, int height);
NV_INTERNAL void nv_window_platform_get_size(nv_window_t* window, int* out_w, int* out_h);
NV_INTERNAL void nv_window_platform_set_position(nv_window_t* window, int x, int y);
NV_INTERNAL void nv_window_platform_get_position(nv_window_t* window, int* out_x, int* out_y);
NV_INTERNAL void nv_window_platform_center(nv_window_t* window);
NV_INTERNAL void nv_window_platform_minimize(nv_window_t* window);
NV_INTERNAL void nv_window_platform_maximize(nv_window_t* window);
NV_INTERNAL void nv_window_platform_restore(nv_window_t* window);
NV_INTERNAL void nv_window_platform_fullscreen(nv_window_t* window, int enable);
NV_INTERNAL int  nv_window_platform_is_fullscreen(nv_window_t* window);
NV_INTERNAL void nv_window_platform_focus(nv_window_t* window);
NV_INTERNAL int  nv_window_platform_is_focused(nv_window_t* window);
NV_INTERNAL void nv_window_platform_set_resizable(nv_window_t* window, int enable);
NV_INTERNAL void nv_window_platform_set_always_on_top(nv_window_t* window, int enable);
NV_INTERNAL void nv_window_platform_set_opacity(nv_window_t* window, int opacity_pct);
NV_INTERNAL void nv_window_platform_set_zoom_factor(nv_window_t* window, double factor);
NV_INTERNAL void nv_window_platform_close(nv_window_t* window);

NV_INTERNAL void nv_window_platform_load_html(nv_window_t* window, const char* html, const char* base_url);
NV_INTERNAL void nv_window_platform_load_url(nv_window_t* window, const char* url);
NV_INTERNAL void nv_window_platform_eval_js(nv_window_t* window, const char* js);

#ifdef __cplusplus
}
#endif

#endif /* NV_WINDOW_INTERNAL_H */
