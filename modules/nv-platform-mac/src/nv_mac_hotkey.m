/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#import <Carbon/Carbon.h>
#import <dispatch/dispatch.h>

#include "nv_window_internal.h"

typedef struct {
  int used;
  long long handle;
  EventHotKeyRef ref;
  void (*cb)(long long handle, void* ctx);
  void* ctx;
} nv_mac_hk_slot_t;

#define NV_MAC_HK_MAX 32
static nv_mac_hk_slot_t g_mac_hk[NV_MAC_HK_MAX];
static EventHandlerRef g_mac_hk_handler = NULL;

static UInt32 nv_mac_spec_mod(const nv_hotkey_combo_t* s) {
  UInt32 m = 0;
  if (!s) return 0;
  if (s->mod_flags & NV_HOTKEY_MOD_SHIFT) m |= (UInt32)shiftKey;
  if (s->mod_flags & NV_HOTKEY_MOD_ALT) m |= (UInt32)optionKey;
  if (s->mod_flags & NV_HOTKEY_MOD_CTRL) m |= (UInt32)controlKey;
  if (s->mod_flags & NV_HOTKEY_MOD_CMD_OR_CTRL) m |= (UInt32)cmdKey;
  else if (s->mod_flags & NV_HOTKEY_MOD_META) m |= (UInt32)cmdKey;
  return m;
}

static UInt32 nv_mac_spec_vk(const nv_hotkey_combo_t* spec) {
  if (!spec) return 0xFF;
  if (spec->is_fn) {
    static const UInt32 fk[12] = {kVK_F1,  kVK_F2,  kVK_F3,  kVK_F4,  kVK_F5,  kVK_F6,
                                  kVK_F7,  kVK_F8,  kVK_F9,  kVK_F10, kVK_F11, kVK_F12};
    if (spec->fn_index < 1 || spec->fn_index > 12) return 0xFF;
    return fk[spec->fn_index - 1];
  }
  switch (spec->special) {
    case NV_HOTKEY_SPECIAL_SPACE:
      return (UInt32)kVK_Space;
    case NV_HOTKEY_SPECIAL_TAB:
      return (UInt32)kVK_Tab;
    case NV_HOTKEY_SPECIAL_RETURN:
      return (UInt32)kVK_Return;
    case NV_HOTKEY_SPECIAL_ESCAPE:
      return (UInt32)kVK_Escape;
    default:
      break;
  }
  if (spec->key_char >= '0' && spec->key_char <= '9') {
    switch (spec->key_char) {
      case '0':
        return (UInt32)kVK_ANSI_0;
      case '1':
        return (UInt32)kVK_ANSI_1;
      case '2':
        return (UInt32)kVK_ANSI_2;
      case '3':
        return (UInt32)kVK_ANSI_3;
      case '4':
        return (UInt32)kVK_ANSI_4;
      case '5':
        return (UInt32)kVK_ANSI_5;
      case '6':
        return (UInt32)kVK_ANSI_6;
      case '7':
        return (UInt32)kVK_ANSI_7;
      case '8':
        return (UInt32)kVK_ANSI_8;
      case '9':
        return (UInt32)kVK_ANSI_9;
      default:
        return 0xFF;
    }
  }
  switch (spec->key_char) {
    case 'a':
      return (UInt32)kVK_ANSI_A;
    case 'b':
      return (UInt32)kVK_ANSI_B;
    case 'c':
      return (UInt32)kVK_ANSI_C;
    case 'd':
      return (UInt32)kVK_ANSI_D;
    case 'e':
      return (UInt32)kVK_ANSI_E;
    case 'f':
      return (UInt32)kVK_ANSI_F;
    case 'g':
      return (UInt32)kVK_ANSI_G;
    case 'h':
      return (UInt32)kVK_ANSI_H;
    case 'i':
      return (UInt32)kVK_ANSI_I;
    case 'j':
      return (UInt32)kVK_ANSI_J;
    case 'k':
      return (UInt32)kVK_ANSI_K;
    case 'l':
      return (UInt32)kVK_ANSI_L;
    case 'm':
      return (UInt32)kVK_ANSI_M;
    case 'n':
      return (UInt32)kVK_ANSI_N;
    case 'o':
      return (UInt32)kVK_ANSI_O;
    case 'p':
      return (UInt32)kVK_ANSI_P;
    case 'q':
      return (UInt32)kVK_ANSI_Q;
    case 'r':
      return (UInt32)kVK_ANSI_R;
    case 's':
      return (UInt32)kVK_ANSI_S;
    case 't':
      return (UInt32)kVK_ANSI_T;
    case 'u':
      return (UInt32)kVK_ANSI_U;
    case 'v':
      return (UInt32)kVK_ANSI_V;
    case 'w':
      return (UInt32)kVK_ANSI_W;
    case 'x':
      return (UInt32)kVK_ANSI_X;
    case 'y':
      return (UInt32)kVK_ANSI_Y;
    case 'z':
      return (UInt32)kVK_ANSI_Z;
    default:
      return 0xFF;
  }
}

static pascal OSStatus nv_mac_hotkey_handler(EventHandlerCallRef nextHandler, EventRef event,
                                             void* userData) {
  (void)userData;
  if (GetEventClass(event) != kEventClassKeyboard) {
    return CallNextEventHandler(nextHandler, event);
  }
  if (GetEventKind(event) != kEventHotKeyPressed) {
    return CallNextEventHandler(nextHandler, event);
  }
  EventHotKeyID hid;
  OSStatus st = GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hid),
                                    NULL, &hid);
  if (st != noErr) {
    return CallNextEventHandler(nextHandler, event);
  }
  UInt32 hid32 = hid.id;
  for (int i = 0; i < NV_MAC_HK_MAX; i++) {
    if (!g_mac_hk[i].used) continue;
    if ((UInt32)g_mac_hk[i].handle != hid32) continue;
    if (g_mac_hk[i].cb) g_mac_hk[i].cb(g_mac_hk[i].handle, g_mac_hk[i].ctx);
    return noErr;
  }
  return CallNextEventHandler(nextHandler, event);
}

static void nv_mac_hotkey_install_handler_once(void) {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    EventTypeSpec spec;
    spec.eventClass = kEventClassKeyboard;
    spec.eventKind = kEventHotKeyPressed;
    InstallApplicationEventHandler(NewEventHandlerUPP(nv_mac_hotkey_handler), 1, &spec, NULL,
                                   &g_mac_hk_handler);
  });
}

NV_INTERNAL int nv_mac_register_hotkey(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                                        void (*cb)(long long handle, void* ctx), void* ctx) {
  (void)w;
  if (!spec || !cb) return -1;
  UInt32 vk = nv_mac_spec_vk(spec);
  UInt32 mod = nv_mac_spec_mod(spec);
  if (vk == 0xFF || mod == 0) return -1;

  int slot = -1;
  for (int i = 0; i < NV_MAC_HK_MAX; i++) {
    if (!g_mac_hk[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return -1;

  nv_mac_hotkey_install_handler_once();

  EventHotKeyID hid;
  hid.signature = 'NVHK';
  hid.id = (UInt32)handle;

  EventHotKeyRef ref = NULL;
  OSStatus rs = RegisterEventHotKey(vk, mod, hid, GetApplicationEventTarget(), 0, &ref);
  if (rs != noErr || !ref) {
    return -1;
  }

  g_mac_hk[slot].used = 1;
  g_mac_hk[slot].handle = handle;
  g_mac_hk[slot].ref = ref;
  g_mac_hk[slot].cb = cb;
  g_mac_hk[slot].ctx = ctx;
  return 0;
}

NV_INTERNAL void nv_mac_unregister_hotkey(long long handle) {
  for (int i = 0; i < NV_MAC_HK_MAX; i++) {
    if (!g_mac_hk[i].used || g_mac_hk[i].handle != handle) continue;
    if (g_mac_hk[i].ref) {
      UnregisterEventHotKey(g_mac_hk[i].ref);
    }
    g_mac_hk[i].used = 0;
    g_mac_hk[i].ref = NULL;
    g_mac_hk[i].cb = NULL;
    g_mac_hk[i].ctx = NULL;
    return;
  }
}
