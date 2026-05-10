/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifdef _WIN32

#include "nv_win_fs_watch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nv_window_internal.h"
#include "nv_win_tray.h"

enum { NV_WIN_FS_MAX = 64 };

typedef struct nv_win_fs_change_msg {
  nv_window_t* window;
  long long watch_id;
  char path[2048];
  char type[16];
} nv_win_fs_change_msg_t;

typedef struct nv_win_fs_slot {
  int used;
  long long id;
  nv_window_t* window;
  HWND hwnd;
  HANDLE thread;
  HANDLE stop_evt;
  HANDLE h_dir;
  char root_utf8[1024];
} nv_win_fs_slot_t;

static nv_win_fs_slot_t g_nv_win_fs_slots[NV_WIN_FS_MAX];
static CRITICAL_SECTION g_nv_win_fs_lock;
static LONG g_nv_win_fs_lock_inited = 0;

static void nv_win_fs_lock_init(void) {
  if (InterlockedCompareExchange(&g_nv_win_fs_lock_inited, 1, 0) == 0) {
    InitializeCriticalSection(&g_nv_win_fs_lock);
  }
}

static int nv_win_fs_find_id(long long id) {
  for (int i = 0; i < NV_WIN_FS_MAX; i++) {
    if (g_nv_win_fs_slots[i].used && g_nv_win_fs_slots[i].id == id) return i;
  }
  return -1;
}

static int nv_win_fs_find_free(void) {
  for (int i = 0; i < NV_WIN_FS_MAX; i++) {
    if (!g_nv_win_fs_slots[i].used) return i;
  }
  return -1;
}

static const char* nv_win_fs_action_str(int act) {
  switch (act) {
    case 1:
      return "created";
    case 2:
      return "deleted";
    case 3:
      return "renamed";
    default:
      return "modified";
  }
}

typedef struct nv_win_fs_thread_ctx {
  int slot_idx;
} nv_win_fs_thread_ctx_t;

static DWORD WINAPI nv_win_fs_thread_proc(void* param) {
  nv_win_fs_thread_ctx_t* tctx = (nv_win_fs_thread_ctx_t*)param;
  if (!tctx) return 0;
  int idx = tctx->slot_idx;
  HeapFree(GetProcessHeap(), 0, tctx);

  nv_win_fs_lock_init();
  EnterCriticalSection(&g_nv_win_fs_lock);
  if (idx < 0 || idx >= NV_WIN_FS_MAX || !g_nv_win_fs_slots[idx].used) {
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return 0;
  }
  HANDLE hDir = g_nv_win_fs_slots[idx].h_dir;
  HANDLE stop = g_nv_win_fs_slots[idx].stop_evt;
  HWND hwnd = g_nv_win_fs_slots[idx].hwnd;
  nv_window_t* win = g_nv_win_fs_slots[idx].window;
  long long wid = g_nv_win_fs_slots[idx].id;
  char root_copy[1024];
  memcpy(root_copy, g_nv_win_fs_slots[idx].root_utf8, sizeof(root_copy));
  LeaveCriticalSection(&g_nv_win_fs_lock);

  unsigned char buf[8192];
  for (;;) {
    if (WaitForSingleObject(stop, 0) == WAIT_OBJECT_0) break;
    DWORD br = 0;
    memset(buf, 0, sizeof(buf));
    if (!ReadDirectoryChangesW(hDir, buf, sizeof(buf), TRUE /* subtree */,
                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                   FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                               &br, NULL, NULL)) {
      DWORD err = GetLastError();
      if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE) break;
      Sleep(50);
      continue;
    }
    if (br == 0) continue;

    EnterCriticalSection(&g_nv_win_fs_lock);
    int still = (idx >= 0 && idx < NV_WIN_FS_MAX && g_nv_win_fs_slots[idx].used &&
                 g_nv_win_fs_slots[idx].id == wid);
    HWND hwnd_post = still ? g_nv_win_fs_slots[idx].hwnd : NULL;
    LeaveCriticalSection(&g_nv_win_fs_lock);
    if (!hwnd_post || !win) continue;

    for (FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)buf;;) {
      int act = 0;
      switch (info->Action) {
        case FILE_ACTION_ADDED:
          act = 1;
          break;
        case FILE_ACTION_REMOVED:
          act = 2;
          break;
        case FILE_ACTION_RENAMED_NEW_NAME:
          act = 3;
          break;
        case FILE_ACTION_MODIFIED:
        default:
          act = 0;
          break;
      }

      char rel[1024];
      rel[0] = '\0';
      if (info->FileNameLength > 0) {
        int nw = (int)(info->FileNameLength / sizeof(WCHAR));
        if (nw > 0 && nw < (int)(sizeof(rel) / sizeof(WCHAR))) {
          WCHAR wtmp[512];
          memcpy(wtmp, info->FileName, info->FileNameLength);
          wtmp[nw] = L'\0';
          WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, rel, (int)sizeof(rel), NULL, NULL);
        }
      }

      char full[2048];
      if (rel[0]) {
        snprintf(full, sizeof(full), "%s\\%s", root_copy, rel);
      } else {
        strncpy(full, root_copy, sizeof(full) - 1);
        full[sizeof(full) - 1] = '\0';
      }

      nv_win_fs_change_msg_t* msg =
          (nv_win_fs_change_msg_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*msg));
      if (msg) {
        msg->window = win;
        msg->watch_id = wid;
        strncpy(msg->path, full, sizeof(msg->path) - 1);
        strncpy(msg->type, nv_win_fs_action_str(act), sizeof(msg->type) - 1);
        if (!PostMessageW(hwnd_post, WM_NV_FS_CHANGE, 0, (LPARAM)msg)) {
          HeapFree(GetProcessHeap(), 0, msg);
        }
      }

      if (info->NextEntryOffset == 0) break;
      info = (FILE_NOTIFY_INFORMATION*)((unsigned char*)info + info->NextEntryOffset);
    }
  }
  return 0;
}

static void nv_win_fs_close_slot_handles(int idx) {
  if (idx < 0 || idx >= NV_WIN_FS_MAX) return;
  if (g_nv_win_fs_slots[idx].stop_evt) {
    SetEvent(g_nv_win_fs_slots[idx].stop_evt);
  }
  if (g_nv_win_fs_slots[idx].h_dir) {
    CancelIoEx(g_nv_win_fs_slots[idx].h_dir, NULL);
  }
  if (g_nv_win_fs_slots[idx].thread) {
    WaitForSingleObject(g_nv_win_fs_slots[idx].thread, 15000);
    CloseHandle(g_nv_win_fs_slots[idx].thread);
    g_nv_win_fs_slots[idx].thread = NULL;
  }
  if (g_nv_win_fs_slots[idx].h_dir) {
    CloseHandle(g_nv_win_fs_slots[idx].h_dir);
    g_nv_win_fs_slots[idx].h_dir = NULL;
  }
  if (g_nv_win_fs_slots[idx].stop_evt) {
    CloseHandle(g_nv_win_fs_slots[idx].stop_evt);
    g_nv_win_fs_slots[idx].stop_evt = NULL;
  }
}

NV_INTERNAL int nv_win_fs_watch_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                        struct nv_win_window_platform* platform, LRESULT* lr_out) {
  (void)hwnd;
  (void)wparam;
  (void)platform;
  if (msg != WM_NV_FS_CHANGE) return 0;
  nv_win_fs_change_msg_t* m = (nv_win_fs_change_msg_t*)lparam;
  if (m && m->window) {
    nv_fs_changed_emit(m->window, m->watch_id, m->path, m->type);
  }
  if (m) HeapFree(GetProcessHeap(), 0, m);
  if (lr_out) *lr_out = 0;
  return 1;
}

NV_INTERNAL int nv_win_fs_watch_start(long long id, const char* path, nv_window_t* w) {
  if (id <= 0 || !path || !path[0] || !w) return -1;
  HWND hwnd = nv_win_window_hwnd(w);
  if (!hwnd) return -1;

  nv_win_fs_lock_init();
  EnterCriticalSection(&g_nv_win_fs_lock);
  int ex = nv_win_fs_find_id(id);
  if (ex >= 0) {
    nv_win_fs_close_slot_handles(ex);
    memset(&g_nv_win_fs_slots[ex], 0, sizeof(g_nv_win_fs_slots[0]));
  }
  int idx = nv_win_fs_find_free();
  if (idx < 0) {
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return -1;
  }

  WCHAR wpath[MAX_PATH];
  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH) <= 0) {
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return -1;
  }

  HANDLE hDir = CreateFileW(wpath, FILE_LIST_DIRECTORY,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (hDir == INVALID_HANDLE_VALUE) {
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return -1;
  }

  HANDLE stop = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!stop) {
    CloseHandle(hDir);
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return -1;
  }

  g_nv_win_fs_slots[idx].used = 1;
  g_nv_win_fs_slots[idx].id = id;
  g_nv_win_fs_slots[idx].window = w;
  g_nv_win_fs_slots[idx].hwnd = hwnd;
  g_nv_win_fs_slots[idx].h_dir = hDir;
  g_nv_win_fs_slots[idx].stop_evt = stop;
  strncpy(g_nv_win_fs_slots[idx].root_utf8, path, sizeof(g_nv_win_fs_slots[idx].root_utf8) - 1);
  g_nv_win_fs_slots[idx].root_utf8[sizeof(g_nv_win_fs_slots[idx].root_utf8) - 1] = '\0';

  nv_win_fs_thread_ctx_t* tctx =
      (nv_win_fs_thread_ctx_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*tctx));
  if (!tctx) {
    CloseHandle(stop);
    CloseHandle(hDir);
    memset(&g_nv_win_fs_slots[idx], 0, sizeof(g_nv_win_fs_slots[0]));
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return -1;
  }
  tctx->slot_idx = idx;

  LeaveCriticalSection(&g_nv_win_fs_lock);

  HANDLE th = CreateThread(NULL, 0, nv_win_fs_thread_proc, tctx, 0, NULL);
  if (!th) {
    HeapFree(GetProcessHeap(), 0, tctx);
    CloseHandle(stop);
    CloseHandle(hDir);
    EnterCriticalSection(&g_nv_win_fs_lock);
    memset(&g_nv_win_fs_slots[idx], 0, sizeof(g_nv_win_fs_slots[0]));
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return -1;
  }

  EnterCriticalSection(&g_nv_win_fs_lock);
  if (g_nv_win_fs_slots[idx].used && g_nv_win_fs_slots[idx].id == id) {
    g_nv_win_fs_slots[idx].thread = th;
  }
  LeaveCriticalSection(&g_nv_win_fs_lock);
  return 0;
}

NV_INTERNAL void nv_win_fs_watch_stop(long long id) {
  nv_win_fs_lock_init();
  EnterCriticalSection(&g_nv_win_fs_lock);
  int idx = nv_win_fs_find_id(id);
  if (idx < 0) {
    LeaveCriticalSection(&g_nv_win_fs_lock);
    return;
  }
  nv_win_fs_close_slot_handles(idx);
  memset(&g_nv_win_fs_slots[idx], 0, sizeof(g_nv_win_fs_slots[0]));
  LeaveCriticalSection(&g_nv_win_fs_lock);
}

NV_INTERNAL void nv_win_fs_watch_detach_for_window(nv_window_t* w) {
  if (!w) return;
  nv_win_fs_lock_init();
  EnterCriticalSection(&g_nv_win_fs_lock);
  for (int i = 0; i < NV_WIN_FS_MAX; i++) {
    if (g_nv_win_fs_slots[i].used && g_nv_win_fs_slots[i].window == w) {
      nv_win_fs_close_slot_handles(i);
      memset(&g_nv_win_fs_slots[i], 0, sizeof(g_nv_win_fs_slots[i]));
    }
  }
  LeaveCriticalSection(&g_nv_win_fs_lock);
}

#endif /* _WIN32 */
