/*
 * live_data — Slider + live display.
 *
 * Demonstrates continuous value updates: slider NV_EVENT_INPUT fires on
 * every change; we update the label and flush. For 60fps live feeds,
 * Phase 4 shm path (nv_slider_attach_shm + shm_read_f32 in rAF) would
 * replace IPC with SharedArrayBuffer for sub-millisecond reads.
 */

#include "nv.h"
#include "nv_ui.h"
#include <stdio.h>
#include <string.h>

static char g_value_buf[32] = "0.50";

static void on_slider_input(nv_component_t* comp, void* userdata) {
  (void)userdata;
  float v = comp->state.slider.value;
  snprintf(g_value_buf, sizeof(g_value_buf), "%.2f", v);
  nv_form_t* form = nv_component_get_form(comp);
  if (form) {
    nv_component_t* lbl = nv_component_find_by_name((nv_component_t*)form, "value");
    if (lbl) {
      NV_SET_STATE(lbl, state.label.text, g_value_buf);
      nv_ui_flush(form);
    }
  }
}

static void on_ready(nv_window_t* window, void* userdata) {
  nv_form_t* form = (nv_form_t*)userdata;
  nv_form_set_window(form, window);
  printf("[live_data] window ready\n");
  fflush(stdout);
}

static void on_close(nv_window_t* window, void* userdata) {
  (void)window;
  nv_app_quit((nv_app_t*)userdata);
}

int main(void) {
  nv_app_t* app = nv_app_create();
  if (!app) { fprintf(stderr, "nv_app_create failed\n"); return 1; }

  nv_window_cfg_t wcfg;
  memset(&wcfg, 0, sizeof(wcfg));
  wcfg.title     = "Live Data (nv-ui)";
  wcfg.width     = 360;
  wcfg.height    = 120;
  wcfg.resizable = 1;
  wcfg.devtools  = 1;

  nv_window_t* win = nv_window_create(app, &wcfg);
  if (!win) {
    fprintf(stderr, "nv_window_create failed\n");
    nv_app_destroy(app);
    return 1;
  }

  nv_form_cfg_t fcfg = { .title = "Live", .width = 360, .height = 120 };
  nv_form_t* form = nv_form_new(app, &fcfg);
  if (!form) { fprintf(stderr, "nv_form_new failed\n"); nv_app_destroy(app); return 1; }

  nv_label_new((nv_component_t*)form,
    &(nv_label_cfg_t){ .name = "value", .text = "0.50", .x = 20, .y = 20, .w = 320, .h = 24 });

  nv_slider_t* sld = nv_slider_new((nv_component_t*)form,
    &(nv_slider_cfg_t){ .name = "slider", .value = 0.5f, .min = 0.0f, .max = 1.0f, .step = 0.01f,
                        .x = 20, .y = 56, .w = 320, .h = 24 });
  nv_component_bind_event((nv_component_t*)sld, NV_EVENT_INPUT, on_slider_input, NULL);

  nv_on_ready(win, on_ready, form);
  nv_window_on_close(win, on_close, app);
  nv_form_set_window(form, win);
  nv_ui_schedule_diff((nv_component_t*)form);
  nv_ui_flush(form);
  nv_window_show(win);

  nv_app_run(app);
  nv_component_destroy((nv_component_t*)form);
  nv_app_destroy(app);
  return 0;
}
