/*
 * settings_form — All widget types example.
 *
 * Labels, inputs, checkboxes, select, slider, textarea, and a Save button.
 * Demonstrates NV_SET_STATE and NV_EVENT_* for each widget type.
 */

#include "nv.h"
#include "nv_ui.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  nv_form_t*   form;
  nv_label_t*  summary;
} ctx_t;

static const char* theme_items[] = { "Light", "Dark", "System" };
static int theme_count = 3;

static void on_save(nv_component_t* btn, void* userdata) {
  (void)btn;
  ctx_t* ctx = (ctx_t*)userdata;
  nv_component_t* root = (nv_component_t*)ctx->form;

  const char* name = NULL;
  const char* email = NULL;
  int notifs = 0;
  int theme_idx = 0;
  float volume = 0.5f;
  const char* notes = NULL;

  nv_component_t* n = nv_component_find_by_name(root, "name");
  if (n) name = n->state.input.text;
  n = nv_component_find_by_name(root, "email");
  if (n) email = n->state.input.text;
  n = nv_component_find_by_name(root, "notifs");
  if (n) notifs = n->state.checkbox.checked;
  n = nv_component_find_by_name(root, "theme");
  if (n) theme_idx = n->state.select_.selected_index;
  n = nv_component_find_by_name(root, "volume");
  if (n) volume = n->state.slider.value;
  n = nv_component_find_by_name(root, "notes");
  if (n) notes = n->state.textarea.text;

  const char* theme = theme_idx >= 0 && theme_idx < theme_count
    ? theme_items[theme_idx] : "?";
  if (!name) name = "";
  if (!email) email = "";
  if (!notes) notes = "";

  static char buf[256];
  snprintf(buf, sizeof(buf),
    "Name: %s | Email: %s | Notifs: %s | Theme: %s | Volume: %.0f%% | Notes: %s",
    name, email, notifs ? "on" : "off", theme, volume * 100.0f, notes);
  NV_SET_STATE(ctx->summary, state.label.text, buf);
  nv_ui_flush(ctx->form);
  printf("[settings_form] saved: %s\n", buf);
  fflush(stdout);
}

static void on_ready(nv_window_t* window, void* userdata) {
  ctx_t* ctx = (ctx_t*)userdata;
  nv_form_set_window(ctx->form, window);
  printf("[settings_form] window ready\n");
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
  wcfg.title     = "Settings Form (nv-ui)";
  wcfg.width     = 420;
  wcfg.height    = 420;
  wcfg.resizable = 1;
  wcfg.devtools  = 1;

  nv_window_t* win = nv_window_create(app, &wcfg);
  if (!win) {
    fprintf(stderr, "nv_window_create failed\n");
    nv_app_destroy(app);
    return 1;
  }

  nv_form_cfg_t fcfg = { .title = "Settings", .width = 420, .height = 420 };
  ctx_t ctx = { NULL, NULL };

  ctx.form = nv_form_new(app, &fcfg);
  if (!ctx.form) { fprintf(stderr, "nv_form_new failed\n"); return 1; }

  ctx.summary = nv_label_new((nv_component_t*)ctx.form,
    &(nv_label_cfg_t){ .name = "summary", .text = "Click Save to see values",
                       .x = 20, .y = 20, .w = 380, .h = 24 });

  nv_input_new((nv_component_t*)ctx.form,
    &(nv_input_cfg_t){ .name = "name", .text = "", .placeholder = "Name",
                       .x = 20, .y = 56, .w = 380, .h = 28 });

  nv_input_new((nv_component_t*)ctx.form,
    &(nv_input_cfg_t){ .name = "email", .text = "", .placeholder = "Email",
                       .x = 20, .y = 96, .w = 380, .h = 28 });

  nv_checkbox_new((nv_component_t*)ctx.form,
    &(nv_checkbox_cfg_t){ .name = "notifs", .label = "Enable notifications", .checked = 1,
                          .x = 20, .y = 136, .w = 200, .h = 24 });

  nv_select_new((nv_component_t*)ctx.form,
    &(nv_select_cfg_t){ .name = "theme", .items = theme_items, .item_count = theme_count,
                        .selected_index = 0, .x = 20, .y = 176, .w = 180, .h = 28 });

  nv_slider_new((nv_component_t*)ctx.form,
    &(nv_slider_cfg_t){ .name = "volume", .value = 0.5f, .min = 0.0f, .max = 1.0f, .step = 0.01f,
                        .x = 20, .y = 216, .w = 380, .h = 24 });

  nv_textarea_new((nv_component_t*)ctx.form,
    &(nv_textarea_cfg_t){ .name = "notes", .text = "", .rows = 3,
                          .x = 20, .y = 252, .w = 380, .h = 80 });

  nv_button_t* save = nv_button_new((nv_component_t*)ctx.form,
    &(nv_button_cfg_t){ .name = "save", .caption = "Save", .enabled = 1,
                        .x = 20, .y = 348, .w = 100, .h = 32 });
  nv_component_bind_event((nv_component_t*)save, NV_EVENT_CLICK, on_save, &ctx);

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
