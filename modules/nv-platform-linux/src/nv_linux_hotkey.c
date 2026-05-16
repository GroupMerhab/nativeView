/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_linux_hotkey.c — X11 global hotkeys (XGrabKey + root Gdk filter)
 *
 * Wayland: no portable global shortcut API — returns NV_HOTKEY_RC_NOT_SUPPORTED
 * when GDK reports a Wayland display name.
 * =============================================================================
 */

#include "nv_window_internal.h"
#include "nv.h"

#include <gdk/gdk.h>
#include <string.h>
#include <strings.h>

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

typedef struct {
  int used;
  long long handle;
  unsigned x_mods;
  unsigned keycode;
  void (*cb)(long long handle, void* ctx);
  void* ctx;
} nv_lx_hk_slot_t;

#define NV_LX_HK_MAX 32
static nv_lx_hk_slot_t g_lx_hk[NV_LX_HK_MAX];
static int g_lx_filter_installed;

#if defined(GDK_WINDOWING_X11)

static int nv_lx_is_wayland_session(void) {
  GdkDisplay* d = gdk_display_get_default();
  if (!d) return 0;
  const char* n = gdk_display_get_name(d);
  if (!n) return 0;
  return (strncasecmp(n, "wayland", 7) == 0);
}

static unsigned nv_lx_mod(const nv_hotkey_combo_t* s) {
  unsigned m = 0;
  if (!s) return 0;
  if (s->mod_flags & NV_HOTKEY_MOD_SHIFT) m |= ShiftMask;
  if (s->mod_flags & NV_HOTKEY_MOD_CTRL) m |= ControlMask;
  if (s->mod_flags & NV_HOTKEY_MOD_CMD_OR_CTRL) m |= ControlMask;
  if (s->mod_flags & NV_HOTKEY_MOD_ALT) m |= Mod1Mask;
  if (s->mod_flags & NV_HOTKEY_MOD_META) m |= Mod4Mask;
  return m;
}

static KeySym nv_lx_keysym(const nv_hotkey_combo_t* s) {
  if (!s) return NoSymbol;
  if (s->is_fn) {
    if (s->fn_index < 1 || s->fn_index > 24) return NoSymbol;
    return (KeySym)(XK_F1 + (s->fn_index - 1));
  }
  switch (s->special) {
    case NV_HOTKEY_SPECIAL_SPACE:
      return XK_space;
    case NV_HOTKEY_SPECIAL_TAB:
      return XK_Tab;
    case NV_HOTKEY_SPECIAL_RETURN:
      return XK_Return;
    case NV_HOTKEY_SPECIAL_ESCAPE:
      return XK_Escape;
    default:
      break;
  }
  if (s->key_char >= 'a' && s->key_char <= 'z') return (KeySym)(XK_a + (s->key_char - 'a'));
  if (s->key_char >= '0' && s->key_char <= '9') return (KeySym)(XK_0 + (s->key_char - '0'));
  return NoSymbol;
}

static int nv_lx_mod_match(unsigned want, unsigned state) {
  unsigned m = state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
  return m == want;
}

static GdkFilterReturn nv_lx_hk_filter(GdkXEvent* gx, GdkEvent* ev, gpointer data) {
  (void)data;
  (void)ev;
  XEvent* xev = (XEvent*)gx;
  if (!xev || xev->type != KeyPress) return GDK_FILTER_CONTINUE;
  unsigned st = (unsigned)xev->xkey.state;
  unsigned kc = (unsigned)xev->xkey.keycode;
  for (int i = 0; i < NV_LX_HK_MAX; i++) {
    if (!g_lx_hk[i].used) continue;
    if (g_lx_hk[i].keycode != kc) continue;
    if (!nv_lx_mod_match(g_lx_hk[i].x_mods, st)) continue;
    if (g_lx_hk[i].cb) g_lx_hk[i].cb(g_lx_hk[i].handle, g_lx_hk[i].ctx);
    return GDK_FILTER_CONTINUE;
  }
  return GDK_FILTER_CONTINUE;
}

static void nv_lx_hk_ensure_filter(void) {
  if (g_lx_filter_installed) return;
  GdkWindow* root = gdk_get_default_root_window();
  if (!root) return;
  gdk_window_add_filter(root, nv_lx_hk_filter, NULL);
  g_lx_filter_installed = 1;
}

NV_INTERNAL int nv_linux_register_hotkey(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                                        void (*cb)(long long handle, void* ctx), void* ctx) {
  (void)w;
  if (!spec || !cb) return -1;
  if (nv_lx_is_wayland_session()) return NV_HOTKEY_RC_NOT_SUPPORTED;

  GdkDisplay* gd = gdk_display_get_default();
  if (!gd || !GDK_IS_X11_DISPLAY(gd)) return NV_HOTKEY_RC_NOT_SUPPORTED;

  Display* d = GDK_DISPLAY_XDISPLAY(gd);
  if (!d) return -1;

  KeySym ks = nv_lx_keysym(spec);
  if (ks == NoSymbol) return -1;
  unsigned mods = nv_lx_mod(spec);
  if (mods == 0) return -1;

  KeyCode kc = XKeysymToKeycode(d, ks);
  if (kc == 0) return -1;

  int slot = -1;
  for (int i = 0; i < NV_LX_HK_MAX; i++) {
    if (!g_lx_hk[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return -1;

  Window root = DefaultRootWindow(d);
  int grab_st = (int)XGrabKey(d, kc, mods, root, True, GrabModeAsync, GrabModeAsync);
  if (grab_st != GrabSuccess) {
    return -1;
  }

  nv_lx_hk_ensure_filter();

  g_lx_hk[slot].used = 1;
  g_lx_hk[slot].handle = handle;
  g_lx_hk[slot].x_mods = mods;
  g_lx_hk[slot].keycode = (unsigned)kc;
  g_lx_hk[slot].cb = cb;
  g_lx_hk[slot].ctx = ctx;
  return 0;
}

NV_INTERNAL void nv_linux_unregister_hotkey(long long handle) {
  GdkDisplay* gd = gdk_display_get_default();
  if (!gd || !GDK_IS_X11_DISPLAY(gd)) return;
  Display* d = GDK_DISPLAY_XDISPLAY(gd);
  if (!d) return;
  Window root = DefaultRootWindow(d);

  for (int i = 0; i < NV_LX_HK_MAX; i++) {
    if (!g_lx_hk[i].used || g_lx_hk[i].handle != handle) continue;
    XUngrabKey(d, (int)g_lx_hk[i].keycode, g_lx_hk[i].x_mods, root);
    g_lx_hk[i].used = 0;
    g_lx_hk[i].cb = NULL;
    g_lx_hk[i].ctx = NULL;
    return;
  }
}

#else /* !GDK_WINDOWING_X11 */

NV_INTERNAL int nv_linux_register_hotkey(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                                        void (*cb)(long long handle, void* ctx), void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  return NV_HOTKEY_RC_NOT_SUPPORTED;
}

NV_INTERNAL void nv_linux_unregister_hotkey(long long handle) { (void)handle; }

#endif /* GDK_WINDOWING_X11 */
