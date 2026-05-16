/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/*
 * Integration: real fs.watch → fs.changed within 500 ms (macOS / Linux).
 */

#include "nv_window_internal.h"
#include "nv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
NV_INTERNAL int nv_mac_fs_watch_start(long long id, const char* path, nv_window_t* w);
NV_INTERNAL void nv_mac_fs_watch_stop(long long id);
#elif defined(__linux__) && !defined(__APPLE__)
#include <gtk/gtk.h>
#include <time.h>
#include <unistd.h>
NV_INTERNAL int nv_linux_fs_watch_start(long long id, const char* path, nv_window_t* w);
NV_INTERNAL void nv_linux_fs_watch_stop(long long id);
#endif

static char* g_last_event = NULL;
static char* g_last_json = NULL;

NV_API void nv_send(nv_window_t* window, const char* event, const char* json) {
  (void)window;
  free(g_last_event);
  free(g_last_json);
  g_last_event = event ? strdup(event) : NULL;
  g_last_json = json ? strdup(json) : NULL;
}

static nv_window_t* make_window(void) {
  nv_window_cfg_t cfg = {"FsWatchInteg", 640, 480, 0, 0, 1, 0, 0, 0, 0};
  return nv_window_alloc(NULL, &cfg);
}

#if defined(__APPLE__) || (defined(__linux__) && !defined(__APPLE__))

static int wait_for_fs_changed_ms(int timeout_ms) {
#if defined(__APPLE__)
  double deadline = CFAbsoluteTimeGetCurrent() + (double)timeout_ms / 1000.0;
  while (CFAbsoluteTimeGetCurrent() < deadline) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, true);
    if (g_last_event && strcmp(g_last_event, "fs.changed") == 0) return 1;
  }
  return 0;
#else
  const gint64 deadline = g_get_monotonic_time() + (gint64)timeout_ms * 1000;
  while (g_get_monotonic_time() < deadline) {
    while (gtk_events_pending()) gtk_main_iteration();
    g_main_context_iteration(NULL, FALSE);
    if (g_last_event && strcmp(g_last_event, "fs.changed") == 0) return 1;
    g_usleep(2000);
  }
  return 0;
#endif
}

static int run(void) {
  char tmpl[] = "/tmp/nv_fs_watch_XXXXXX";
#if defined(__linux__) && !defined(__APPLE__)
  if (!gtk_init_check(0, NULL)) {
    fprintf(stderr, "skip: gtk_init_check failed (no display?)\n");
    return 0;
  }
#endif
  if (mkdtemp(tmpl) == NULL) {
    fprintf(stderr, "mkdtemp failed\n");
    return 1;
  }

  nv_window_t* w = make_window();
  if (!w) return 1;

  const long long wid = 4242;
#if defined(__APPLE__)
  if (nv_mac_fs_watch_start(wid, tmpl, w) != 0) {
    fprintf(stderr, "nv_mac_fs_watch_start failed\n");
    rmdir(tmpl);
    nv_window_free(w);
    return 1;
  }
#elif defined(__linux__) && !defined(__APPLE__)
  if (nv_linux_fs_watch_start(wid, tmpl, w) != 0) {
    fprintf(stderr, "nv_linux_fs_watch_start failed\n");
    rmdir(tmpl);
    nv_window_free(w);
    return 1;
  }
#endif

  char fp[512];
  snprintf(fp, sizeof(fp), "%s/poke.txt", tmpl);
  FILE* f = fopen(fp, "w");
  if (!f) {
#if defined(__APPLE__)
    nv_mac_fs_watch_stop(wid);
#elif defined(__linux__) && !defined(__APPLE__)
    nv_linux_fs_watch_stop(wid);
#endif
    rmdir(tmpl);
    nv_window_free(w);
    return 1;
  }
  fprintf(f, "x");
  fclose(f);

  int ok = wait_for_fs_changed_ms(500);
#if defined(__APPLE__)
  nv_mac_fs_watch_stop(wid);
#elif defined(__linux__) && !defined(__APPLE__)
  nv_linux_fs_watch_stop(wid);
#endif

  unlink(fp);
  rmdir(tmpl);
  nv_window_free(w);

  if (!ok) {
    fprintf(stderr, "timeout: no fs.changed (last event=%s json=%s)\n",
            g_last_event ? g_last_event : "(null)", g_last_json ? g_last_json : "(null)");
    return 1;
  }
  if (!g_last_json || strstr(g_last_json, "\"id\":4242") == NULL) {
    fprintf(stderr, "bad payload: %s\n", g_last_json ? g_last_json : "(null)");
    return 1;
  }
  if (strstr(g_last_json, "\"type\":") == NULL) {
    fprintf(stderr, "missing type: %s\n", g_last_json);
    return 1;
  }
  return 0;
}
#endif

int main(void) {
#if defined(__APPLE__) || (defined(__linux__) && !defined(__APPLE__))
  int rc = run();
  free(g_last_event);
  free(g_last_json);
  return rc;
#else
  (void)g_last_event;
  (void)g_last_json;
  printf("skip: integration only on macOS and Linux\n");
  return 0;
#endif
}
