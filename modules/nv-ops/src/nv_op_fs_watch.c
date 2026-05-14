/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "nv_op_fs.h"
#include "nv.h"
#include "nv_core_internal.h"
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

static void watch_remove_at(size_t i) {
  if (i >= g_watch_count) return;
  if (i + 1 < g_watch_count) g_watches[i] = g_watches[g_watch_count - 1];
  g_watch_count--;
}

NV_INTERNAL void nv_op_fs_watch(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena) {
  if (!w || !w->app) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "no window context", arena);
    return;
  }
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
      const nv_platform_api_t* api = &w->app->platform_api;
      if (api->fs_watch_stop) {
        api->fs_watch_stop(id);
      }
      watch_remove_at(i);
      break;
    }
  }

  const nv_platform_api_t* api = &w->app->platform_api;
  if (!api->fs_watch_start) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "fs.watch not supported", arena);
    return;
  }
  int rc = api->fs_watch_start(id, path, w);
  if (rc != 0) {
    if (rc == NV_PLATFORM_RC_NOT_SUPPORTED) {
      nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "fs.watch not supported", arena);
    } else {
      nv_ipc_reply_err(w, seq, "ERR_IO", "fs.watch failed", arena);
    }
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
  const nv_platform_api_t* api = (w && w->app) ? &w->app->platform_api : NULL;
  for (size_t i = 0; i < g_watch_count; i++) {
    if (g_watches[i].id == id) {
      if (api && api->fs_watch_stop) {
        api->fs_watch_stop(id);
      }
      watch_remove_at(i);
      break;
    }
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}
