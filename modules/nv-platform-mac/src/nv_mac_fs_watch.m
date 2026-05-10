/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#import <CoreServices/CoreServices.h>
#import <dispatch/dispatch.h>

#include "nv_window_internal.h"
#include "nv_util.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
#define NV_MAC_FS_ATTR __attribute__((weak))
#else
#define NV_MAC_FS_ATTR
#endif

enum { NV_MAC_FS_WATCH_MAX = 64 };

typedef struct {
  int used;
  long long id;
  nv_window_t *window;
  FSEventStreamRef stream;
  char root[512];
} nv_mac_fs_slot_t;

static nv_mac_fs_slot_t g_nv_mac_fs_slots[NV_MAC_FS_WATCH_MAX];

static void nv_mac_fs_clear_slot(int idx) {
  if (idx < 0 || idx >= NV_MAC_FS_WATCH_MAX) return;
  nv_mac_fs_slot_t *s = &g_nv_mac_fs_slots[idx];
  if (s->stream) {
    FSEventStreamStop(s->stream);
    FSEventStreamInvalidate(s->stream);
    FSEventStreamRelease(s->stream);
    s->stream = NULL;
  }
  memset(s, 0, sizeof(*s));
}

static void nv_mac_fs_event_cb(ConstFSEventStreamRef streamRef, void *clientCallBackInfo, size_t numEvents,
                               void *eventPaths, const FSEventStreamEventFlags eventFlags[],
                               const FSEventStreamEventId eventIds[]) {
  (void)streamRef;
  (void)eventIds;
  int idx = (int)(intptr_t)clientCallBackInfo;
  if (idx < 0 || idx >= NV_MAC_FS_WATCH_MAX || !g_nv_mac_fs_slots[idx].used) return;
  nv_window_t *w = g_nv_mac_fs_slots[idx].window;
  long long wid = g_nv_mac_fs_slots[idx].id;
  if (!w) return;
  char **paths = (char **)eventPaths;
  for (size_t i = 0; i < numEvents; i++) {
    const char *p = (paths && paths[i]) ? paths[i] : g_nv_mac_fs_slots[idx].root;
    unsigned fl = (unsigned)eventFlags[i];
    const char *typ = "modified";
    if (fl & kFSEventStreamEventFlagItemRenamed) {
      typ = "renamed";
    } else if (fl & kFSEventStreamEventFlagItemCreated) {
      typ = "created";
    } else if (fl & kFSEventStreamEventFlagItemRemoved) {
      typ = "deleted";
    }
    nv_fs_changed_emit(w, wid, p, typ);
  }
}

static int nv_mac_fs_find_id(long long id) {
  for (int i = 0; i < NV_MAC_FS_WATCH_MAX; i++) {
    if (g_nv_mac_fs_slots[i].used && g_nv_mac_fs_slots[i].id == id) return i;
  }
  return -1;
}

static int nv_mac_fs_find_free(void) {
  for (int i = 0; i < NV_MAC_FS_WATCH_MAX; i++) {
    if (!g_nv_mac_fs_slots[i].used) return i;
  }
  return -1;
}

NV_INTERNAL NV_MAC_FS_ATTR int nv_mac_fs_watch_start(long long id, const char *path, nv_window_t *w) {
  if (id <= 0 || !path || !path[0] || !w) return -1;
  int existing = nv_mac_fs_find_id(id);
  if (existing >= 0) {
    nv_mac_fs_clear_slot(existing);
  }
  int idx = nv_mac_fs_find_free();
  if (idx < 0) return -1;

  char resolved[512];
  if (realpath(path, resolved)) {
    strncpy(g_nv_mac_fs_slots[idx].root, resolved, sizeof(g_nv_mac_fs_slots[idx].root) - 1);
  } else {
    strncpy(g_nv_mac_fs_slots[idx].root, path, sizeof(g_nv_mac_fs_slots[idx].root) - 1);
  }
  g_nv_mac_fs_slots[idx].root[sizeof(g_nv_mac_fs_slots[idx].root) - 1] = '\0';

  CFStringRef cfs =
      CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, g_nv_mac_fs_slots[idx].root);
  if (!cfs) return -1;
  const void *vals[1] = {cfs};
  CFArrayRef arr = CFArrayCreate(kCFAllocatorDefault, vals, 1, &kCFTypeArrayCallBacks);
  CFRelease(cfs);
  if (!arr) return -1;

  FSEventStreamContext ctx = {0, (void *)(intptr_t)idx, NULL, NULL, NULL};
  FSEventStreamRef stream = FSEventStreamCreate(
      kCFAllocatorDefault, nv_mac_fs_event_cb, &ctx, arr, kFSEventStreamEventIdSinceNow, 0.15,
      (UInt32)(kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer));
  CFRelease(arr);
  if (!stream) return -1;

  FSEventStreamSetDispatchQueue(stream, dispatch_get_main_queue());
  if (!FSEventStreamStart(stream)) {
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
    return -1;
  }

  g_nv_mac_fs_slots[idx].used = 1;
  g_nv_mac_fs_slots[idx].id = id;
  g_nv_mac_fs_slots[idx].window = w;
  g_nv_mac_fs_slots[idx].stream = stream;
  return 0;
}

NV_INTERNAL NV_MAC_FS_ATTR void nv_mac_fs_watch_stop(long long id) {
  int idx = nv_mac_fs_find_id(id);
  if (idx < 0) return;
  nv_mac_fs_clear_slot(idx);
}

NV_INTERNAL NV_MAC_FS_ATTR void nv_mac_fs_watch_detach_for_window(nv_window_t *w) {
  if (!w) return;
  for (int i = 0; i < NV_MAC_FS_WATCH_MAX; i++) {
    if (g_nv_mac_fs_slots[i].used && g_nv_mac_fs_slots[i].window == w) {
      nv_mac_fs_clear_slot(i);
    }
  }
}
