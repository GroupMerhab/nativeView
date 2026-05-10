/* =============================================================================
 * nv_menu.h — Application menu bar (macOS / Linux window menu)
 *
 * Apache 2.0 - See LICENSE for details
 * =============================================================================
 */

#ifndef NV_MENU_H
#define NV_MENU_H

#include "nv_util.h"
#include "nv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nv_menu_item {
  const char* id;
  const char* label;
  const char* shortcut; /* e.g. "CmdOrCtrl+S" */
  int enabled;
  int separator; /* if 1, other fields ignored */
  struct nv_menu_item* children;
  int child_count;
} nv_menu_item_t;

/**
 * Set the application menu from a tree of items (macOS: NSApp main menu;
 * Linux: GtkMenuBar on each window belonging to \p app).
 * NULL-safe on \p app (no-op). \p items may be NULL when \p count is 0.
 */
NV_API void nv_app_set_menu(nv_app_t* app, const nv_menu_item_t* items, int count);

#ifdef __cplusplus
}
#endif

#endif /* NV_MENU_H */
