/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_hotkey_stubs.c — weak / MSVC stubs for global hotkey platform hooks
 * =============================================================================
 */

#include "nv_window_internal.h"
#include <stddef.h>

#if defined(_MSC_VER)
#pragma comment(linker, "/alternatename:nv_mac_register_hotkey=nv_mac_register_hotkey_stub")
#pragma comment(linker, "/alternatename:nv_mac_unregister_hotkey=nv_mac_unregister_hotkey_stub")
#pragma comment(linker, "/alternatename:nv_win_register_hotkey=nv_win_register_hotkey_stub")
#pragma comment(linker, "/alternatename:nv_win_unregister_hotkey=nv_win_unregister_hotkey_stub")
#pragma comment(linker, "/alternatename:nv_linux_register_hotkey=nv_linux_register_hotkey_stub")
#pragma comment(linker, "/alternatename:nv_linux_unregister_hotkey=nv_linux_unregister_hotkey_stub")

int nv_mac_register_hotkey_stub(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                                void (*cb)(long long handle, void* ctx), void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  return -1;
}
void nv_mac_unregister_hotkey_stub(long long handle) { (void)handle; }

int nv_win_register_hotkey_stub(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                                void (*cb)(long long handle, void* ctx), void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  return -1;
}
void nv_win_unregister_hotkey_stub(long long handle) { (void)handle; }

int nv_linux_register_hotkey_stub(nv_window_t* w, long long handle, const nv_hotkey_combo_t* spec,
                                  void (*cb)(long long handle, void* ctx), void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  return -1;
}
void nv_linux_unregister_hotkey_stub(long long handle) { (void)handle; }

#elif defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) int nv_mac_register_hotkey(nv_window_t* w, long long handle,
                                                 const nv_hotkey_combo_t* spec,
                                                 void (*cb)(long long handle, void* ctx),
                                                 void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  return -1;
}
__attribute__((weak)) void nv_mac_unregister_hotkey(long long handle) { (void)handle; }

__attribute__((weak)) int nv_win_register_hotkey(nv_window_t* w, long long handle,
                                                  const nv_hotkey_combo_t* spec,
                                                  void (*cb)(long long handle, void* ctx),
                                                  void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  return -1;
}
__attribute__((weak)) void nv_win_unregister_hotkey(long long handle) { (void)handle; }

__attribute__((weak)) int nv_linux_register_hotkey(nv_window_t* w, long long handle,
                                                    const nv_hotkey_combo_t* spec,
                                                    void (*cb)(long long handle, void* ctx),
                                                    void* ctx) {
  (void)w;
  (void)handle;
  (void)spec;
  (void)cb;
  (void)ctx;
  return -1;
}
__attribute__((weak)) void nv_linux_unregister_hotkey(long long handle) { (void)handle; }

#else
#error "nv_hotkey_stubs.c: add stubs for this toolchain"
#endif
