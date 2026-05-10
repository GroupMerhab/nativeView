#include "nv_op_shell.h"
#include "nv.h"
#include "nv_window_internal.h"
#include <stdlib.h>
#include <string.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message, nv_arena_t* arena);

/* macOS platform hooks (implemented in nv_mac.m) */
NV_INTERNAL int nv_mac_shell_open_url(const char* url);
NV_INTERNAL int nv_mac_shell_open_path(const char* path);
NV_INTERNAL int nv_mac_shell_show_in_folder(const char* path);

/* Windows platform hooks (implemented in nv_win.c) */
NV_INTERNAL int nv_win_shell_open_url(const char* url);
NV_INTERNAL int nv_win_shell_open_path(const char* path);
NV_INTERNAL int nv_win_shell_show_in_folder(const char* path);

/* Linux platform hooks (implemented in nv_linux.c) */
NV_INTERNAL int nv_linux_shell_open_url(const char* url);
NV_INTERNAL int nv_linux_shell_open_path(const char* path);
NV_INTERNAL int nv_linux_shell_show_in_folder(const char* path);

/* GCC/Clang: unit tests may provide strong symbols to observe dispatch. */
#if defined(__GNUC__) || defined(__clang__)
void nv_shell_test_hook_open_url(void) __attribute__((weak));
void nv_shell_test_hook_open_url(void) {}
void nv_shell_test_hook_open_path(void) __attribute__((weak));
void nv_shell_test_hook_open_path(void) {}
void nv_shell_test_hook_show_in_folder(void) __attribute__((weak));
void nv_shell_test_hook_show_in_folder(void) {}
#define NV_SHELL_TEST_HOOK_OPEN_URL() nv_shell_test_hook_open_url()
#define NV_SHELL_TEST_HOOK_OPEN_PATH() nv_shell_test_hook_open_path()
#define NV_SHELL_TEST_HOOK_SHOW_IN_FOLDER() nv_shell_test_hook_show_in_folder()
#else
#define NV_SHELL_TEST_HOOK_OPEN_URL() ((void)0)
#define NV_SHELL_TEST_HOOK_OPEN_PATH() ((void)0)
#define NV_SHELL_TEST_HOOK_SHOW_IN_FOLDER() ((void)0)
#endif

static int has_shell_meta(const char* s) {
  if (!s) return 0;
  const char* metas = ";|&><$`(){}[]*?~#\\\"'";
  for (const char* p = s; *p; ++p) {
    for (const char* m = metas; *m; ++m) {
      if (*p == *m) return 1;
    }
  }
  return 0;
}

NV_INTERNAL void nv_op_shell_open_url(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* url = args ? nv_json_get_str(args, "url") : NULL;
  if (!url) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'url'", arena); return; }
  int rc = 0;
#ifdef __APPLE__
  NV_SHELL_TEST_HOOK_OPEN_URL();
  rc = nv_mac_shell_open_url(url);
#elif defined(_WIN32)
  NV_SHELL_TEST_HOOK_OPEN_URL();
  rc = nv_win_shell_open_url(url);
#else
  NV_SHELL_TEST_HOOK_OPEN_URL();
  rc = nv_linux_shell_open_url(url);
#endif
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "openUrl failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_shell_open_path(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (!path) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'path'", arena); return; }
  int rc = 0;
#ifdef __APPLE__
  NV_SHELL_TEST_HOOK_OPEN_PATH();
  rc = nv_mac_shell_open_path(path);
#elif defined(_WIN32)
  NV_SHELL_TEST_HOOK_OPEN_PATH();
  rc = nv_win_shell_open_path(path);
#else
  NV_SHELL_TEST_HOOK_OPEN_PATH();
  rc = nv_linux_shell_open_path(path);
#endif
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "openPath failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_shell_show_in_folder(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (!path) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'path'", arena); return; }
  int rc = 0;
#ifdef __APPLE__
  NV_SHELL_TEST_HOOK_SHOW_IN_FOLDER();
  rc = nv_mac_shell_show_in_folder(path);
#elif defined(_WIN32)
  NV_SHELL_TEST_HOOK_SHOW_IN_FOLDER();
  rc = nv_win_shell_show_in_folder(path);
#else
  NV_SHELL_TEST_HOOK_SHOW_IN_FOLDER();
  rc = nv_linux_shell_show_in_folder(path);
#endif
  if (rc != 0) { nv_ipc_reply_err(w, seq, "ERR_IO", "showInFolder failed", arena); return; }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

static void nv_shell_result_free_heap(nv_shell_result_t* r) {
  if (!r) return;
  free(r->out);
  free(r->err);
  free(r);
}

NV_INTERNAL void nv_op_shell_exec(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  const char* cmd = args ? nv_json_get_str(args, "cmd") : NULL;
  if (!cmd) { nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'cmd'", arena); return; }
#ifndef NV_ALLOW_SHELL_EXEC
  if (has_shell_meta(cmd)) {
    NV_ERR("shell.exec rejected for metacharacters: %s", cmd);
    nv_ipc_reply_err(w, seq, "ERR_PERMISSION", "shell metacharacters not allowed", arena);
    return;
  }
#endif
  {
    nv_shell_result_t* pr = NULL;
    char* arena_stdout = NULL;
    char* arena_stderr = NULL;
    nv_json_t* result = NULL;

#ifdef __APPLE__
    pr = nv_mac_shell_exec(cmd);
#elif defined(_WIN32)
    pr = nv_win_shell_exec(cmd);
#else
    pr = nv_linux_shell_exec(cmd);
#endif
    if (!pr) {
      nv_ipc_reply_err(w, seq, "ERR_IO", "shell.exec failed", arena);
      return;
    }

    arena_stdout = nv_arena_str_dup(arena, pr->out);
    arena_stderr = nv_arena_str_dup(arena, pr->err);
    if (!arena || !arena_stdout || !arena_stderr) {
      nv_shell_result_free_heap(pr);
      nv_ipc_reply_err(w, seq, "ERR_IO", "shell.exec reply alloc failed", arena);
      return;
    }

    result = nv_json_object(arena);
    if (!result) {
      nv_shell_result_free_heap(pr);
      nv_ipc_reply_err(w, seq, "ERR_IO", "shell.exec reply alloc failed", arena);
      return;
    }

    nv_json_int(result, "exitCode", (long long)pr->exit_code);
    nv_json_str(result, "stdout", arena_stdout);
    nv_json_str(result, "stderr", arena_stderr);
    if (pr->truncated) nv_json_bool(result, "truncated", 1);

    nv_shell_result_free_heap(pr);
    nv_ipc_reply_ok(w, seq, result, arena);
  }
}
