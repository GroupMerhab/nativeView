/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_window_context_menu.c — weak platform stubs for window.setContextMenu
 *
 * Each platform backend provides a strong definition. Tests / partial links
 * resolve these weak fallbacks when a backend is absent (GNU) or MSVC stubs
 * for non-mac / non-linux when building nv-runtime alone on Windows.
 * =============================================================================
 */

#include "nv_window_internal.h"
#include "nv_menu.h"
#include <stddef.h>

#if defined(_MSC_VER) && defined(_WIN32)
void nv_mac_window_set_context_menu(nv_window_t* w, const nv_menu_item_t* items, int count) {
  (void)w;
  (void)items;
  (void)count;
}
void nv_linux_window_set_context_menu(nv_window_t* w, const nv_menu_item_t* items, int count) {
  (void)w;
  (void)items;
  (void)count;
}
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) void nv_mac_window_set_context_menu(nv_window_t* w,
                                                            const nv_menu_item_t* items,
                                                            int count) {
  (void)w;
  (void)items;
  (void)count;
}
__attribute__((weak)) void nv_win_window_set_context_menu(nv_window_t* w,
                                                           const nv_menu_item_t* items,
                                                           int count) {
  (void)w;
  (void)items;
  (void)count;
}
__attribute__((weak)) void nv_linux_window_set_context_menu(nv_window_t* w,
                                                              const nv_menu_item_t* items,
                                                              int count) {
  (void)w;
  (void)items;
  (void)count;
}
#else
#error "nv_window_context_menu.c: add stubs for this toolchain"
#endif
