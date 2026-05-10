/* =============================================================================
 * nv_win_hotkey.c — RegisterHotKey + WM_HOTKEY for global shortcuts
 * =============================================================================
 */

#include "nv_win_hotkey.h"
#include "nv_win_tray.h"
#include "nv_window_internal.h"
#include "nv.h"
#include <windows.h>

typedef struct {
  int used;
  int win_hotkey_id;
  long long handle;
  HWND hwnd;
  void (*cb)(long long handle, void* ctx);
  void* ctx;
} nv_win_hk_slot_t;

#define NV_WIN_HK_MAX 64
static nv_win_hk_slot_t g_win_hk[NV_WIN_HK_MAX];
static int g_win_hk_next_id = 1;

static UINT nv_spec_vk(const nv_hotkey_combo_t* s) {
  if (!s) return 0;
  if (s->is_fn) {
    if (s->fn_index < 1 || s->fn_index > 24) return 0;
    return (UINT)(VK_F1 + (s->fn_index - 1));
  }
  switch (s->special) {
    case NV_HOTKEY_SPECIAL_SPACE:
      return VK_SPACE;
    case NV_HOTKEY_SPECIAL_TAB:
      return VK_TAB;
    case NV_HOTKEY_SPECIAL_RETURN:
      return VK_RETURN;
    case NV_HOTKEY_SPECIAL_ESCAPE:
      return VK_ESCAPE;
    default:
      break;
  }
  if (s->key_char >= 'a' && s->key_char <= 'z') return (UINT)(s->key_char - 'a' + 'A');
  if (s->key_char >= '0' && s->key_char <= '9') return (UINT)s->key_char;
  return 0;
}

static UINT nv_spec_mod(const nv_hotkey_combo_t* s) {
  if (!s) return 0;
  UINT m = MOD_NOREPEAT;
  if (s->mod_flags & NV_HOTKEY_MOD_SHIFT) m |= MOD_SHIFT;
  if (s->mod_flags & NV_HOTKEY_MOD_ALT) m |= MOD_ALT;
  if (s->mod_flags & NV_HOTKEY_MOD_CTRL) m |= MOD_CONTROL;
  if (s->mod_flags & NV_HOTKEY_MOD_CMD_OR_CTRL) m |= MOD_CONTROL;
  if (s->mod_flags & NV_HOTKEY_MOD_META) m |= MOD_WIN;
  return m;
}

NV_INTERNAL int nv_win_register_hotkey(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                                      void (*cb)(long long handle, void* ctx), void* ctx) {
  if (!w || !spec || !cb) return -1;
  HWND hwnd = nv_win_window_hwnd(w);
  if (!hwnd) return -1;
  UINT vk = nv_spec_vk(spec);
  UINT mod = nv_spec_mod(spec);
  if (vk == 0) return -1;

  int slot = -1;
  for (int i = 0; i < NV_WIN_HK_MAX; i++) {
    if (!g_win_hk[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return -1;

  int wid = g_win_hk_next_id++;
  if (wid > 0xBFFF) {
    g_win_hk_next_id = 1;
    wid = 1;
  }

  if (!RegisterHotKey(hwnd, wid, mod, vk)) {
    return -1;
  }

  g_win_hk[slot].used = 1;
  g_win_hk[slot].win_hotkey_id = wid;
  g_win_hk[slot].handle = handle;
  g_win_hk[slot].hwnd = hwnd;
  g_win_hk[slot].cb = cb;
  g_win_hk[slot].ctx = ctx;
  return 0;
}

NV_INTERNAL void nv_win_unregister_hotkey(long long handle) {
  for (int i = 0; i < NV_WIN_HK_MAX; i++) {
    if (!g_win_hk[i].used || g_win_hk[i].handle != handle) continue;
    if (g_win_hk[i].hwnd) UnregisterHotKey(g_win_hk[i].hwnd, g_win_hk[i].win_hotkey_id);
    g_win_hk[i].used = 0;
    g_win_hk[i].hwnd = NULL;
    g_win_hk[i].cb = NULL;
    g_win_hk[i].ctx = NULL;
    return;
  }
}

NV_INTERNAL int nv_win_hotkey_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                      struct nv_win_window_platform* platform, LRESULT* lr_out) {
  (void)lparam;
  (void)platform;
  if (msg != WM_HOTKEY || !lr_out) return 0;
  int id = (int)wparam;
  for (int i = 0; i < NV_WIN_HK_MAX; i++) {
    if (!g_win_hk[i].used || g_win_hk[i].hwnd != hwnd) continue;
    if (g_win_hk[i].win_hotkey_id != id) continue;
    if (g_win_hk[i].cb) g_win_hk[i].cb(g_win_hk[i].handle, g_win_hk[i].ctx);
    *lr_out = 0;
    return 1;
  }
  return 0;
}
