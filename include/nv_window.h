/* =============================================================================
 * nv_window.h - Native Window Management
 *
 * Window creation, lifecycle, and native↔web communication.
 * Platform-specific implementations: macOS (WebKit), Windows (WebView2), Linux (GTK/WebKit)
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_WINDOW_H
#define NV_WINDOW_H

#include "nv_arena.h"
#include "nv_ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Types
 * =============================================================================
 */

/* Opaque window handle */
typedef struct nv_window nv_window_t;

/* Window configuration */
typedef struct {
  const char* title;           /* Window title */
  int width;                   /* Initial width in pixels */
  int height;                  /* Initial height in pixels */
  int min_width;               /* Minimum width (0 = no limit) */
  int min_height;              /* Minimum height (0 = no limit) */
  const char* initial_url;     /* Initial URL to load (optional) */
  int resizable;               /* 1 = allow resize, 0 = fixed size */
  int dev_tools;               /* 1 = enable dev tools, 0 = disabled */
} nv_window_config_t;

/* Window state */
typedef enum {
  NV_WINDOW_CREATED,           /* Window created, not yet shown */
  NV_WINDOW_SHOWN,             /* Window visible */
  NV_WINDOW_HIDDEN,            /* Window hidden */
  NV_WINDOW_FOCUSED,           /* Window has keyboard focus */
  NV_WINDOW_MINIMIZED,         /* Window minimized */
  NV_WINDOW_MAXIMIZED,         /* Window maximized */
  NV_WINDOW_CLOSING,           /* Close requested, not yet closed */
  NV_WINDOW_CLOSED             /* Window closed */
} nv_window_state_t;

/* Message handler callback */
typedef int (*nv_window_message_handler_t)(nv_window_t* window, nv_ipc_message_t* msg, void* user_data);

/* =============================================================================
 * Note on Public vs Legacy APIs
 * =============================================================================
 *
 * This header historically exposed a public API that now conflicts with the
 * modern public API declared in nv.h. To prevent duplicate/conflicting
 * declarations when nv.h is included, the legacy/overlapping declarations
 * below are only visible when NV_H is NOT defined.
 *
 * Include nv.h for the authoritative public API.
 */
#ifndef NV_H

/* =============================================================================
 * Window Lifecycle Functions (Legacy - for backward compatibility only)
 * These functions are deprecated and may conflict with nv.h public API
 * Declarations are skipped when compiling together with nv.h
 * =============================================================================
 */

/* (legacy nv_window_create removed to avoid conflict with public nv.h API) */

/** Create a new window with full configuration - legacy function */
nv_window_t* nv_window_create_with_config(nv_arena_t* arena, const nv_window_config_t* config);


/** Check if window is open - legacy function */
int nv_window_is_open(nv_window_t* window);

/*
 NOTE: The following functions are declared in nv_window.h with int return
 types, but nv.h declares them with void return types. To avoid header conflicts,
 these declarations are not included here when nv.h is already included.
 Implementations are provided for backward compatibility with nv_core.c
 Commented out to prevent compilation errors:
 
 - int nv_window_show(nv_window_t* window);  // conflicts with void nv_window_show in nv.h
 - int nv_window_hide(nv_window_t* window);  // conflicts with void nv_window_hide in nv.h
 - int nv_window_load_url(nv_window_t* window, const char* url);
 - int nv_window_load_html(nv_window_t* window, const char* html);
 - int nv_window_eval_script(nv_window_t* window, const char* script);
 - const char* nv_window_get_url(nv_window_t* window);
 - int nv_window_set_title(nv_window_t* window, const char* title);  // conflicts with void nv_window_set_title in nv.h
 - const char* nv_window_get_title(nv_window_t* window);
 - int nv_window_resize(nv_window_t* window, int width, int height);
 - int nv_window_get_size(nv_window_t* window, int* out_width, int* out_height);
 - int nv_window_set_position(nv_window_t* window, int x, int y);
 - int nv_window_get_position(nv_window_t* window, int* out_x, int* out_y);
 - int nv_window_center(nv_window_t* window);  // conflicts with void nv_window_center in nv.h
 - void* nv_window_get_state(nv_window_t* window);
 - int nv_window_set_message_handler(nv_window_t* window, ...);
 - int nv_window_send_message(nv_window_t* window, void* msg);
 - int nv_window_call_function(nv_window_t* window, const char* func, void* args);
 - void nv_window_destroy(nv_window_t* window);
 
Forward declarations (intentionally omitted to avoid conflicts).
*/

/* =============================================================================
 * Web Content
 * =============================================================================
 */

/**
 * Load HTML/CSS/JS content from URL.
 *
 * @param window Window handle
 * @param url URL to load (http://, https://, file://)
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_load_url(nv_window_t* window, const char* url);

/**
 * Load HTML string directly.
 *
 * @param window Window handle
 * @param html HTML content to render
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_load_html(nv_window_t* window, const char* html);

/**
 * Execute JavaScript code.
 *
 * @param window Window handle
 * @param script JavaScript source code
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_eval_script(nv_window_t* window, const char* script);

/**
 * Get current URL.
 *
 * @param window Window handle
 * @return Current URL, NULL if not loaded
 */
NV_API const char* nv_window_get_url(nv_window_t* window);

/* =============================================================================
 * Properties
 * =============================================================================
 */

/* (legacy nv_window_set_title removed to avoid conflict with public nv.h API) */

/**
 * Get window title.
 *
 * @param window Window handle
 * @return Window title
 */
NV_API const char* nv_window_get_title(nv_window_t* window);

/**
 * Resize window.
 *
 * @param window Window handle
 * @param width New width in pixels
 * @param height New height in pixels
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_resize(nv_window_t* window, int width, int height);

/**
 * Get window dimensions.
 *
 * @param window Window handle
 * @param out_width Pointer to receive width
 * @param out_height Pointer to receive height
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_get_size(nv_window_t* window, int* out_width, int* out_height);

/**
 * Set window position.
 *
 * @param window Window handle
 * @param x X coordinate
 * @param y Y coordinate
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_set_position(nv_window_t* window, int x, int y);

/**
 * Get window position.
 *
 * @param window Window handle
 * @param out_x Pointer to receive X coordinate
 * @param out_y Pointer to receive Y coordinate
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_get_position(nv_window_t* window, int* out_x, int* out_y);

/* (legacy nv_window_center removed to avoid conflict with public nv.h API) */

/**
 * Get current window state.
 *
 * @param window Window handle
 * @return Current state
 */
NV_API nv_window_state_t nv_window_get_state(nv_window_t* window);

/* =============================================================================
 * Messaging
 * =============================================================================
 */

/**
 * Register message handler callback.
 *
 * Called when JavaScript sends a message to native code via IPC.
 *
 * @param window Window handle
 * @param handler Callback function
 * @param user_data User context (passed to handler)
 * @return 0 on success, -1 on error
 */
NV_API int nv_window_set_message_handler(nv_window_t* window,
                                         nv_window_message_handler_t handler,
                                         void* user_data);

/**
 * Send message to JavaScript (C→JS).
 *
 * @param window Window handle
 * @param msg IPC message to send
 * @return 0 on success, -1 on error
 * @note Message is automatically serialized and sent to web runtime
 */
NV_API int nv_window_send_message(nv_window_t* window, nv_ipc_message_t* msg);

/**
 * Call JavaScript function from C and get response.
 *
 * @param window Window handle
 * @param func_name Function name (e.g., "myFunc")
 * @param args JSON object with arguments
 * @return Response message from JS, NULL on error or timeout
 */
NV_API nv_ipc_message_t* nv_window_call_function(nv_window_t* window,
                                                 const char* func_name,
                                                 nv_json_t* args);

/* =============================================================================
 * Cleanup
 * =============================================================================
 */

/**
 * Destroy window handle and free resources.
 *
 * @param window Window handle
 */
NV_API void nv_window_destroy(nv_window_t* window);

#endif /* !NV_H */

#ifdef NV_H
/* Internal/legacy declarations for nv-ops and nv-runtime when building with nv.h (e.g. amalgamation) */
NV_API int nv_window_get_size(nv_window_t* window, int* out_width, int* out_height);
NV_API int nv_window_set_position(nv_window_t* window, int x, int y);
NV_API int nv_window_get_position(nv_window_t* window, int* out_x, int* out_y);
NV_API nv_window_t* nv_window_create_with_config(nv_arena_t* arena, const void* config);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NV_WINDOW_H */
