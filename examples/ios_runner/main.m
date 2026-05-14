#import <Foundation/Foundation.h>

#include <string.h>

#include "nv.h"

static const char* HTML =
  "<!doctype html>"
  "<html><head><meta charset='utf-8'><title>nativeview iOS</title>"
  "<meta name='viewport' content='width=device-width, initial-scale=1' />"
  "<style>"
  "body{font-family:-apple-system,system-ui,sans-serif;margin:0;padding:24px;background:#111;color:#eee}"
  "h1{margin:0 0 12px 0;font-size:22px}"
  "p{margin:0 0 12px 0;line-height:1.4}"
  "button{padding:10px 14px;font-size:16px}"
  "</style>"
  "</head><body>"
  "<h1>nativeview iOS runner</h1>"
  "<p>If you can see this, UIKit + WKWebView + IPC injection are working.</p>"
  "<button onclick=\"window.__nv.send('ping',JSON.stringify({ts:Date.now()}))\">Ping native</button>"
  "<pre id='out'></pre>"
  "<script>"
  "var out=document.getElementById('out');"
  "window.__nv.on('pong',function(d){out.textContent='pong: '+JSON.stringify(d);});"
  "</script>"
  "</body></html>";

static void on_ready(nv_window_t* window, void* userdata) {
  (void)userdata;
  if (!window) return;
  nv_send(window, "pong", "{\"ready\":true}");
}

static void on_message(nv_window_t* window, const char* event, const char* json, void* userdata) {
  (void)userdata;
  if (!window || !event || !json) return;
  if (strcmp(event, "ping") != 0) return;
  nv_send(window, "pong", json);
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  @autoreleasepool {
    nv_app_t* app = nv_app_create();
    if (!app) return 1;

    nv_window_cfg_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.title = "nativeview iOS";
    cfg.width = 390;
    cfg.height = 844;
    cfg.resizable = 0;
    cfg.frameless = 1;
    cfg.transparent = 0;
    cfg.devtools = 1;

    nv_window_t* window = nv_window_create(app, &cfg);
    if (!window) {
      nv_app_destroy(app);
      return 1;
    }

    nv_on_ready(window, on_ready, NULL);
    nv_on_message(window, on_message, NULL);
    nv_load_html(window, HTML, NULL);
    nv_window_show(window);

    nv_app_run(app);
    nv_app_destroy(app);
    return 0;
  }
}
