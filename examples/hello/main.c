/*
 * Cross-platform "Hello NativeView" using ONLY the public nv.h API.
 * Demonstrates:
 *   1) Create nv_app
 *   2) Create an 800x600 resizable window with devtools enabled
 *   3) Load inline HTML (no external files)
 *   4) JS↔C messaging via window.__nv.send / nv_on_message / nv_send
 *   5) on_ready logs "window ready"
 *   6) on_close quits the app event loop
 */

#include <stdio.h>
#include <string.h>
#include "nv.h"

/* Inline HTML UI
 * - Counter display starts at 0
 * - "Increment" and "Reset" buttons send messages to native code
 * - JS listens for "counter_update" and updates the display
 */
static const char* HTML =
  "<!doctype html>"
  "<html><head><meta charset='utf-8'><title>Hello NativeView</title>"
  "<style>"
  "body{font-family:sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh;margin:0;background:#111;color:#eee}"
  "button{margin:0.5rem;padding:0.5rem 1rem;font-size:1rem}"
  "#value{font-size:3rem;margin-bottom:1rem}"
  "</style>"
  "</head><body>"
  "<div id='value'>0</div>"
  "<div>"
  "<button onclick=\"window.__nv.send('counter',JSON.stringify({action:'increment'}))\">Increment</button>"
  "<button onclick=\"window.__nv.send('counter',JSON.stringify({action:'reset'}))\">Reset</button>"
  "</div>"
  "<script>"
  "var display=document.getElementById('value');"
  "window.__nv.on('counter_update',function(d){display.textContent=d.value;});"
  "</script>"
  "</body></html>";

/* Simple counter stored in native code */
static int g_counter = 0;

/* 5) Called once the webview has finished loading */
static void on_ready(nv_window_t* window, void* userdata) {
  (void)window; (void)userdata;
  printf("[hello] window ready\n");
  fflush(stdout);
}

/* 6) Handle JS → C messages and send updates back to JS */
static void on_message(nv_window_t* window, const char* event, const char* json, void* userdata) {
  (void)userdata;
  if (!window || !event || !json) return;
  if (strcmp(event, "counter") != 0) return;

  /* Very small parse-by-substring for the demo */
  if (strstr(json, "increment")) g_counter++;
  else if (strstr(json, "reset")) g_counter = 0;

  /* Send updated counter value back to JS */
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"value\":%d}", g_counter);
  printf("[hello] on_message event=%s json=%s -> send %s\n", event, json, buf);
  fflush(stdout);
  nv_send(window, "counter_update", buf);
}

/* 7) Quit the app when the window is closed */
static void on_close(nv_window_t* window, void* userdata) {
  (void)window;
  nv_app_t* app = (nv_app_t*)userdata;
  printf("[hello] window close -> app quit\n");
  fflush(stdout);
  nv_app_quit(app);
}

int main(void) {
  /* 1) Create application context */
  nv_app_t* app = nv_app_create();
  if (!app) {
    fprintf(stderr, "Failed to create nv_app\n");
    return 1;
  }

  /* 2) Configure window (800x600, resizable=1, devtools=1) */
  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "Hello NativeView";
  cfg.width = 800;
  cfg.height = 600;
  cfg.resizable = 1;
  cfg.frameless = 0;
  cfg.transparent = 0;
  cfg.devtools = 1;

  nv_window_t* window = nv_window_create(app, &cfg);
  if (!window) {
    fprintf(stderr, "Failed to create nv_window\n");
    nv_app_destroy(app);
    return 1;
  }

  /* 3) Wire up callbacks, load inline HTML, and show the window */
  nv_on_ready(window, on_ready, NULL);
  nv_on_message(window, on_message, NULL);
  nv_window_on_close(window, on_close, app);
  printf("[hello] load_html and show\n");
  fflush(stdout);
  nv_load_html(window, HTML, "about:blank");
  nv_window_show(window);

  /* 4) Run the event loop until on_close calls nv_app_quit */
  printf("[hello] app run\n");
  fflush(stdout);
  nv_app_run(app);
  nv_app_destroy(app);
  return 0;
}
