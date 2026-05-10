/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifdef _WIN32

#include "nv_win_tray.h"

#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nv_window_internal.h"

#define WM_NV_TRAY_MSG (WM_USER + 88)
#define WM_NV_TRAY_SYNC (WM_APP + 5)

#define NV_MAX_TRAYS 8
#define NV_TRAY_CMD_BASE 0xE000
#define NV_TRAY_CMD_STRIDE 64

enum {
  NV_TRAY_SYNC_CREATE = 1,
  NV_TRAY_SYNC_DESTROY,
  NV_TRAY_SYNC_SET_ICON,
  NV_TRAY_SYNC_SET_TOOLTIP,
  NV_TRAY_SYNC_SET_MENU
};

typedef struct nv_win_tray_sync_ctx {
  int op;
  int rc;
  long long tray_id;
  nv_window_t* window;
  const char* icon_path;
  const char* tooltip;
  const char* set_icon_path;
  const char* set_tooltip_text;
  const char** menu_labels;
  const long long* menu_item_ids;
  int menu_count;
} nv_win_tray_sync_ctx_t;

typedef struct nv_win_tray_slot {
  int used;
  long long id;
  nv_window_t* window;
  HWND hwnd;
  UINT shell_uid;
  HICON hIcon;
  HMENU hMenu;
  long long menu_item_ids[NV_TRAY_CMD_STRIDE];
  int menu_count;
} nv_win_tray_slot_t;

static nv_win_tray_slot_t g_trays[NV_MAX_TRAYS];

static int nv_win_tray_unit_headless(void) {
  const char* e = getenv("NV_TRAY_UNIT_HEADLESS");
  return (e && (e[0] == '1' || e[0] == 'y' || e[0] == 'Y')) ? 1 : 0;
}

static int nv_win_tray_utf8_to_wide(const char* utf8, WCHAR* out, int out_wchars) {
  if (!utf8 || !out || out_wchars < 2) return -1;
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (n <= 0 || n > out_wchars) return -1;
  if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, n) <= 0) return -1;
  return 0;
}

static int nv_win_tray_index_by_id(long long tray_id) {
  for (int i = 0; i < NV_MAX_TRAYS; i++) {
    if (g_trays[i].used && g_trays[i].id == tray_id) return i;
  }
  return -1;
}

static int nv_win_tray_index_by_shell_uid(UINT uid) {
  for (int i = 0; i < NV_MAX_TRAYS; i++) {
    if (g_trays[i].used && g_trays[i].shell_uid == uid) return i;
  }
  return -1;
}

static void nv_win_tray_free_menu(int slot) {
  if (slot < 0 || slot >= NV_MAX_TRAYS) return;
  if (g_trays[slot].hMenu) {
    DestroyMenu(g_trays[slot].hMenu);
    g_trays[slot].hMenu = NULL;
  }
  g_trays[slot].menu_count = 0;
}

static void nv_win_tray_free_icon(int slot) {
  if (slot < 0 || slot >= NV_MAX_TRAYS) return;
  if (g_trays[slot].hIcon) {
    DestroyIcon(g_trays[slot].hIcon);
    g_trays[slot].hIcon = NULL;
  }
}

static void nv_win_tray_clear_slot(int slot) {
  if (slot < 0 || slot >= NV_MAX_TRAYS) return;
  nv_win_tray_free_menu(slot);
  nv_win_tray_free_icon(slot);
  ZeroMemory(&g_trays[slot], sizeof(g_trays[slot]));
}

static void nv_win_tray_copy_sz_tip(NOTIFYICONDATAW* nid, const WCHAR* src) {
  int j = 0;
  if (!nid) return;
  if (src) {
    for (; j < 127 && src[j]; j++) nid->szTip[j] = src[j];
  }
  nid->szTip[j] = L'\0';
}

static HICON nv_win_tray_load_icon_from_path(const char* icon_path) {
  WCHAR wpath[MAX_PATH];
  if (icon_path && icon_path[0]) {
    if (nv_win_tray_utf8_to_wide(icon_path, wpath, MAX_PATH) != 0) return NULL;
    {
      HICON hi = (HICON)LoadImageW(NULL, wpath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
      if (hi) return hi;
    }
    {
      HICON hi = (HICON)LoadImageW(NULL, wpath, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
      if (hi) return hi;
    }
  }
  return (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON,
                           GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
}

static void nv_win_tray_build_nid(NOTIFYICONDATAW* nid, int slot) {
  ZeroMemory(nid, sizeof(*nid));
  nid->cbSize = sizeof(NOTIFYICONDATAW);
  nid->hWnd = g_trays[slot].hwnd;
  nid->uID = g_trays[slot].shell_uid;
  nid->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid->uCallbackMessage = WM_NV_TRAY_MSG;
  nid->hIcon = g_trays[slot].hIcon;
}

static int nv_win_tray_send_clicked(int slot) {
  if (slot < 0 || !g_trays[slot].window) return -1;
  char buf[64];
  if (snprintf(buf, sizeof(buf), "{\"id\":%lld}", g_trays[slot].id) < 0) return -1;
  nv_send(g_trays[slot].window, "tray.clicked", buf);
  return 0;
}

static int nv_win_tray_send_menu_action(int slot, long long item_id) {
  if (slot < 0 || !g_trays[slot].window) return -1;
  char buf[96];
  if (snprintf(buf, sizeof(buf), "{\"id\":%lld,\"itemId\":%lld}", g_trays[slot].id, item_id) < 0) return -1;
  nv_send(g_trays[slot].window, "tray.menuAction", buf);
  return 0;
}

static int nv_win_tray_do_create(nv_win_tray_sync_ctx_t* c) {
  HWND hwnd;
  if (!c || !c->window || c->tray_id <= 0) return -1;
  hwnd = nv_win_window_hwnd(c->window);
  if (!hwnd) {
    /* nv-ops unit tests: nv_window_alloc without platform (set NV_TRAY_UNIT_HEADLESS=1). */
    if (nv_win_tray_unit_headless()) {
      int slot;
      if (nv_win_tray_index_by_id(c->tray_id) >= 0) return -1;
      slot = -1;
      for (int i = 0; i < NV_MAX_TRAYS; i++) {
        if (!g_trays[i].used) {
          slot = i;
          break;
        }
      }
      if (slot < 0) return -1;
      g_trays[slot].used = 1;
      g_trays[slot].id = c->tray_id;
      g_trays[slot].window = c->window;
      g_trays[slot].hwnd = NULL;
      g_trays[slot].shell_uid = (UINT)(slot + 1u);
      return 0;
    }
    return -1;
  }
  if (nv_win_tray_index_by_id(c->tray_id) >= 0) return -1;
  int slot = -1;
  for (int i = 0; i < NV_MAX_TRAYS; i++) {
    if (!g_trays[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return -1;

  g_trays[slot].used = 1;
  g_trays[slot].id = c->tray_id;
  g_trays[slot].window = c->window;
  g_trays[slot].hwnd = hwnd;
  g_trays[slot].shell_uid = (UINT)(slot + 1u);
  g_trays[slot].hIcon = nv_win_tray_load_icon_from_path(c->icon_path);
  if (!g_trays[slot].hIcon) {
    nv_win_tray_clear_slot(slot);
    return -1;
  }

  {
    NOTIFYICONDATAW nid;
    nv_win_tray_build_nid(&nid, slot);
    if (c->tooltip && c->tooltip[0]) {
      WCHAR wtip[128];
      if (nv_win_tray_utf8_to_wide(c->tooltip, wtip, 128) == 0) {
        nv_win_tray_copy_sz_tip(&nid, wtip);
      }
    }
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
      nv_win_tray_clear_slot(slot);
      return -1;
    }
    {
      NOTIFYICONDATAW ver = nid;
      ver.uVersion = NOTIFYICON_VERSION_4;
      Shell_NotifyIconW(NIM_SETVERSION, &ver);
    }
  }
  return 0;
}

static int nv_win_tray_do_destroy(long long tray_id) {
  int slot = nv_win_tray_index_by_id(tray_id);
  NOTIFYICONDATAW nid;
  if (slot < 0) return -1;
  if (!g_trays[slot].hwnd) {
    nv_win_tray_clear_slot(slot);
    return 0;
  }
  ZeroMemory(&nid, sizeof(nid));
  nid.cbSize = sizeof(nid);
  nid.hWnd = g_trays[slot].hwnd;
  nid.uID = g_trays[slot].shell_uid;
  Shell_NotifyIconW(NIM_DELETE, &nid);
  nv_win_tray_clear_slot(slot);
  return 0;
}

static int nv_win_tray_do_set_icon(nv_win_tray_sync_ctx_t* c) {
  int slot;
  HICON new_icon;
  HICON old_icon;
  NOTIFYICONDATAW nid;
  if (!c || !c->set_icon_path || !c->set_icon_path[0]) return -1;
  slot = nv_win_tray_index_by_id(c->tray_id);
  if (slot < 0) return -1;
  if (!g_trays[slot].hwnd && nv_win_tray_unit_headless()) return 0;
  new_icon = nv_win_tray_load_icon_from_path(c->set_icon_path);
  if (!new_icon) return -1;
  old_icon = g_trays[slot].hIcon;
  g_trays[slot].hIcon = new_icon;
  nv_win_tray_build_nid(&nid, slot);
  if (!Shell_NotifyIconW(NIM_MODIFY, &nid)) {
    g_trays[slot].hIcon = old_icon;
    DestroyIcon(new_icon);
    return -1;
  }
  if (old_icon) DestroyIcon(old_icon);
  return 0;
}

static int nv_win_tray_do_set_tooltip(nv_win_tray_sync_ctx_t* c) {
  int slot;
  NOTIFYICONDATAW nid;
  if (!c || !c->set_tooltip_text) return -1;
  slot = nv_win_tray_index_by_id(c->tray_id);
  if (slot < 0) return -1;
  if (!g_trays[slot].hwnd && nv_win_tray_unit_headless()) return 0;
  nv_win_tray_build_nid(&nid, slot);
  if (c->set_tooltip_text[0]) {
    WCHAR wtip[128];
    if (nv_win_tray_utf8_to_wide(c->set_tooltip_text, wtip, 128) != 0) return -1;
    nv_win_tray_copy_sz_tip(&nid, wtip);
  } else {
    nid.szTip[0] = L'\0';
  }
  return Shell_NotifyIconW(NIM_MODIFY, &nid) ? 0 : -1;
}

static int nv_win_tray_do_set_menu(nv_win_tray_sync_ctx_t* c) {
  int slot;
  HMENU menu;
  if (!c) return -1;
  slot = nv_win_tray_index_by_id(c->tray_id);
  if (slot < 0) return -1;
  if (!g_trays[slot].hwnd && nv_win_tray_unit_headless()) {
    nv_win_tray_free_menu(slot);
    if (c->menu_count <= 0) return 0;
    if (c->menu_count > NV_TRAY_CMD_STRIDE) return -1;
    g_trays[slot].menu_count = c->menu_count;
    for (int i = 0; i < c->menu_count; i++) {
      g_trays[slot].menu_item_ids[i] = c->menu_item_ids ? c->menu_item_ids[i] : 0;
    }
    return 0;
  }
  nv_win_tray_free_menu(slot);
  if (c->menu_count <= 0) return 0;
  if (c->menu_count > NV_TRAY_CMD_STRIDE) return -1;
  menu = CreatePopupMenu();
  if (!menu) return -1;
  for (int i = 0; i < c->menu_count; i++) {
    UINT cmd = (UINT)(NV_TRAY_CMD_BASE + (unsigned)slot * NV_TRAY_CMD_STRIDE + (unsigned)i);
    long long iid = c->menu_item_ids ? c->menu_item_ids[i] : 0;
    g_trays[slot].menu_item_ids[i] = iid;
    if (iid == -1LL) {
      if (!AppendMenuW(menu, MF_SEPARATOR, 0, NULL)) {
        DestroyMenu(menu);
        return -1;
      }
    } else {
      WCHAR wlabel[512];
      const char* lbl = (c->menu_labels && c->menu_labels[i]) ? c->menu_labels[i] : "";
      if (nv_win_tray_utf8_to_wide(lbl, wlabel, 512) != 0) wlabel[0] = L'\0';
      if (!AppendMenuW(menu, MF_STRING, cmd, wlabel)) {
        DestroyMenu(menu);
        return -1;
      }
    }
  }
  g_trays[slot].hMenu = menu;
  g_trays[slot].menu_count = c->menu_count;
  return 0;
}

static int nv_win_tray_run_sync_ctx(nv_window_t* w, nv_win_tray_sync_ctx_t* ctx) {
  HWND hwnd = nv_win_window_hwnd(w);
  if (!ctx || !w) return -1;
  if (!hwnd && nv_win_tray_unit_headless()) {
    switch (ctx->op) {
      case NV_TRAY_SYNC_CREATE:
        ctx->rc = nv_win_tray_do_create(ctx);
        break;
      case NV_TRAY_SYNC_DESTROY:
        ctx->rc = nv_win_tray_do_destroy(ctx->tray_id);
        break;
      case NV_TRAY_SYNC_SET_ICON:
        ctx->rc = nv_win_tray_do_set_icon(ctx);
        break;
      case NV_TRAY_SYNC_SET_TOOLTIP:
        ctx->rc = nv_win_tray_do_set_tooltip(ctx);
        break;
      case NV_TRAY_SYNC_SET_MENU:
        ctx->rc = nv_win_tray_do_set_menu(ctx);
        break;
      default:
        ctx->rc = -1;
        break;
    }
    return 0;
  }
  if (!hwnd) return -1;
  if (!IsWindow(hwnd)) return -1;
  SendMessageW(hwnd, WM_NV_TRAY_SYNC, 0, (LPARAM)(void*)ctx);
  return 0;
}

NV_INTERNAL int nv_win_tray_create(long long tray_id, const char* icon_path, const char* tooltip,
                                   nv_window_t* w) {
  nv_win_tray_sync_ctx_t ctx;
  if (!w) return -1;
  ZeroMemory(&ctx, sizeof(ctx));
  ctx.op = NV_TRAY_SYNC_CREATE;
  ctx.tray_id = tray_id;
  ctx.window = w;
  ctx.icon_path = icon_path;
  ctx.tooltip = tooltip;
  ctx.rc = -1;
  if (nv_win_tray_run_sync_ctx(w, &ctx) != 0) return -1;
  return ctx.rc;
}

NV_INTERNAL int nv_win_tray_destroy(long long tray_id) {
  int slot = nv_win_tray_index_by_id(tray_id);
  nv_win_tray_sync_ctx_t ctx;
  nv_window_t* w;
  if (slot < 0) return -1;
  w = g_trays[slot].window;
  if (!w) return -1;
  ZeroMemory(&ctx, sizeof(ctx));
  ctx.op = NV_TRAY_SYNC_DESTROY;
  ctx.tray_id = tray_id;
  ctx.window = w;
  ctx.rc = -1;
  if (nv_win_tray_run_sync_ctx(w, &ctx) != 0) return -1;
  return ctx.rc;
}

NV_INTERNAL int nv_win_tray_set_icon(long long tray_id, const char* icon_path) {
  nv_win_tray_sync_ctx_t ctx;
  int slot = nv_win_tray_index_by_id(tray_id);
  nv_window_t* w;
  if (slot < 0) return -1;
  w = g_trays[slot].window;
  if (!w) return -1;
  ZeroMemory(&ctx, sizeof(ctx));
  ctx.op = NV_TRAY_SYNC_SET_ICON;
  ctx.tray_id = tray_id;
  ctx.window = w;
  ctx.set_icon_path = icon_path;
  ctx.rc = -1;
  if (nv_win_tray_run_sync_ctx(w, &ctx) != 0) return -1;
  return ctx.rc;
}

NV_INTERNAL int nv_win_tray_set_tooltip(long long tray_id, const char* tooltip) {
  nv_win_tray_sync_ctx_t ctx;
  int slot = nv_win_tray_index_by_id(tray_id);
  nv_window_t* w;
  if (slot < 0) return -1;
  w = g_trays[slot].window;
  if (!w) return -1;
  ZeroMemory(&ctx, sizeof(ctx));
  ctx.op = NV_TRAY_SYNC_SET_TOOLTIP;
  ctx.tray_id = tray_id;
  ctx.window = w;
  ctx.set_tooltip_text = tooltip;
  ctx.rc = -1;
  if (nv_win_tray_run_sync_ctx(w, &ctx) != 0) return -1;
  return ctx.rc;
}

NV_INTERNAL int nv_win_tray_set_menu(long long tray_id, const char** labels, const long long* item_ids,
                                     int count) {
  nv_win_tray_sync_ctx_t ctx;
  int slot = nv_win_tray_index_by_id(tray_id);
  nv_window_t* w;
  if (slot < 0) return -1;
  w = g_trays[slot].window;
  if (!w) return -1;
  ZeroMemory(&ctx, sizeof(ctx));
  ctx.op = NV_TRAY_SYNC_SET_MENU;
  ctx.tray_id = tray_id;
  ctx.window = w;
  ctx.menu_labels = labels;
  ctx.menu_item_ids = item_ids;
  ctx.menu_count = count;
  ctx.rc = -1;
  if (nv_win_tray_run_sync_ctx(w, &ctx) != 0) return -1;
  return ctx.rc;
}

NV_INTERNAL void nv_win_tray_detach_hwnd(HWND hwnd) {
  if (!hwnd) return;
  for (int i = 0; i < NV_MAX_TRAYS; i++) {
    if (g_trays[i].used && g_trays[i].hwnd == hwnd) {
      NOTIFYICONDATAW nid;
      ZeroMemory(&nid, sizeof(nid));
      nid.cbSize = sizeof(nid);
      nid.hWnd = hwnd;
      nid.uID = g_trays[i].shell_uid;
      Shell_NotifyIconW(NIM_DELETE, &nid);
      nv_win_tray_clear_slot(i);
    }
  }
}

NV_INTERNAL int nv_win_tray_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                    struct nv_win_window_platform* platform, LRESULT* lr_out) {
  (void)platform;
  if (!lr_out) return 0;
  *lr_out = 0;
  if (msg == WM_NV_TRAY_SYNC) {
    nv_win_tray_sync_ctx_t* c = (nv_win_tray_sync_ctx_t*)(void*)lparam;
    if (!c) return 1;
    switch (c->op) {
      case NV_TRAY_SYNC_CREATE:
        c->rc = nv_win_tray_do_create(c);
        break;
      case NV_TRAY_SYNC_DESTROY:
        c->rc = nv_win_tray_do_destroy(c->tray_id);
        break;
      case NV_TRAY_SYNC_SET_ICON:
        c->rc = nv_win_tray_do_set_icon(c);
        break;
      case NV_TRAY_SYNC_SET_TOOLTIP:
        c->rc = nv_win_tray_do_set_tooltip(c);
        break;
      case NV_TRAY_SYNC_SET_MENU:
        c->rc = nv_win_tray_do_set_menu(c);
        break;
      default:
        c->rc = -1;
        break;
    }
    return 1;
  }
  if (msg == WM_NV_TRAY_MSG) {
    UINT uid = (UINT)wparam;
    UINT ev = (UINT)LOWORD(lparam);
    int slot = nv_win_tray_index_by_shell_uid(uid);
    if (slot < 0) return 0;
    if (g_trays[slot].hwnd != hwnd) return 0;

    if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
      if (g_trays[slot].hMenu) {
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);
        TrackPopupMenuEx(g_trays[slot].hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x,
                         pt.y, hwnd, NULL);
        PostMessageW(hwnd, WM_NULL, 0, 0);
      }
      *lr_out = 0;
      return 1;
    }
    if (ev == WM_LBUTTONUP || ev == NIN_SELECT || ev == NIN_KEYSELECT) {
      nv_win_tray_send_clicked(slot);
      *lr_out = 0;
      return 1;
    }
    return 0;
  }
  if (msg == WM_COMMAND && HIWORD(wparam) == 0) {
    UINT id = LOWORD(wparam);
    if (id >= NV_TRAY_CMD_BASE && id < NV_TRAY_CMD_BASE + NV_MAX_TRAYS * NV_TRAY_CMD_STRIDE) {
      unsigned off = id - NV_TRAY_CMD_BASE;
      int slot2 = (int)(off / NV_TRAY_CMD_STRIDE);
      int idx = (int)(off % NV_TRAY_CMD_STRIDE);
      if (slot2 < 0 || slot2 >= NV_MAX_TRAYS || !g_trays[slot2].used || g_trays[slot2].hwnd != hwnd) {
        return 0;
      }
      if (idx < 0 || idx >= g_trays[slot2].menu_count) {
        *lr_out = 0;
        return 1;
      }
      {
        long long iid = g_trays[slot2].menu_item_ids[idx];
        if (iid != -1LL) nv_win_tray_send_menu_action(slot2, iid);
      }
      *lr_out = 0;
      return 1;
    }
  }
  return 0;
}

#endif /* _WIN32 */
