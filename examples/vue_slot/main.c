/*
 * vue_slot — NV_COMP_VUE slot example (Option B: createApp per slot).
 *
 * C creates a form with a label and a Vue slot. After flush, we load a
 * minimal Vue component into the slot via nv_vue_load_component.
 * The Vue component receives reactive props; nv_vue_set_props updates them.
 */

#include "nv.h"
#include "nv_ui.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  nv_form_t* form;
  nv_label_t* lbl;
  nv_vue_t*   slot;
} ctx_t;

/* Minimal Vue component: fn body returns component def.
 * Uses props directly in setup return so nv_vue_set_props updates propagate. */
static const char* VUE_COMP_JS =
  "return {"
    "template: '<div style=\"padding:8px;background:#eee;border-radius:4px;\">"
      "<p>Vue in C slot!</p>"
      "<p>Value: {{ props.value }}</p>"
    "</div>',"
    "props: ['value'],"
    "setup(props){ return { props }; }"
  "};";

static void on_ready(nv_window_t* window, void* userdata) {
  ctx_t* ctx = (ctx_t*)userdata;
  nv_form_set_window(ctx->form, window);
  /* Load Vue component into slot after scaffold is in DOM */
  nv_vue_load_component(ctx->slot, VUE_COMP_JS);
  printf("[vue_slot] Vue component loaded into slot\n");
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
  wcfg.title     = "Vue Slot Example";
  wcfg.width     = 400;
  wcfg.height    = 300;
  wcfg.resizable = 1;
  wcfg.devtools  = 1;

  nv_window_t* win = nv_window_create(app, &wcfg);
  if (!win) {
    fprintf(stderr, "nv_window_create failed\n");
    nv_app_destroy(app);
    return 1;
  }

  nv_form_cfg_t fcfg = { .title = "Vue Slot", .width = 400, .height = 300 };
  ctx_t ctx = { NULL, NULL, NULL };

  ctx.form = nv_form_new(app, &fcfg);
  if (!ctx.form) { fprintf(stderr, "nv_form_new failed\n"); return 1; }

  ctx.lbl = nv_label_new((nv_component_t*)ctx.form,
    &(nv_label_cfg_t){ .name = "lbl", .text = "C label above Vue slot:",
                       .x = 20, .y = 20, .w = 360, .h = 24 });

  ctx.slot = nv_vue_new((nv_component_t*)ctx.form,
    &(nv_vue_cfg_t){ .name = "vue1", .component_name = "demo",
                     .props_json = "{\"value\":42}",
                     .x = 20, .y = 60, .w = 360, .h = 200 });

  nv_on_ready(win, on_ready, &ctx);
  nv_window_on_close(win, on_close, app);
  nv_form_set_window(ctx.form, win);
  nv_ui_schedule_diff((nv_component_t*)ctx.form);
  nv_ui_flush(ctx.form);
  nv_window_show(win);

  nv_app_run(app);

  nv_component_destroy((nv_component_t*)ctx.form);
  nv_app_destroy(app);
  return 0;
}
