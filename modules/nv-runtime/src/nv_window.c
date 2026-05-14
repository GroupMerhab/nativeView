/* =============================================================================
 * nv_window.c - Native Window Management
 *
 * Cross-platform window management with platform-specific implementations.
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#include "nv_window_internal.h"
#include "nv_ipc_internal.h"
#include "nv_arena.h"
#include "nv_core_internal.h"
#include "nv_util.h"
#include "nv_window_manager.h"
#include "nv.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static nv_platform_api_t* nv_window_platform_api(nv_window_t* window) {
  if (!window || !window->app) return NULL;
  return &window->app->platform_api;
}


/* =============================================================================
 * Config Validation Helper
 * =============================================================================
 */

static void nv_window_validate_config(nv_window_cfg_t* cfg) {
  if (!cfg) return;
  
  /* Validate and default title */
  if (!cfg->title || strlen(cfg->title) == 0) {
    NV_LOG("Window: NULL or empty title, defaulting to 'App'");
    cfg->title = "App";
  }
  
  /* Validate and default dimensions */
  if (cfg->width <= 0 || cfg->height <= 0) {
    NV_LOG("Window: invalid dimensions (%dx%d), defaulting to 800x600",
           cfg->width, cfg->height);
    cfg->width = 800;
    cfg->height = 600;
  }
  
  /* Clamp min dimensions */
  if (cfg->min_width > cfg->width) {
    NV_LOG("Window: min_width (%d) > width (%d), clamping min_width",
           cfg->min_width, cfg->width);
    cfg->min_width = cfg->width;
  }
  
  if (cfg->min_height > cfg->height) {
    NV_LOG("Window: min_height (%d) > height (%d), clamping min_height",
           cfg->min_height, cfg->height);
    cfg->min_height = cfg->height;
  }
}

/* =============================================================================
 * INTERNAL API - Window Lifecycle
 * =============================================================================
 */

NV_INTERNAL nv_window_t* nv_window_alloc(nv_app_t* app, const nv_window_cfg_t* init_cfg) {
  if (!init_cfg) {
    NV_ERR("Window: NULL config");
    return NULL;
  }
  
  /* Validate config (modifies copy, not original) */
  nv_window_cfg_t cfg = *init_cfg;
  nv_window_validate_config(&cfg);

  /* Create per-window pool arena (4KB chunks, initial 16, growable so nv_send
   * can succeed after many page/prefs/search responses without arena exhaustion) */
  nv_arena_t* pool_arena = nv_arena_create_pool_growable(4096, 16);
  if (!pool_arena) {
    NV_ERR("Window: pool arena creation failed");
    return NULL;
  }
  
  /* Allocate window struct from pool arena */
  nv_window_t* window = nv_arena_alloc(pool_arena, sizeof(nv_window_t));
  if (!window) {
    NV_ERR("Window: allocation failed");
    nv_arena_destroy(pool_arena);
    return NULL;
  }
  
  /* Initialize window struct */
  window->app = app;
  window->cfg = cfg;
  window->arena = pool_arena;
  window->platform = NULL;
  window->visible = 0;
  window->ctx_menu_items = NULL;
  window->ctx_menu_count = 0;
  window->hotkey_slots = NULL;
  window->hotkey_slot_count = 0;

  /* Create IPC state for this window */
  window->ipc = nv_ipc_state_create(pool_arena);
  if (!window->ipc) {
    NV_ERR("Window: IPC state creation failed");
    nv_arena_destroy(pool_arena);
    return NULL;
  }
  
  /* Call platform-specific initialization */
  if (window->app) {
    nv_platform_api_t* api = nv_window_platform_api(window);
    if (api && api->window_create) {
      api->window_create(window);
    } else {
      NV_ERR("Window: missing platform window_create hook");
    }
  }
  
  /* Auto-register window with ID */
  char id[64];
  if (nv_wm_count() == 0) {
    snprintf(id, sizeof(id), "main");
  } else {
    /* Simple sequential ID */
    snprintf(id, sizeof(id), "win-%d", nv_wm_count() + 1);
  }
  nv_wm_register(id, window);
  
  return window;
}

NV_INTERNAL void nv_window_free(nv_window_t* window) {
  if (!window) return;

  nv_hotkey_detach_for_window(window);

  /* Clean up platform resources first */
  if (window->app) {
    nv_platform_api_t* api = nv_window_platform_api(window);
    if (api && api->window_destroy) {
      api->window_destroy(window);
    }
  }
  
  /* IPC state is freed when arena is destroyed */
  /* Pool arena owned by window is freed here */
  if (window->arena) {
    nv_arena_destroy(window->arena);
  }
}

/* =============================================================================
 * INTERNAL API - Platform Handle Access
 * =============================================================================
 */

NV_INTERNAL void* nv_window_get_platform(nv_window_t* window) {
  if (!window) return NULL;
  return window->platform;
}

NV_INTERNAL void nv_window_set_platform(nv_window_t* window, void* platform) {
  if (!window) return;
  window->platform = platform;
}

/* =============================================================================
 * INTERNAL API - Resource Access
 * =============================================================================
 */

NV_INTERNAL nv_ipc_state_t* nv_window_get_ipc(nv_window_t* window) {
  if (!window) return NULL;
  return window->ipc;
}

NV_INTERNAL nv_arena_t* nv_window_get_arena(nv_window_t* window) {
  if (!window) return NULL;
  return window->arena;
}

/* Public API (nv.h) */
NV_API nv_window_t* nv_window_create(nv_app_t* app, const nv_window_cfg_t* config) {
  return nv_window_alloc(app, config);
}

NV_API void nv_window_destroy(nv_window_t* window) {
  if (!window) return;

  nv_app_t* app = window->app;

  /* Invoke any registered close callback before tearing down the window.
     This gives C consumers a chance to react (analogous to JS beforeClose
     event). */
  nv_ipc_state_t* state = nv_window_get_ipc(window);
  nv_ipc_invoke_close_cb(window, state);

  const char* id = nv_wm_get_id_by_window(window);
  if (id) {
    nv_wm_unregister(id);
  }
  /* If this was the last window, quit the app so the run loop exits. */
  if (app && nv_wm_count() == 0) {
    nv_app_quit(app);
  }
  nv_window_free(window);
}

NV_API void nv_window_close(nv_window_t* window) {
  /* Close is now identical to destroy. */
  nv_window_destroy(window);
}

/* =============================================================================
 * Legacy Config Structure (for adapter compatibility with nv_core.c)
 * =============================================================================
 */

/* Minimal definition of legacy nv_window_config_t for adapter function */
typedef struct {
  const char* title;
  int width;
  int height;
  int min_width;
  int min_height;
  const char* initial_url;  /* unused in new API */
  int resizable;
  int dev_tools;             /* maps to devtools */
} nv_window_config_legacy_t;

/* =============================================================================
 * Adapter API - Bridge between legacy nv_core.c and new internal API
 * =============================================================================
 */

/** Adapter: Create window from legacy config (for nv_core.c compatibility) */
nv_window_t* nv_window_create_with_config(nv_arena_t* arena, const void* config) {
  if (!arena || !config) {
    NV_ERR("Window: NULL arena or config");
    return NULL;
  }
  
  /* Cast config to legacy format */
  const nv_window_config_legacy_t* legacy_cfg = (const nv_window_config_legacy_t*)config;
  
  /* Convert to new cfg format */
  nv_window_cfg_t new_cfg = {
    .title = legacy_cfg->title,
    .width = legacy_cfg->width,
    .height = legacy_cfg->height,
    .min_width = legacy_cfg->min_width,
    .min_height = legacy_cfg->min_height,
    .resizable = legacy_cfg->resizable,
    .frameless = 0,
    .transparent = 0,
    .devtools = legacy_cfg->dev_tools
  };
  
  /* Use nv_window_alloc with NULL app for legacy API */
  return nv_window_alloc(NULL, &new_cfg);
}

int nv_window_is_open(nv_window_t* window) {
  if (!window) return 0;
  return window->visible && window->platform != NULL;
}

void nv_window_show(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_show) {
    api->window_show(window);
  }
  window->visible = 1;
}

void nv_window_hide(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_hide) {
    api->window_hide(window);
  }
  window->visible = 0;
}

/* Stub implementations for remaining legacy functions */
int nv_window_load_url(nv_window_t* window, const char* url) {
  (void)window; (void)url;
  return 0;
}

int nv_window_load_html(nv_window_t* window, const char* html) {
  (void)window; (void)html;
  return 0;
}

int nv_window_eval_script(nv_window_t* window, const char* script) {
  (void)window; (void)script;
  return 0;
}

const char* nv_window_get_url(nv_window_t* window) {
  (void)window;
  return NULL;
}

void nv_window_set_title(nv_window_t* window, const char* title) {
  if (!window || !title) return;
  window->cfg.title = title;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_title) {
    api->window_set_title(window, title);
  }
}

const char* nv_window_get_title(nv_window_t* window) {
  if (!window) return NULL;
  return window->cfg.title;
}

int nv_window_resize(nv_window_t* window, int width, int height) {
  if (!window || width <= 0 || height <= 0) return -1;
  window->cfg.width = width;
  window->cfg.height = height;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_size) {
    api->window_set_size(window, width, height);
  }
  return 0;
}

int nv_window_get_size(nv_window_t* window, int* out_width, int* out_height) {
  if (!window || !out_width || !out_height) return -1;
  int w = window->cfg.width, h = window->cfg.height;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_get_size) {
    api->window_get_size(window, &w, &h);
  }
  *out_width = w;
  *out_height = h;
  return 0;
}

int nv_window_set_position(nv_window_t* window, int x, int y) {
  (void)window; (void)x; (void)y;
  if (!window) return -1;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_position) {
    api->window_set_position(window, x, y);
  }
  return 0;
}

int nv_window_get_position(nv_window_t* window, int* out_x, int* out_y) {
  if (!window || !out_x || !out_y) return -1;
  int x = 0, y = 0;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_get_position) {
    api->window_get_position(window, &x, &y);
  }
  *out_x = x;
  *out_y = y;
  return 0;
}

void nv_window_center(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_center) {
    api->window_center(window);
  }
}

/* Public Property API */
NV_API void nv_window_set_size(nv_window_t* window, int width, int height) {
  if (!window || width <= 0 || height <= 0) return;
  window->cfg.width = width;
  window->cfg.height = height;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_size) {
    api->window_set_size(window, width, height);
  }
}

NV_API void nv_window_set_min_size(nv_window_t* window, int width, int height) {
  if (!window) return;
  window->cfg.min_width = width;
  window->cfg.min_height = height;
}

NV_API void nv_window_fullscreen(nv_window_t* window, int enable) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_fullscreen) {
    api->window_fullscreen(window, enable ? 1 : 0);
  }
}

NV_API int nv_window_is_fullscreen(nv_window_t* window) {
  if (!window) return 0;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_is_fullscreen) {
    return api->window_is_fullscreen(window) ? 1 : 0;
  }
  return 0;
}

NV_API void nv_window_minimize(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_minimize) {
    api->window_minimize(window);
  }
}

NV_API void nv_window_maximize(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_maximize) {
    api->window_maximize(window);
  }
}

NV_API void nv_window_restore(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_restore) {
    api->window_restore(window);
  }
}

NV_API void nv_window_focus(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_focus) {
    api->window_focus(window);
  }
}

NV_API int nv_window_is_focused(nv_window_t* window) {
  if (!window) return 0;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_is_focused) {
    return api->window_is_focused(window) ? 1 : 0;
  }
  return 0;
}

NV_API void nv_window_set_resizable(nv_window_t* window, int enable) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_resizable) {
    api->window_set_resizable(window, enable ? 1 : 0);
  }
}

NV_API void nv_window_set_always_on_top(nv_window_t* window, int enable) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_always_on_top) {
    api->window_set_always_on_top(window, enable ? 1 : 0);
  }
}

NV_API void nv_window_set_opacity(nv_window_t* window, int opacity_pct) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_opacity) {
    api->window_set_opacity(window, opacity_pct);
  }
}

NV_API void nv_window_set_zoom_factor(nv_window_t* window, double factor) {
  if (!window) return;
  if (factor <= 0.0) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_set_zoom_factor) {
    api->window_set_zoom_factor(window, factor);
  }
}

NV_API void nv_window_request_close(nv_window_t* window) {
  if (!window) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_close) {
    api->window_close(window);
  }
}

/* Messaging public API */
NV_API void nv_on_message(nv_window_t* window, nv_msg_cb_t callback, void* userdata) {
  if (!window) return;
  nv_ipc_state_t* state = nv_window_get_ipc(window);
  nv_ipc_set_msg_cb(state, callback, userdata);
}

NV_API void nv_on_ready(nv_window_t* window, nv_ready_cb_t callback, void* userdata) {
  if (!window) return;
  nv_ipc_state_t* state = nv_window_get_ipc(window);
  nv_ipc_set_ready_cb(state, callback, userdata);
}

NV_API void nv_window_on_close(nv_window_t* window, nv_close_cb_t callback, void* userdata) {
  if (!window) return;
  nv_ipc_state_t* state = nv_window_get_ipc(window);
  nv_ipc_set_close_cb(state, callback, userdata);
}

NV_API void nv_load_url(nv_window_t* window, const char* url) {
  if (!window || !url) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_load_url) {
    api->window_load_url(window, url);
  }
}

NV_API void nv_load_html(nv_window_t* window, const char* html, const char* base_url) {
  if (!window || !html) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_load_html) {
    api->window_load_html(window, html, base_url);
  }
}

NV_API void nv_load_html_ref(nv_window_t* window, const char* html, size_t length, const char* base_url) {
  if (!window || !html || length == 0) return;
  nv_arena_t* arena = nv_window_get_arena(window);
  char* buf = nv_arena_alloc(arena, length + 1);
  if (!buf) return;
  memcpy(buf, html, length);
  buf[length] = '\0';
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_load_html) {
    api->window_load_html(window, buf, base_url);
  }
}

NV_API void nv_eval_js(nv_window_t* window, const char* js) {
  if (!window || !js) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_eval_js) {
    api->window_eval_js(window, js);
  }
}

NV_API void nv_eval_js_batch(nv_window_t* window, const char** scripts, size_t count) {
  if (!window || !scripts || count == 0) return;
  for (size_t i = 0; i < count; i++) {
    const char* s = scripts[i];
    if (!s) continue;
    nv_platform_api_t* api = nv_window_platform_api(window);
    if (api && api->window_eval_js) {
      api->window_eval_js(window, s);
    }
  }
}

/* MinGW + static .a: weak nv_send is often not pulled from libnv-runtime.a. */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__MINGW32__)
#define NV_SEND_ATTR __attribute__((weak))
#else
#define NV_SEND_ATTR
#endif
#ifdef _MSC_VER
#pragma push_macro("send")
#ifdef send
#undef send
#endif
#endif
NV_API NV_SEND_ATTR void nv_send(nv_window_t* window, const char* event, const char* json) {
  if (!window || !event || !json) return;
  /* Use window arena for the send script so large payloads (e.g. global search)
   * are not allocated from the IPC pool_arena, which is reset each dispatch.
   * Allocating a large script from the pool that was just used for parsing can
   * contribute to use-after-free or segfaults when the script is passed to the
   * platform. The window arena is not reset during dispatch and supports
   * overflow allocations for large scripts. */
  nv_arena_t* arena = nv_window_get_arena(window);
  const char* js = nv_ipc_build_send(arena, event, json);
  if (!js) return;
  nv_platform_api_t* api = nv_window_platform_api(window);
  if (api && api->window_eval_js) {
    api->window_eval_js(window, js);
  }
}
#ifdef _MSC_VER
#pragma pop_macro("send")
#endif

int nv_window_call_function(nv_window_t* window, const char* func, void* args) {
  (void)window; (void)func; (void)args;
  return 0;
}

/* duplicate removed: nv_window_destroy defined above */

/* =============================================================================
 * Platform Function Declarations
 * Actual implementations in nv_mac.m, nv_win.c, nv_linux.c
 * =============================================================================
 */

/* prototypes moved to top */

/* Platform functions are provided by platform backends */
