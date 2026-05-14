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
#include "nv_arena.h"
#include "nv_menu.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { NV_PLATFORM_RC_NOT_SUPPORTED = -100 };

struct nv_hotkey_combo;
typedef struct nv_hotkey_combo nv_hotkey_combo_t;

typedef struct nv_dialog_ctx {
  nv_window_t* window;
  int seq;
  nv_arena_t* arena;
} nv_dialog_ctx_t;

typedef void (*nv_dialog_cb_t)(nv_dialog_ctx_t* ctx, int canceled, void* result);

typedef struct nv_shell_result {
  int exit_code;
  char* out;
  char* err;
  int truncated;
} nv_shell_result_t;

typedef struct nv_platform_api {
  const char* platform_name;
  int (*shell_open_url)(const char* url);
  int (*shell_open_path)(const char* path);
  int (*shell_show_in_folder)(const char* path);
  nv_shell_result_t* (*shell_exec)(const char* cmd);
  char* (*clipboard_read_text)(void);
  int (*clipboard_write_text)(const char* text);
  void (*clipboard_clear)(void);
  int (*clipboard_has_text)(void);
  char* (*clipboard_read_image)(int* out_w, int* out_h);
  int (*clipboard_write_image)(const char* base64_png);
  int (*clipboard_has_image)(void);
  int (*notification_show)(long long id, const char* title, const char* body,
                           const char* icon_path, nv_window_t* w);
  int (*notification_close)(long long id);
  char* (*screen_get_all)(void);
  char* (*screen_get_primary)(void);
  char* (*screen_get_cursor)(void);
  int (*tray_create)(long long id, const char* icon_path, const char* tooltip, nv_window_t* w);
  int (*tray_destroy)(long long id);
  int (*tray_set_icon)(long long id, const char* icon_path);
  int (*tray_set_tooltip)(long long id, const char* tooltip);
  int (*tray_set_menu)(long long id, const char** labels, const long long* item_ids, int count);
  int (*fs_watch_start)(long long id, const char* path, nv_window_t* w);
  void (*fs_watch_stop)(long long id);
  void (*dialog_open_file_async)(int allow_multiple, nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
  void (*dialog_save_file_async)(nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
  void (*dialog_open_folder_async)(nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
  void (*dialog_message_async)(const char* title, const char* body, const char* type,
                               const char** buttons, size_t btn_count, nv_dialog_ctx_t* ctx,
                               nv_dialog_cb_t cb);
  void (*dialog_confirm_async)(const char* title, const char* body, nv_dialog_ctx_t* ctx,
                               nv_dialog_cb_t cb);
  char* (*app_get_data_dir)(void);
  char* (*app_get_exe_path)(void);
  char* (*app_get_resource_dir)(void);
  char* (*app_get_locale)(void);
  void (*app_set_menu)(nv_window_t* w, const nv_menu_item_t* items, int count);

  void (*window_create)(nv_window_t* window);
  void (*window_destroy)(nv_window_t* window);
  void (*window_show)(nv_window_t* window);
  void (*window_hide)(nv_window_t* window);
  void (*window_set_modal)(nv_window_t* window, int enable);
  void (*window_set_title)(nv_window_t* window, const char* title);
  void (*window_set_size)(nv_window_t* window, int width, int height);
  void (*window_get_size)(nv_window_t* window, int* out_w, int* out_h);
  void (*window_set_position)(nv_window_t* window, int x, int y);
  void (*window_get_position)(nv_window_t* window, int* out_x, int* out_y);
  void (*window_center)(nv_window_t* window);
  void (*window_minimize)(nv_window_t* window);
  void (*window_maximize)(nv_window_t* window);
  void (*window_restore)(nv_window_t* window);
  void (*window_fullscreen)(nv_window_t* window, int enable);
  int  (*window_is_fullscreen)(nv_window_t* window);
  void (*window_focus)(nv_window_t* window);
  int  (*window_is_focused)(nv_window_t* window);
  void (*window_set_resizable)(nv_window_t* window, int enable);
  void (*window_set_always_on_top)(nv_window_t* window, int enable);
  void (*window_set_opacity)(nv_window_t* window, int opacity_pct);
  void (*window_set_zoom_factor)(nv_window_t* window, double factor);
  void (*window_close)(nv_window_t* window);
  void (*window_load_html)(nv_window_t* window, const char* html, const char* base_url);
  void (*window_load_url)(nv_window_t* window, const char* url);
  void (*window_eval_js)(nv_window_t* window, const char* js);
  void (*window_set_context_menu)(nv_window_t* window, const nv_menu_item_t* items, int count);

  int (*hotkey_register)(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                         void (*cb)(long long handle, void* ctx), void* ctx);
  void (*hotkey_unregister)(long long handle);
} nv_platform_api_t;

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
  nv_platform_api_t platform_api;
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
