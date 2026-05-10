/*
 * todo_app — Dynamic component list example.
 *
 * Input + Add button to append items; each item has label + Delete button.
 * Demonstrates NV_SET_STATE, nv_component_remove, and full scaffold reload
 * when the component tree structure changes.
 */

#include "nv.h"
#include "nv_ui.h"
#include <stdio.h>
#include <string.h>

#define MAX_ITEMS 64
#define ITEM_TEXT_LEN 128

typedef struct {
  nv_form_t*   form;
  nv_input_t*  input;
  nv_panel_t*  list_panel;
} ctx_t;

static char item_texts[MAX_ITEMS][ITEM_TEXT_LEN];
static int  item_count;

/* Force full scaffold reload (tree structure changed). */
static void reload_scaffold(nv_form_t* form) {
  form->state.form.initialized = 0;
  form->state.form.dirty = 1;
  nv_ui_schedule_diff((nv_component_t*)form);
  nv_ui_flush(form);
}

/* Update y positions of items in list (vertical stack, 36px per item). */
static void update_list_layout(nv_panel_t* list_panel) {
  int i = 0;
  for (nv_component_t* c = list_panel->owned_head; c; c = c->owned_next, i++) {
    c->state.panel.y = i * 36;
    c->state.panel.x = 0;
    c->state.panel.w = 340;
    c->state.panel.h = 32;
  }
}

static void on_delete_wrapper(nv_component_t* btn, void* userdata) {
  nv_panel_t* item = (nv_panel_t*)userdata;
  nv_component_t* list = item->owner;
  nv_form_t* form = nv_component_get_form((nv_component_t*)item);
  if (!list || !form) return;
  nv_component_remove(list, (nv_component_t*)item);
  update_list_layout((nv_panel_t*)list);
  reload_scaffold(form);
  printf("[todo_app] removed item\n");
  fflush(stdout);
  (void)btn;
}

static void on_add(nv_component_t* btn, void* userdata) {
  (void)btn;
  ctx_t* ctx = (ctx_t*)userdata;
  const char* text = ctx->input->state.input.text;
  if (!text || !text[0]) return;
  if (item_count >= MAX_ITEMS) return;

  size_t len = strlen(text);
  if (len >= ITEM_TEXT_LEN) len = ITEM_TEXT_LEN - 1;
  memcpy(item_texts[item_count], text, len);
  item_texts[item_count][len] = '\0';

  char name[32];
  snprintf(name, sizeof(name), "item%02d", item_count);

  nv_panel_t* item = nv_panel_new((nv_component_t*)ctx->list_panel,
    &(nv_panel_cfg_t){ .name = name, .x = 0, .y = item_count * 36, .w = 340, .h = 32 });

  nv_label_new((nv_component_t*)item,
    &(nv_label_cfg_t){ .name = "lbl", .text = item_texts[item_count],
                       .x = 4, .y = 4, .w = 260, .h = 24 });

  nv_button_t* del = nv_button_new((nv_component_t*)item,
    &(nv_button_cfg_t){ .name = "del", .caption = "\u00d7", .enabled = 1,
                        .x = 270, .y = 2, .w = 32, .h = 28 });
  nv_component_bind_event((nv_component_t*)del, NV_EVENT_CLICK, on_delete_wrapper, item);

  item_count++;

  /* Clear input */
  NV_SET_STATE(ctx->input, state.input.text, "");
  reload_scaffold(ctx->form);
  printf("[todo_app] added item %d\n", item_count);
  fflush(stdout);
}

static void on_ready(nv_window_t* window, void* userdata) {
  ctx_t* ctx = (ctx_t*)userdata;
  nv_form_set_window(ctx->form, window);
  printf("[todo_app] window ready\n");
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
  wcfg.title     = "Todo App (nv-ui)";
  wcfg.width     = 400;
  wcfg.height    = 420;
  wcfg.resizable = 1;
  wcfg.devtools  = 1;

  nv_window_t* win = nv_window_create(app, &wcfg);
  if (!win) {
    fprintf(stderr, "nv_window_create failed\n");
    nv_app_destroy(app);
    return 1;
  }

  item_count = 0;
  nv_form_cfg_t fcfg = { .title = "Todo", .width = 400, .height = 420 };
  ctx_t ctx = { NULL, NULL, NULL };

  ctx.form = nv_form_new(app, &fcfg);
  if (!ctx.form) { fprintf(stderr, "nv_form_new failed\n"); return 1; }

  ctx.input = nv_input_new((nv_component_t*)ctx.form,
    &(nv_input_cfg_t){ .name = "newitem", .text = "", .placeholder = "New item...",
                       .x = 20, .y = 20, .w = 260, .h = 32 });

  nv_button_t* add_btn = nv_button_new((nv_component_t*)ctx.form,
    &(nv_button_cfg_t){ .name = "add", .caption = "Add", .enabled = 1,
                        .x = 290, .y = 20, .w = 80, .h = 32 });
  nv_component_bind_event((nv_component_t*)add_btn, NV_EVENT_CLICK, on_add, &ctx);

  ctx.list_panel = nv_panel_new((nv_component_t*)ctx.form,
    &(nv_panel_cfg_t){ .name = "list", .x = 20, .y = 70, .w = 360, .h = 320 });

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
