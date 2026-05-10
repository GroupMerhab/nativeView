/* =============================================================================
 * nv_menu.c — nv_app_set_menu (MSVC stubs / GNU weak fallbacks for app menu)
 * =============================================================================
 */

#include "nv_menu.h"
#include "nv_window_internal.h"
#include "nv_window_manager.h"
#include <stddef.h>

#if defined(_MSC_VER)
void nv_mac_app_set_menu(const nv_menu_item_t* items, int count) {
  (void)items;
  (void)count;
}

void nv_linux_app_set_menu(nv_window_t* w, const nv_menu_item_t* items, int count) {
  (void)w;
  (void)items;
  (void)count;
}
#elif defined(__GNUC__)
__attribute__((weak)) void nv_mac_app_set_menu(const nv_menu_item_t* items, int count) {
  (void)items;
  (void)count;
}

__attribute__((weak)) void nv_linux_app_set_menu(nv_window_t* w, const nv_menu_item_t* items,
                                                 int count) {
  (void)w;
  (void)items;
  (void)count;
}
#else
#error "nv_menu.c: add nv_mac_app_set_menu / nv_linux_app_set_menu stubs for this toolchain"
#endif

NV_API void nv_app_set_menu(nv_app_t* app, const nv_menu_item_t* items, int count) {
  if (!app) {
    return;
  }
  if (count < 0) {
    return;
  }
  nv_mac_app_set_menu(items, count);
  nv_wm_entry_t entries[NV_MAX_WINDOWS];
  size_t n = NV_MAX_WINDOWS;
  if (nv_wm_list(entries, &n) != 0) {
    return;
  }
  for (size_t i = 0; i < n; i++) {
    nv_window_t* w = entries[i].window;
    if (w && w->app == app) {
      nv_linux_app_set_menu(w, items, count);
    }
  }
}
