/* =============================================================================
 * nv_core.c - Core Library Management
 *
 * Library initialization, shutdown, and global state management.
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#include "nv_core.h"
#include "nv_core_internal.h"
#include "nv_arena.h"
#include "nv_window.h"
#include "nv_util.h"
#include "nv.h"
#include "nv_op_registry.h"
#include "nv_window_manager.h"
#include <stdlib.h>
#include <string.h>

#define NV_VERSION "0.1.0"

/* =============================================================================
 * Internal Types
 * =============================================================================
 */

struct nv_context {
  nv_arena_t* arena;
  int running;
  int should_exit;
  nv_window_t** windows;
  int window_count;
  int window_capacity;
};

/* Global context pointer (single instance) */
static nv_context_t* g_context = NULL;

/* =============================================================================
 * Lifecycle
 * =============================================================================
 */

NV_API nv_context_t* nv_init(void) {
  if (g_context != NULL) {
    NV_ERR("Core: already initialized");
    return NULL;
  }
  
  /* Create root arena */
  nv_arena_t* arena = nv_arena_create(65536);
  if (!arena) {
    NV_ERR("Core: arena allocation failed");
    return NULL;
  }
  
  /* Allocate context */
  nv_context_t* ctx = nv_arena_alloc(arena, sizeof(nv_context_t));
  if (!ctx) {
    NV_ERR("Core: context allocation failed");
    nv_arena_destroy(arena);
    return NULL;
  }
  
  ctx->arena = arena;
  ctx->running = 0;
  ctx->should_exit = 0;
  ctx->windows = NULL;
  ctx->window_count = 0;
  ctx->window_capacity = 0;
  
  g_context = ctx;
  /* Initialize IPC operation registry so handlers like app.handshake are registered */
  nv_op_registry_init();
  
  return ctx;
}

NV_API void nv_shutdown(nv_context_t* ctx) {
  if (!ctx) return;
  if (ctx != g_context) {
    return;
  }
  
  /* Close any remaining windows */
  for (int i = 0; i < ctx->window_count; i++) {
    if (ctx->windows[i]) {
      nv_window_destroy(ctx->windows[i]);
    }
  }
  
  /* Destroy arena (frees all allocations) */
  nv_arena_destroy(ctx->arena);
  
  g_context = NULL;
}

/* =============================================================================
 * Window Management
 * =============================================================================
 */

NV_API nv_window_t* nv_create_window(nv_context_t* ctx, const char* title, int width, int height) {
  nv_window_config_t config = {
    .title = title,
    .width = width,
    .height = height,
    .min_width = 0,
    .min_height = 0,
    .initial_url = NULL,
    .resizable = 1,
    .dev_tools = 0
  };
  
  return nv_create_window_with_config(ctx, &config);
}

NV_API nv_window_t* nv_create_window_with_config(nv_context_t* ctx, const nv_window_config_t* config) {
  if (!ctx || !config) {
    NV_ERR("Core: invalid window creation parameters");
    return NULL;
  }
  
  /* Create window */
  nv_window_t* window = nv_window_create_with_config(ctx->arena, config);
  if (!window) {
    NV_ERR("Core: window creation failed");
    return NULL;
  }
  
  /* Add to window list */
  if (ctx->window_count >= ctx->window_capacity) {
    /* Grow window array */
    int new_capacity = ctx->window_capacity == 0 ? 4 : ctx->window_capacity * 2;
    nv_window_t** new_windows = nv_arena_alloc(ctx->arena, new_capacity * sizeof(nv_window_t*));
    if (!new_windows) {
      NV_ERR("Core: window list allocation failed");
      return NULL;
    }
    
    /* Copy existing windows */
    if (ctx->windows && ctx->window_count > 0) {
      memcpy(new_windows, ctx->windows, ctx->window_count * sizeof(nv_window_t*));
    }
    
    ctx->windows = new_windows;
    ctx->window_capacity = new_capacity;
  }
  
  ctx->windows[ctx->window_count++] = window;
  
  return window;
}

/* =============================================================================
 * Event Loop
 * =============================================================================
 */

NV_API int nv_run(nv_context_t* ctx) {
  if (!ctx) {
    NV_ERR("Core: no context");
    return -1;
  }
  
  if (ctx->window_count == 0) {
    return -1;
  }
  
  ctx->running = 1;
  ctx->should_exit = 0;
  
  /* Main event loop - platform-specific implementation would go here
   * For this stub, we'll exit immediately */
  
  ctx->running = 0;
  
  return ctx->should_exit ? 0 : 0;
}

NV_API void nv_exit_loop(nv_context_t* ctx) {
  if (!ctx) return;
  ctx->should_exit = 1;
}

/* =============================================================================
 * Version Information
 * =============================================================================
 */

NV_API const char* nv_get_version(void) {
  return NV_VERSION;
}

NV_API int nv_is_debug(void) {
#ifdef NV_DEBUG
  return 1;
#else
  return 0;
#endif
}

/* =============================================================================
 * Internal nv_app Lifecycle
 * =============================================================================
 */

NV_INTERNAL nv_app_t* nv_app_alloc(void) {
  nv_app_t* app = (nv_app_t*)malloc(sizeof(nv_app_t));
  if (!app) {
    NV_ERR("Core: failed to allocate nv_app");
    return NULL;
  }

  app->platform = NULL;
  app->running = 0;
  return app;
}

NV_INTERNAL void nv_app_free(nv_app_t* app) {
  if (!app) return;
  free(app);
}

NV_INTERNAL void* nv_app_get_platform(nv_app_t* app) {
  if (!app) return NULL;
  return app->platform;
}

NV_INTERNAL void nv_app_set_platform(nv_app_t* app, void* platform) {
  if (!app) return;
  app->platform = platform;
}

/* =============================================================================
 * Platform Hooks (provided by platform backends)
 * =============================================================================
 *
 * Implemented in:
 * - macOS:   src/platform/nv_mac.m
 * - Windows: src/platform/nv_win.c
 * - Linux:   src/platform/nv_linux.c
 */
NV_INTERNAL void nv_app_platform_init(nv_app_t* app);
NV_INTERNAL void nv_app_platform_run(nv_app_t* app);
NV_INTERNAL void nv_app_platform_quit(nv_app_t* app);

/* =============================================================================
 * Public App API (nv.h)
 * =============================================================================
 */

NV_API nv_app_t* nv_app_create(void) {
  nv_app_t* app = nv_app_alloc();
  if (!app) {
    return NULL;
  }

  /* Initialize window manager registry */
  nv_wm_init();

  /* Ensure operation registry is populated for apps that don't call nv_init() */
  nv_op_registry_init();

  nv_app_platform_init(app);
  return app;
}

NV_API void nv_app_run(nv_app_t* app) {
  if (!app) return;
  app->running = 1;
  nv_app_platform_run(app);
  app->running = 0;
}

NV_API void nv_app_quit(nv_app_t* app) {
  if (!app) return;
  if (!app->running) {
    return;
  }
  nv_app_platform_quit(app);
  app->running = 0;
}

NV_API void nv_app_destroy(nv_app_t* app) {
  if (!app) return;

  /* Idempotent with respect to platform quit: only quit if still running. */
  if (app->running) {
    nv_app_platform_quit(app);
    app->running = 0;
  }

  nv_app_free(app);
}
