/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#if defined(__linux__) && !defined(__APPLE__)

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
#define NV_LINUX_FS_ATTR __attribute__((weak))
#else
#define NV_LINUX_FS_ATTR
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <glib.h>

#include "nv_window_internal.h"
#include "nv_util.h"

enum { NV_LINUX_FS_MAX = 64 };

typedef struct {
  int used;
  int wd;
  long long id;
  nv_window_t* window;
  char root[512];
} nv_linux_fs_slot_t;

static int s_inotify_fd = -1;
static guint s_io_source = 0;
static nv_linux_fs_slot_t s_slots[NV_LINUX_FS_MAX];

static int nv_linux_fs_find_wd(int wd) {
  for (int i = 0; i < NV_LINUX_FS_MAX; i++) {
    if (s_slots[i].used && s_slots[i].wd == wd) return i;
  }
  return -1;
}

static int nv_linux_fs_find_id(long long id) {
  for (int i = 0; i < NV_LINUX_FS_MAX; i++) {
    if (s_slots[i].used && s_slots[i].id == id) return i;
  }
  return -1;
}

static int nv_linux_fs_find_free(void) {
  for (int i = 0; i < NV_LINUX_FS_MAX; i++) {
    if (!s_slots[i].used) return i;
  }
  return -1;
}

static void nv_linux_fs_emit_for_event(nv_linux_fs_slot_t* slot, uint32_t mask, const char* name) {
  if (!slot || !slot->window) return;
  const char* typ = "modified";
  if (mask & (IN_MOVED_TO | IN_MOVED_FROM)) {
    typ = "renamed";
  } else if (mask & IN_CREATE) {
    typ = "created";
  } else if (mask & (IN_DELETE | IN_DELETE_SELF)) {
    typ = "deleted";
  }

  char full[768];
  if (name && name[0]) {
    snprintf(full, sizeof(full), "%s/%s", slot->root, name);
  } else {
    strncpy(full, slot->root, sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';
  }
  nv_fs_changed_emit(slot->window, slot->id, full, typ);
}

static gboolean nv_linux_fs_inotify_io(GIOChannel* source, GIOCondition cond, gpointer user_data) {
  (void)user_data;
  (void)cond;
  int fd = g_io_channel_unix_get_fd(source);
  unsigned char buf[4096];
  for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN) break;
      break;
    }
    if (n == 0) break;
    for (unsigned char* p = buf; p < buf + n;) {
      struct inotify_event* ev = (struct inotify_event*)(void*)p;
      p += sizeof(struct inotify_event) + (size_t)ev->len;

      if (ev->mask & IN_Q_OVERFLOW) continue;

      int si = nv_linux_fs_find_wd(ev->wd);
      if (si < 0) continue;

      if (ev->mask & IN_IGNORED) {
        s_slots[si].used = 0;
        s_slots[si].wd = -1;
        continue;
      }

      nv_linux_fs_emit_for_event(&s_slots[si], ev->mask, ev->len ? ev->name : "");
    }
  }
  return TRUE;
}

static int nv_linux_fs_ensure_inotify_source(void) {
  if (s_inotify_fd >= 0) return 0;
  s_inotify_fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
  if (s_inotify_fd < 0) return -1;
  GIOChannel* ch = g_io_channel_unix_new(s_inotify_fd);
  if (!ch) {
    close(s_inotify_fd);
    s_inotify_fd = -1;
    return -1;
  }
  g_io_channel_set_close_on_unref(ch, FALSE);
  s_io_source = g_io_add_watch(ch, G_IO_IN, nv_linux_fs_inotify_io, NULL);
  g_io_channel_unref(ch);
  if (s_io_source == 0) {
    close(s_inotify_fd);
    s_inotify_fd = -1;
    return -1;
  }
  return 0;
}

NV_INTERNAL NV_LINUX_FS_ATTR int nv_linux_fs_watch_start(long long id, const char* path,
                                                         nv_window_t* w) {
  if (id <= 0 || !path || !path[0] || !w) return -1;
  if (nv_linux_fs_ensure_inotify_source() != 0) return -1;

  int ex = nv_linux_fs_find_id(id);
  if (ex >= 0) {
    inotify_rm_watch(s_inotify_fd, s_slots[ex].wd);
    s_slots[ex].used = 0;
    s_slots[ex].wd = -1;
  }

  int idx = nv_linux_fs_find_free();
  if (idx < 0) return -1;

  char resolved[512];
  if (realpath(path, resolved)) {
    strncpy(s_slots[idx].root, resolved, sizeof(s_slots[idx].root) - 1);
  } else {
    strncpy(s_slots[idx].root, path, sizeof(s_slots[idx].root) - 1);
  }
  s_slots[idx].root[sizeof(s_slots[idx].root) - 1] = '\0';

  for (int j = 0; j < NV_LINUX_FS_MAX; j++) {
    if (j != idx && s_slots[j].used && strcmp(s_slots[j].root, s_slots[idx].root) == 0) {
      inotify_rm_watch(s_inotify_fd, s_slots[j].wd);
      s_slots[j].used = 0;
      s_slots[j].wd = -1;
    }
  }

  uint32_t mask = IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_MOVE_SELF;
  int wd = inotify_add_watch(s_inotify_fd, s_slots[idx].root, mask);
  if (wd < 0) {
    return -1;
  }

  s_slots[idx].used = 1;
  s_slots[idx].wd = wd;
  s_slots[idx].id = id;
  s_slots[idx].window = w;
  return 0;
}

NV_INTERNAL NV_LINUX_FS_ATTR void nv_linux_fs_watch_stop(long long id) {
  int idx = nv_linux_fs_find_id(id);
  if (idx < 0) return;
  if (s_inotify_fd >= 0 && s_slots[idx].wd >= 0) {
    inotify_rm_watch(s_inotify_fd, s_slots[idx].wd);
  }
  s_slots[idx].used = 0;
  s_slots[idx].wd = -1;
}

NV_INTERNAL NV_LINUX_FS_ATTR void nv_linux_fs_watch_detach_for_window(nv_window_t* w) {
  if (!w) return;
  for (int i = 0; i < NV_LINUX_FS_MAX; i++) {
    if (s_slots[i].used && s_slots[i].window == w) {
      nv_linux_fs_watch_stop(s_slots[i].id);
    }
  }
}

#endif /* linux */
