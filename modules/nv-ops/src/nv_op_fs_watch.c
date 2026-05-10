/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "nv_op_fs.h"
#include "nv.h"
#include "nv_window_internal.h"
#include "nv_json.h"
#include <stdio.h>
#include <string.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message,
                                  nv_arena_t* arena);

typedef struct {
  long long id;
  char path[512];
} nv_watch_entry_t;

static nv_watch_entry_t g_watches[64];
static size_t g_watch_count = 0;

static int has_traversal(const char* p) { return p && strstr(p, "..") != NULL; }

NV_INTERNAL void nv_fs_changed_emit(nv_window_t* w, long long id, const char* path,
                                    const char* type) {
  if (!w || !path || !type) return;
  nv_arena_t* arena = nv_window_get_arena(w);
  if (!arena) return;
  nv_json_t* obj = nv_json_object(arena);
  if (!obj) return;
  nv_json_int(obj, "id", id);
  nv_json_str(obj, "path", path);
  nv_json_str(obj, "type", type);
  const char* payload = nv_json_serialize(obj);
  if (payload) nv_send(w, "fs.changed", payload);
}

#ifdef __APPLE__
NV_INTERNAL int nv_mac_fs_watch_start(long long id, const char* path, nv_window_t* w);
NV_INTERNAL void nv_mac_fs_watch_stop(long long id);
#elif defined(_WIN32)
#if (defined(__GNUC__) || defined(__clang__))
__attribute__((weak)) int nv_win_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  (void)id;
  (void)path;
  (void)w;
  return -1;
}
__attribute__((weak)) void nv_win_fs_watch_stop(long long id) { (void)id; }
#else
static int nv_win_fs_watch_start_stub(long long id, const char* path, nv_window_t* w) {
  (void)id;
  (void)path;
  (void)w;
  return -1;
}
static void nv_win_fs_watch_stop_stub(long long id) { (void)id; }
#define nv_win_fs_watch_start nv_win_fs_watch_start_stub
#define nv_win_fs_watch_stop nv_win_fs_watch_stop_stub
#endif
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER) && defined(__linux__)
__attribute__((weak)) int nv_linux_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  (void)id;
  (void)path;
  (void)w;
  return -1;
}
__attribute__((weak)) void nv_linux_fs_watch_stop(long long id) { (void)id; }
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
__attribute__((weak)) int nv_mac_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  (void)id;
  (void)path;
  (void)w;
  return -1;
}
__attribute__((weak)) void nv_mac_fs_watch_stop(long long id) { (void)id; }
#else
static int nv_mac_fs_watch_start_stub(long long id, const char* path, nv_window_t* w) {
  (void)id;
  (void)path;
  (void)w;
  return -1;
}
static void nv_mac_fs_watch_stop_stub(long long id) { (void)id; }
#define nv_mac_fs_watch_start nv_mac_fs_watch_start_stub
#define nv_mac_fs_watch_stop nv_mac_fs_watch_stop_stub
#endif

static void watch_remove_at(size_t i) {
  if (i >= g_watch_count) return;
  if (i + 1 < g_watch_count) g_watches[i] = g_watches[g_watch_count - 1];
  g_watch_count--;
}

static void watch_stop_platform_by_id(long long id) {
#ifdef __APPLE__
  nv_mac_fs_watch_stop(id);
#elif defined(_WIN32)
  nv_win_fs_watch_stop(id);
#elif defined(__linux__) && !defined(__APPLE__)
  nv_linux_fs_watch_stop(id);
#else
  nv_mac_fs_watch_stop(id);
#endif
}

NV_INTERNAL void nv_op_fs_watch(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  const char* path = args ? nv_json_get_str(args, "path") : NULL;
  if (id <= 0) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'id'", arena);
    return;
  }
  if (!path) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing 'path'", arena);
    return;
  }
  if (has_traversal(path)) {
    NV_ERR("fs: path traversal attempt: %s", path);
    nv_ipc_reply_err(w, seq, "ERR_PERMISSION", "path traversal blocked", arena);
    return;
  }
  if (g_watch_count >= sizeof(g_watches) / sizeof(g_watches[0])) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "too many watches", arena);
    return;
  }

  for (size_t i = 0; i < g_watch_count; i++) {
    if (g_watches[i].id == id) {
      watch_stop_platform_by_id(id);
      watch_remove_at(i);
      break;
    }
  }

  int rc;
#ifdef __APPLE__
  rc = nv_mac_fs_watch_start(id, path, w);
#elif defined(_WIN32)
  rc = nv_win_fs_watch_start(id, path, w);
#elif defined(__linux__) && !defined(__APPLE__)
  rc = nv_linux_fs_watch_start(id, path, w);
#else
  rc = nv_mac_fs_watch_start(id, path, w);
#endif
  if (rc != 0) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "fs.watch failed", arena);
    return;
  }

  g_watches[g_watch_count].id = id;
  strncpy(g_watches[g_watch_count].path, path, sizeof(g_watches[g_watch_count].path) - 1);
  g_watches[g_watch_count].path[sizeof(g_watches[g_watch_count].path) - 1] = '\0';
  g_watch_count++;
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_fs_unwatch(nv_window_t* w, int seq, const nv_json_val_t* args,
                                  nv_arena_t* arena) {
  long long id = args ? nv_json_get_int(args, "id") : 0;
  (void)w;
  for (size_t i = 0; i < g_watch_count; i++) {
    if (g_watches[i].id == id) {
      watch_stop_platform_by_id(id);
      watch_remove_at(i);
      break;
    }
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}
