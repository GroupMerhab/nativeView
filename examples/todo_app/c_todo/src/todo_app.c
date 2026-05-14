/*
 * todo_app.c — Vue UI in embedded HTML + SQLite via todo_bridge IPC.
 */

#include "todo_app.h"
#include "todo_bridge.h"
#include "todo_html.h"
#include "todo_store.h"

#include "nv.h"
#include "nv_arena.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  nv_app_t*      app;
  nv_window_t*   window;
  todo_store_t*  store;
  nv_arena_t*    bridge_arena;
} app_ctx_t;

static void emit_to_js(void* userdata, const char* event, const char* json) {
  nv_window_t* w = (nv_window_t*)userdata;
  if (!w || !event || !json) return;
  nv_send(w, event, json);
}

static void on_message(nv_window_t* window, const char* event, const char* json, void* userdata) {
  app_ctx_t* ctx = (app_ctx_t*)userdata;
  if (!ctx || !ctx->store || !window || !json || !ctx->bridge_arena) return;
  if (!event || strcmp(event, "todo") != 0) return;
  nv_arena_reset(ctx->bridge_arena);
  todo_bridge_handle_wire(ctx->store, ctx->bridge_arena, json, emit_to_js, window);
}

static void on_ready(nv_window_t* window, void* userdata) {
  (void)window;
  (void)userdata;
  printf("[todo_app] webview ready\n");
  fflush(stdout);
}

static void on_close(nv_window_t* window, void* userdata) {
  (void)window;
  nv_app_quit((nv_app_t*)userdata);
}

int todo_app_main(int argc, char** argv) {
  const char* db_path = (argc >= 2 && argv[1] && argv[1][0]) ? argv[1] : "./todo_app.db";

  todo_store_t* store = todo_store_open(db_path);
  if (!store) {
    fprintf(stderr, "[todo_app] todo_store_open failed for %s\n", db_path);
    return 1;
  }

  nv_app_t* app = nv_app_create();
  if (!app) {
    fprintf(stderr, "[todo_app] nv_app_create failed\n");
    todo_store_close(store);
    return 1;
  }

  nv_window_cfg_t wcfg;
  memset(&wcfg, 0, sizeof(wcfg));
  wcfg.title     = "Todo App";
  wcfg.width     = 960;
  wcfg.height    = 720;
  wcfg.resizable = 1;
  wcfg.devtools  = 1;

  nv_window_t* win = nv_window_create(app, &wcfg);
  if (!win) {
    fprintf(stderr, "[todo_app] nv_window_create failed\n");
    todo_store_close(store);
    nv_app_destroy(app);
    return 1;
  }

  nv_arena_t* br = nv_arena_create(256 * 1024);
  if (!br) {
    fprintf(stderr, "[todo_app] bridge arena failed\n");
    nv_window_destroy(win);
    todo_store_close(store);
    nv_app_destroy(app);
    return 1;
  }

  app_ctx_t ctx = { .app = app, .window = win, .store = store, .bridge_arena = br };

  nv_on_ready(win, on_ready, &ctx);
  nv_on_message(win, on_message, &ctx);
  nv_window_on_close(win, on_close, app);
  nv_load_html_ref(win, nv_todo_html(), nv_todo_html_len(), "about:blank");
  nv_window_show(win);

  nv_app_run(app);

  nv_arena_destroy(br);
  todo_store_close(store);
  nv_app_destroy(app);
  return 0;
}
