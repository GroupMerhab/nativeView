/*
 * counter_ui — nv-ui counter example.
 *
 * Demonstrates the full nv-ui interaction loop:
 *   1) Build a component tree with nv_form_new / nv_label_new / nv_button_new
 *   2) Bind click handlers with nv_component_bind_event
 *   3) On window ready: nv_form_set_window + nv_ui_flush → HTML in WebView
 *   4) User clicks Increment/Reset → JS "nv.event" → C handler fires
 *   5) Handler: NV_SET_STATE → nv_ui_flush → updated HTML in WebView
 */

#include "nv.h"
#include "nv_ui.h"
#include <stdio.h>
#include <string.h>

/* App context passed through callbacks */
typedef struct {
  nv_form_t*  form;
  nv_label_t* lbl;
  int         count;
} ctx_t;

/* Persistent buffer for the label text — must outlive nv_ui_flush */
static char g_count_buf[32] = "Count: 0";

static void on_increment(nv_component_t* btn, void* userdata) {
  (void)btn;
  ctx_t* ctx = (ctx_t*)userdata;
  ctx->count++;
  snprintf(g_count_buf, sizeof(g_count_buf), "Count: %d", ctx->count);
  NV_SET_STATE(ctx->lbl, state.label.text, g_count_buf);
  nv_ui_flush(ctx->form);
  printf("[counter_ui] increment → %d\n", ctx->count);
  fflush(stdout);
}

static void on_reset(nv_component_t* btn, void* userdata) {
  (void)btn;
  ctx_t* ctx = (ctx_t*)userdata;
  ctx->count = 0;
  snprintf(g_count_buf, sizeof(g_count_buf), "Count: 0");
  NV_SET_STATE(ctx->lbl, state.label.text, g_count_buf);
  nv_ui_flush(ctx->form);
  printf("[counter_ui] reset\n");
  fflush(stdout);
}

/* Called once the WebView has finished loading (if it fires).
 * didFinishNavigation may not fire for about:blank on some WebKit versions,
 * so we load the scaffold immediately before show (see main). This callback
 * just registers the event router for subsequent flushes. */
static void on_ready(nv_window_t* window, void* userdata) {
  ctx_t* ctx = (ctx_t*)userdata;
  nv_form_set_window(ctx->form, window);   /* registers nv_ui_on_event for clicks */
  printf("[counter_ui] window ready\n");
  fflush(stdout);
}

static void on_close(nv_window_t* window, void* userdata) {
  (void)window;
  nv_app_quit((nv_app_t*)userdata);
}

int main(void) {
  /* 1) Create app and window */
  nv_app_t* app = nv_app_create();
  if (!app) { fprintf(stderr, "nv_app_create failed\n"); return 1; }

  nv_window_cfg_t wcfg;
  memset(&wcfg, 0, sizeof(wcfg));
  wcfg.title     = "Counter UI (nv-ui)";
  wcfg.width     = 400;
  wcfg.height    = 220;
  wcfg.resizable = 1;
  wcfg.devtools  = 1;

  nv_window_t* win = nv_window_create(app, &wcfg);
  if (!win) {
    fprintf(stderr, "nv_window_create failed\n");
    nv_app_destroy(app);
    return 1;
  }

  /* 2) Build component tree (before binding to a window) */
  nv_form_cfg_t fcfg = { .title = "Counter", .width = 400, .height = 220 };
  ctx_t ctx = { NULL, NULL, 0 };

  ctx.form = nv_form_new(app, &fcfg);
  if (!ctx.form) { fprintf(stderr, "nv_form_new failed\n"); return 1; }

  ctx.lbl = nv_label_new((nv_component_t*)ctx.form,
               &(nv_label_cfg_t){ .name = "lbl", .text = "Count: 0",
                                  .x = 20, .y = 20, .w = 360, .h = 32 });

  nv_button_t* inc = nv_button_new((nv_component_t*)ctx.form,
               &(nv_button_cfg_t){ .name = "inc", .caption = "Increment", .enabled = 1,
                                   .x = 20, .y = 70, .w = 120, .h = 36 });

  nv_button_t* rst = nv_button_new((nv_component_t*)ctx.form,
               &(nv_button_cfg_t){ .name = "rst", .caption = "Reset", .enabled = 1,
                                   .x = 160, .y = 70, .w = 100, .h = 36 });

  /* 3) Bind click handlers */
  nv_component_bind_event((nv_component_t*)inc, NV_EVENT_CLICK, on_increment, &ctx);
  nv_component_bind_event((nv_component_t*)rst, NV_EVENT_CLICK, on_reset,     &ctx);

  /* 4) Wire window callbacks, load scaffold before show (didFinishNavigation
   *    may not fire for about:blank), then show and run */
  nv_on_ready(win, on_ready, &ctx);
  nv_window_on_close(win, on_close, app);
  nv_form_set_window(ctx.form, win);
  nv_ui_schedule_diff((nv_component_t*)ctx.form);
  nv_ui_flush(ctx.form);
  nv_window_show(win);

  printf("[counter_ui] app run\n");
  fflush(stdout);
  nv_app_run(app);

  /* 5) Cleanup */
  nv_component_destroy((nv_component_t*)ctx.form);
  nv_app_destroy(app);
  return 0;
}
