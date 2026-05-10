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
typedef struct nv_hotkey_combo {
  uint32_t mod_flags; /**< NV_HOTKEY_MOD_* bitmask */
  int is_fn;          /**< 1 = function key, 0 = letter/digit/named key */
  int fn_index;       /**< 1–24 when is_fn */
  char key_char;      /**< lowercase letter a–z, digit 0–9, or 0 when is_fn / named */
  int special;        /**< NV_HOTKEY_SPECIAL_* when !is_fn && key_char==0 */
} nv_hotkey_combo_t;

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

/* =============================================================================
 * Shell exec (platform): run `cmd` via system shell, capture stdout/stderr.
 * Returns malloc'd struct with malloc'd out/err; caller frees all.
 * =============================================================================
 */

typedef struct nv_shell_result {
  int exit_code;
  char* out;
  char* err;
  int truncated; /* 1 if either stream was capped at 1 MiB */
} nv_shell_result_t;

NV_INTERNAL nv_shell_result_t* nv_mac_shell_exec(const char* cmd);
NV_INTERNAL nv_shell_result_t* nv_win_shell_exec(const char* cmd);
NV_INTERNAL nv_shell_result_t* nv_linux_shell_exec(const char* cmd);

/** Application menu: nv_mac_menu.m, nv_linux.c, or no-op stubs in nv_win.c */
NV_INTERNAL void nv_mac_app_set_menu(const nv_menu_item_t* items, int count);
NV_INTERNAL void nv_linux_app_set_menu(nv_window_t* w, const nv_menu_item_t* items, int count);

/* macOS menu bar extras (NSStatusItem); implemented in nv_mac.m */
#ifdef __APPLE__
NV_INTERNAL int nv_mac_tray_create(long long id, const char* icon_path,
                                   const char* tooltip, nv_window_t* w);
NV_INTERNAL int nv_mac_tray_destroy(long long id);
NV_INTERNAL int nv_mac_tray_set_icon(long long id, const char* icon_path);
NV_INTERNAL int nv_mac_tray_set_tooltip(long long id, const char* tooltip);
NV_INTERNAL int nv_mac_tray_set_menu(long long id, const char** labels,
                                     const long long* item_ids, int count);

/** Local notifications (UNUserNotificationCenter / NSUserNotification); implemented in nv_mac.m */
NV_INTERNAL int nv_mac_notification_show(long long id, const char* title, const char* body,
                                         const char* icon_path, nv_window_t* w);
NV_INTERNAL int nv_mac_notification_close(long long id);

/** FSEvents-backed `fs.watch`; emits `fs.changed` via `nv_fs_changed_emit`. */
NV_INTERNAL int nv_mac_fs_watch_start(long long id, const char* path, nv_window_t* w);
NV_INTERNAL void nv_mac_fs_watch_stop(long long id);
NV_INTERNAL void nv_mac_fs_watch_detach_for_window(nv_window_t* w);
#endif

#ifdef _WIN32
NV_INTERNAL int nv_win_tray_create(long long id, const char* icon_path, const char* tooltip,
                                   nv_window_t* w);
NV_INTERNAL int nv_win_tray_destroy(long long id);
NV_INTERNAL int nv_win_tray_set_icon(long long id, const char* icon_path);
NV_INTERNAL int nv_win_tray_set_tooltip(long long id, const char* tooltip);
NV_INTERNAL int nv_win_tray_set_menu(long long id, const char** labels, const long long* item_ids,
                                     int count);
/** WinRT toast notifications; implemented in nv_win_notification.c. Returns 0, -1 (blocked/disabled), or -2 (I/O). */
NV_INTERNAL int nv_win_notification_show(long long id, const char* title, const char* body,
                                         const char* icon_path, nv_window_t* w);
NV_INTERNAL int nv_win_notification_close(long long id);

/** ReadDirectoryChangesW-backed `fs.watch`. */
NV_INTERNAL int nv_win_fs_watch_start(long long id, const char* path, nv_window_t* w);
NV_INTERNAL void nv_win_fs_watch_stop(long long id);
NV_INTERNAL void nv_win_fs_watch_detach_for_window(nv_window_t* w);
#endif

#if defined(__linux__) && !defined(__APPLE__)
/** Tray hook return value mapped to IPC `ERR_NOT_SUPPORTED` in nv_op_tray.c */
#define NV_TRAY_RC_NOT_SUPPORTED (-100)
NV_INTERNAL int nv_linux_tray_create(long long id, const char* icon_path, const char* tooltip,
                                     nv_window_t* w);
NV_INTERNAL int nv_linux_tray_destroy(long long id);
NV_INTERNAL int nv_linux_tray_set_icon(long long id, const char* icon_path);
NV_INTERNAL int nv_linux_tray_set_tooltip(long long id, const char* tooltip);
NV_INTERNAL int nv_linux_tray_set_menu(long long id, const char** labels, const long long* item_ids,
                                       int count);
/** libnotify; maps to `ERR_NOT_SUPPORTED` in nv_op_notification.c when built without libnotify. */
#define NV_LINUX_NOTIFICATION_RC_NOT_SUPPORTED NV_TRAY_RC_NOT_SUPPORTED
NV_INTERNAL int nv_linux_notification_show(long long id, const char* title, const char* body,
                                           const char* icon_path, nv_window_t* w);
NV_INTERNAL int nv_linux_notification_close(long long id);

/** inotify-backed `fs.watch`. */
NV_INTERNAL int nv_linux_fs_watch_start(long long id, const char* path, nv_window_t* w);
NV_INTERNAL void nv_linux_fs_watch_stop(long long id);
NV_INTERNAL void nv_linux_fs_watch_detach_for_window(nv_window_t* w);
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* NV_WINDOW_INTERNAL_H */

