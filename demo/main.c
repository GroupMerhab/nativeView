/* NativeView Mega Demo - Entry Point */
#include "nv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <unistd.h>
#  if defined(__APPLE__)
#    include <mach-o/dyld.h>
#  endif
#endif

#ifndef NV_PATH_MAX
#define NV_PATH_MAX 4096
#endif

static int last_sep_index(const char* s) {
#if defined(_WIN32)
  const char* b = strrchr(s, '\\'); const char* f = strrchr(s, '/');
  const char* p = (b && f) ? (b > f ? b : f) : (b ? b : f);
#else
  const char* p = strrchr(s, '/');
#endif
  return p ? (int)(p - s) : -1;
}

static void dir_of(const char* path, char* out, size_t out_sz) {
  if (!path || !out || out_sz == 0) return;
  int idx = last_sep_index(path);
  if (idx < 0) {
    out[0] = '.'; out[1] = '\0';
  } else {
    size_t n = (size_t)idx;
    if (n >= out_sz) n = out_sz - 1;
    memcpy(out, path, n);
    out[n] = '\0';
  }
}

static void join_path(char* out, size_t out_sz, const char* a, const char* b) {
  if (!out || out_sz == 0) return;
  out[0] = '\0';
  if (!a) a = "";
  if (!b) b = "";
  size_t alen = strlen(a);
  int need_sep =
    (alen > 0) &&
#if defined(_WIN32)
    (a[alen - 1] != '\\' && a[alen - 1] != '/');
#else
    (a[alen - 1] != '/');
#endif
  snprintf(out, out_sz, "%s%s%s", a, need_sep ? "/" : "", b);
}

static void to_file_url(const char* abs_path, char* out, size_t out_sz) {
  /* Convert to file:// URL and normalize slashes */
  size_t oi = 0; out[0] = '\0';
  strncpy(out, "file://", out_sz - 1); out[out_sz - 1] = '\0'; oi = strlen(out);
  for (size_t i = 0; abs_path[i] && oi + 1 < out_sz; i++) {
    char c = abs_path[i];
#if defined(_WIN32)
    if (c == '\\') c = '/';
#endif
    out[oi++] = c;
  }
  out[oi] = '\0';
}

static int resolve_exe_dir(char* out, size_t out_sz, const char* argv0) {
  char path[NV_PATH_MAX] = {0};
#if defined(__APPLE__)
  uint32_t sz = (uint32_t)sizeof(path);
  if (_NSGetExecutablePath(path, &sz) == 0) {
    char real[NV_PATH_MAX];
    if (realpath(path, real)) { dir_of(real, out, out_sz); return 1; }
  }
  if (argv0 && argv0[0]) {
    char real[NV_PATH_MAX];
    if (realpath(argv0, real)) { dir_of(real, out, out_sz); return 1; }
    dir_of(argv0, out, out_sz); return 1;
  }
  return 0;
#elif defined(_WIN32)
  DWORD n = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
  if (n > 0 && n < (DWORD)sizeof(path)) { dir_of(path, out, out_sz); return 1; }
  if (argv0 && argv0[0]) { dir_of(argv0, out, out_sz); return 1; }
  return 0;
#else
  ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (n > 0) { path[n] = '\0'; dir_of(path, out, out_sz); return 1; }
  if (argv0 && argv0[0]) {
    char real[NV_PATH_MAX];
    if (realpath(argv0, real)) { dir_of(real, out, out_sz); return 1; }
    dir_of(argv0, out, out_sz); return 1;
  }
  return 0;
#endif
}

static void on_ready(nv_window_t* w, void* ud) { (void)w; (void)ud; fprintf(stderr, "demo ready\n"); fflush(stderr); }
static void on_close(nv_window_t* w, void* ud) { (void)w; nv_app_quit((nv_app_t*)ud); }

int main(int argc, char** argv) {
  (void)argc;
  nv_app_t* app = nv_app_create();
  if (!app) return 1;

  nv_window_cfg_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.title = "NativeView Mega Demo";
  cfg.width = 1200;
  cfg.height = 800;
  cfg.resizable = 1;
  cfg.devtools = 1;

  nv_window_t* win = nv_window_create(app, &cfg);
  if (!win) { nv_app_destroy(app); return 1; }

  char exe_dir[NV_PATH_MAX];
  if (!resolve_exe_dir(exe_dir, sizeof(exe_dir), (argv && argv[0]) ? argv[0] : NULL)) {
    fprintf(stderr, "Failed to resolve executable directory\n");
  }
  char index_abs[NV_PATH_MAX];
  join_path(index_abs, sizeof(index_abs), exe_dir, "demo/index.html");

  char url[NV_PATH_MAX * 3];
  to_file_url(index_abs, url, sizeof(url));

  nv_on_ready(win, on_ready, NULL);
  nv_window_on_close(win, on_close, app);
  nv_load_url(win, url);
  nv_window_show(win);
  nv_app_run(app);
  nv_app_destroy(app);
  return 0;
}
