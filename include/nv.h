/* =============================================================================
 * nv.h - nativeview Public API
 *
 * ONLY header required by users. Provides complete native window management
 * with bidirectional C↔JavaScript communication.
 *
 * ABI-stable: opaque handles, version-checked at runtime.
 * Memory: all allocations owned by nativeview, automatic cleanup.
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_H
#define NV_H

#include "nv_util.h"
#include "nv_hotkey.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Opaque Handle Types (ABI-stable forward declarations)
 * =============================================================================
 */

/** Opaque application context. Created once, runs event loop. */
typedef struct nv_app nv_app_t;

/** Opaque window handle. Owns webview and message handlers. */
typedef struct nv_window nv_window_t;

/* =============================================================================
 * Callback Types
 * =============================================================================
 */

/**
 * Message callback - invoked when JavaScript posts a message.
 * @param window   Window that received the message
 * @param event    Event name (caller-defined string)
 * @param json     Payload as JSON string (may be "null")
 * @param userdata Context pointer passed to nv_on_message()
 * NOTE: json pointer valid only within callback. Copy if needed.
 */
typedef void (*nv_msg_cb_t)(nv_window_t* window, const char* event,
                             const char* json, void* userdata);

/** Ready callback - invoked when window webview is fully loaded.
 * @param window   Window that is ready
 * @param userdata Context pointer passed to nv_on_ready() */
typedef void (*nv_ready_cb_t)(nv_window_t* window, void* userdata);

/** Close callback - invoked when user closes window.
 * @param window   Window being closed
 * @param userdata Context pointer passed to nv_window_on_close() */
typedef void (*nv_close_cb_t)(nv_window_t* window, void* userdata);

/* =============================================================================
 * Configuration Struct (input-only, never returned by API)
 * =============================================================================
 */

/**
 * Window configuration for creation. All fields required.
 * Set to 0/NULL for defaults.
 */
typedef struct {
  const char* title;      /**< Window title. If NULL, defaults to "nativeview". */
  int width;              /**< Initial width in pixels. Min 100. */
  int height;             /**< Initial height in pixels. Min 100. */
  int min_width;          /**< Minimum width (0 = no limit). */
  int min_height;         /**< Minimum height (0 = no limit). */
  int resizable;          /**< 1 = allow resize, 0 = fixed. */
  int frameless;          /**< 1 = no titlebar/border, 0 = normal. */
  int transparent;        /**< 1 = transparent background, 0 = opaque. */
  int devtools;           /**< 1 = show dev tools, 0 = hidden. */
  int modal;              /**< 1 = modal window, 0 = modeless window. */
} nv_window_cfg_t;

/* =============================================================================
 * Application Lifecycle
 * =============================================================================
 */

/** Create application context.
 * @return New application instance, or NULL on error. */
NV_API nv_app_t* nv_app_create(void);

/** Run the application event loop (blocking until nv_app_quit() is called).
 * @param app Application instance (NULL-safe). Processes all window events. */
NV_API void nv_app_run(nv_app_t* app);

/** Signal application to exit event loop.
 * @param app Application instance (NULL-safe). */
NV_API void nv_app_quit(nv_app_t* app);

/** Destroy application and all windows.
 * @param app Application instance (NULL-safe). */
NV_API void nv_app_destroy(nv_app_t* app);

/* =============================================================================
 * Window Lifecycle
 * =============================================================================
 */

/** Create a new window (not shown until nv_window_show() is called).
 * @param app Application context (required)
 * @param config Window configuration (required)
 * @return New window handle, or NULL on error. */
NV_API nv_window_t* nv_window_create(nv_app_t* app,
                                      const nv_window_cfg_t* config);

/** Close a window (destroy by default). Equivalent to nv_window_destroy().
 * @param window Window handle (NULL-safe). */
NV_API void nv_window_close(nv_window_t* window);

/** Destroy window and free resources.
 * @param window Window handle (NULL-safe). */
NV_API void nv_window_destroy(nv_window_t* window);

/** Make window visible.
 * @param window Window handle (NULL-safe). */
NV_API void nv_window_show(nv_window_t* window);

/** Hide window (keep in memory).
 * @param window Window handle (NULL-safe). */
NV_API void nv_window_hide(nv_window_t* window);

/** Update window title.
 * @param window Window handle (NULL-safe)
 * @param title New title (copied internally) */
NV_API void nv_window_set_title(nv_window_t* window, const char* title);

/** Resize window.
 * @param window Window handle (NULL-safe)
 * @param width New width (pixels)
 * @param height New height (pixels) */
NV_API void nv_window_set_size(nv_window_t* window, int width, int height);

/** Set minimum window size.
 * @param window Window handle (NULL-safe)
 * @param width Minimum width (pixels, 0 = no limit)
 * @param height Minimum height (pixels, 0 = no limit) */
NV_API void nv_window_set_min_size(nv_window_t* window, int width,
                                    int height);

/** Center window on screen.
 * @param window Window handle (NULL-safe). */
NV_API void nv_window_center(nv_window_t* window);

/** Toggle fullscreen mode.
 * @param window Window handle (NULL-safe)
 * @param enable 1 = fullscreen, 0 = windowed */
NV_API void nv_window_fullscreen(nv_window_t* window, int enable);
/** Query fullscreen state (1 = fullscreen, 0 = windowed). */
NV_API int nv_window_is_fullscreen(nv_window_t* window);

/** Minimize window to dock. */
NV_API void nv_window_minimize(nv_window_t* window);
/** Maximize/zoom window. */
NV_API void nv_window_maximize(nv_window_t* window);
/** Restore from minimized/maximized. */
NV_API void nv_window_restore(nv_window_t* window);
/** Bring window to front and focus. */
NV_API void nv_window_focus(nv_window_t* window);
/** Query focus state (1 = focused, 0 = not focused). */
NV_API int nv_window_is_focused(nv_window_t* window);
/** Enable or disable window resizing. */
NV_API void nv_window_set_resizable(nv_window_t* window, int enable);
/** Keep window always on top (floating). */
NV_API void nv_window_set_always_on_top(nv_window_t* window, int enable);
/** Set window opacity as percentage (0-100). */
NV_API void nv_window_set_opacity(nv_window_t* window, int opacity_pct);
NV_API void nv_window_set_zoom_factor(nv_window_t* window, double factor);
/** Request window close (non-conflicting with legacy). */
NV_API void nv_window_request_close(nv_window_t* window);

/** Register close callback (invoked when user closes window).
 * @param window Window handle (NULL-safe)
 * @param callback Invoked on close (may be NULL)
 * @param userdata Custom context pointer */
NV_API void nv_window_on_close(nv_window_t* window, nv_close_cb_t callback,
                                void* userdata);

/* =============================================================================
 * Web Content
 * =============================================================================
 */

/** Load URL in webview.
 * @param window Window handle (NULL-safe)
 * @param url URL to load (http://, https://, file://) */
NV_API void nv_load_url(nv_window_t* window, const char* url);

/** Load HTML string in webview.
 * @param window Window handle (NULL-safe)
 * @param html HTML content (null-terminated)
 * @param base_url Base URL for relative links (may be NULL) */
NV_API void nv_load_html(nv_window_t* window, const char* html,
                          const char* base_url);

/** Load HTML with explicit length (for binary/unterminated data).
 * @param window Window handle (NULL-safe)
 * @param html HTML content (NOT null-terminated)
 * @param length Number of bytes
 * @param base_url Base URL for relative links (may be NULL) */
NV_API void nv_load_html_ref(nv_window_t* window, const char* html,
                              size_t length, const char* base_url);

/** Execute JavaScript in webview (asynchronously).
 * @param window Window handle (NULL-safe)
 * @param js JavaScript code (null-terminated) */
NV_API void nv_eval_js(nv_window_t* window, const char* js);

/** Execute multiple JavaScript snippets in batch.
 * @param window Window handle (NULL-safe)
 * @param scripts Array of JS code strings
 * @param count Number of scripts */
NV_API void nv_eval_js_batch(nv_window_t* window, const char** scripts,
                              size_t count);

/** Non-zero if the calling thread is the process primordial (first) thread.
 *  On macOS, AppKit expects UI setup on this thread; JNI hosts typically need
 *  java -XstartOnFirstThread (see docs/Java.md). Other platforms return 1. */
NV_API int nv_is_process_main_thread(void);

/* =============================================================================
 * Messaging (C↔JavaScript Communication)
 * =============================================================================
 */

/** Register callback for JavaScript messages.
 * @param window Window handle (NULL-safe)
 * @param callback Invoked when JS calls nativeview.send(event, json)
 * @param userdata Custom context pointer */
NV_API void nv_on_message(nv_window_t* window, nv_msg_cb_t callback,
                           void* userdata);

/** Register callback for window ready notification (document complete).
 * @param window Window handle (NULL-safe)
 * @param callback Invoked when webview is fully loaded
 * @param userdata Custom context pointer */
NV_API void nv_on_ready(nv_window_t* window, nv_ready_cb_t callback,
                         void* userdata);

/** Send message from C to JavaScript.
 * @param window Window handle (NULL-safe)
 * @param event Event name
 * @param json JSON payload (caller must ensure valid JSON) */
NV_API void nv_send(nv_window_t* window, const char* event,
                     const char* json);

#ifdef __cplusplus
}
#endif

#endif /* NV_H */
