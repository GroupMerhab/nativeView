/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Linux platform implementation using GTK + WebKitGTK.
 * System tray: nv_linux_tray.c (nv_linux_tray_*), same static library. */

#if defined(__linux__) && !defined(__APPLE__)

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gio/gio.h>
#ifdef NV_HAS_LIBNOTIFY
#include <libnotify/notify.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "nv_op_windows.h"
#include "nv_op_dialog.h"
#include <webkit2/webkit2.h>
#include <jsc/jsc.h>

#include "nv_core_internal.h"
#include "nv_menu.h"
#include "nv_window_internal.h"
#include "nv_ipc_internal.h"
#include "nv_arena.h"
#include "nv_base64.h"
#include "nv_util.h"
#include "nv_window_manager.h"
#include "nv.h"

/* =============================================================================
 * App-level Platform State
 * =============================================================================
 */

typedef struct nv_linux_app_platform {
  WebKitWebContext *context;
  WebKitSettings   *settings;
} nv_linux_app_platform_t;

/* =============================================================================
 * Window-level Platform State
 * =============================================================================
 */

typedef struct nv_linux_window_platform {
  GtkWidget                *window;
  WebKitWebView            *webview;
  WebKitUserContentManager *ucm;
  nv_window_t              *c_window;
  GtkWidget                *root_vbox;
  GtkAccelGroup            *accel_group;
  GtkWidget                *menubar;
  GSimpleActionGroup       *ctx_menu_actions;
} nv_linux_window_platform_t;

/* =============================================================================
 * Helpers
 * =============================================================================
 */

static gboolean nv_linux_env_truthy(const gchar *value) {
  if (!value || !*value) return FALSE;
  if (g_ascii_strcasecmp(value, "1") == 0) return TRUE;
  if (g_ascii_strcasecmp(value, "true") == 0) return TRUE;
  if (g_ascii_strcasecmp(value, "yes") == 0) return TRUE;
  if (g_ascii_strcasecmp(value, "on") == 0) return TRUE;
  return FALSE;
}

static gboolean nv_linux_webview_debug_enabled(void) {
  const gchar *value = g_getenv("NV_WEBVIEW_DEBUG");
  if (value && *value) return nv_linux_env_truthy(value);
  value = g_getenv("NV_DEBUG_WEBVIEW");
  if (value && *value) return nv_linux_env_truthy(value);
#ifdef NV_DEBUG
  return TRUE;
#else
  return FALSE;
#endif
}

static gboolean nv_linux_context_menu_enabled(void) {
  const gchar *value = g_getenv("NV_WEBVIEW_CONTEXT_MENU");
  if (value && *value) return nv_linux_env_truthy(value);
  return FALSE;
}

static nv_linux_app_platform_t *nv_linux_get_app_platform(nv_window_t *window) {
  if (!window || !window->app) return NULL;
  return (nv_linux_app_platform_t *)nv_app_get_platform(window->app);
}

static nv_linux_window_platform_t *nv_linux_get_window_platform(
    nv_window_t *window) {
  if (!window) return NULL;
  return (nv_linux_window_platform_t *)nv_window_get_platform(window);
}

/* Notify ready when load finishes */
static void nv_linux_on_load_changed(WebKitWebView *webview,
                                     WebKitLoadEvent load_event,
                                     gpointer user_data) {
  (void)webview;
  nv_window_t *window = (nv_window_t *)user_data;
  if (!window) return;
  if (load_event == WEBKIT_LOAD_FINISHED) {
    nv_ipc_state_t *ipc = nv_window_get_ipc(window);
    if (ipc) {
      nv_ipc_invoke_ready(window, ipc);
    }
  }
}

static void nv_linux_send_context_menu_action_id(nv_window_t *win, const char *id) {
  GString *gs = g_string_new("{\"id\":\"");
  const unsigned char *p = (const unsigned char *)(id ? id : "");
  for (; *p; p++) {
    if (*p == '\\' || *p == '"') {
      g_string_append_c(gs, '\\');
    }
    if (*p < 0x20) {
      continue;
    }
    g_string_append_c(gs, (char)*p);
  }
  g_string_append(gs, "\"}");
  nv_send(win, "window.contextMenuAction", gs->str);
  g_string_free(gs, TRUE);
}

static void nv_linux_ctx_menu_item_activated(GSimpleAction *action, GVariant *param, gpointer user_data) {
  (void)param;
  (void)user_data;
  nv_window_t *w = (nv_window_t *)g_object_get_data(G_OBJECT(action), "nv-win");
  const char *mid = (const char *)g_object_get_data(G_OBJECT(action), "nv-ctx-id");
  if (!w) return;
  nv_linux_send_context_menu_action_id(w, mid ? mid : "");
}

static gboolean nv_linux_on_context_menu(WebKitWebView *webview,
                                        WebKitContextMenu *menu,
                                        GdkEvent *event,
                                        WebKitHitTestResult *hit_test_result,
                                        gpointer user_data) {
  (void)event;
  (void)hit_test_result;
  nv_window_t *window = (nv_window_t *)user_data;
  if (window && window->ctx_menu_count > 0 && window->ctx_menu_items) {
    nv_linux_window_platform_t *p = nv_linux_get_window_platform(window);
    webkit_context_menu_remove_all(menu);
    for (int i = 0; i < window->ctx_menu_count; i++) {
      const nv_menu_item_t *it = &window->ctx_menu_items[i];
      if (it->separator) {
        webkit_context_menu_append(menu, webkit_context_menu_item_new_separator());
        continue;
      }
      if (!p || !p->ctx_menu_actions) continue;
      gchar *an = g_strdup_printf("ctx%d", i);
      GAction *ga = g_action_map_lookup_action(G_ACTION_MAP(p->ctx_menu_actions), an);
      g_free(an);
      if (!ga) continue;
      WebKitContextMenuItem *wmi = webkit_context_menu_item_new_from_gaction(
          ga, it->label ? it->label : "", NULL);
      if (wmi) {
        webkit_context_menu_append(menu, wmi);
        g_object_unref(wmi);
      }
    }
    return FALSE;
  }
  (void)webview;
  return nv_linux_context_menu_enabled() ? FALSE : TRUE;
}

/* Edit menu: Cut/Copy/Paste/Select All - forward to WebView so Ctrl+C/X/V/A work */
static void nv_linux_on_edit_cut(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data) {
  WebKitWebView *w = (WebKitWebView *)user_data;
  if (w) webkit_web_view_execute_editing_command(w, WEBKIT_EDITING_COMMAND_CUT);
}
static void nv_linux_on_edit_copy(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data) {
  WebKitWebView *w = (WebKitWebView *)user_data;
  if (w) webkit_web_view_execute_editing_command(w, WEBKIT_EDITING_COMMAND_COPY);
}
static void nv_linux_on_edit_paste(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data) {
  WebKitWebView *w = (WebKitWebView *)user_data;
  if (w) webkit_web_view_execute_editing_command(w, WEBKIT_EDITING_COMMAND_PASTE);
}
static void nv_linux_on_edit_select_all(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data) {
  WebKitWebView *w = (WebKitWebView *)user_data;
  if (w) webkit_web_view_execute_editing_command(w, WEBKIT_EDITING_COMMAND_SELECT_ALL);
}

static void nv_linux_on_debug_devtools(G_GNUC_UNUSED GtkMenuItem *item, gpointer user_data) {
  WebKitWebView *w = (WebKitWebView *)user_data;
  if (!w) return;
  WebKitWebInspector *inspector = webkit_web_view_get_inspector(w);
  if (!inspector) return;
  webkit_web_inspector_show(inspector);
}

typedef struct nv_linux_app_menu_ud {
  nv_window_t     *win;
  WebKitWebView   *wv;
  char            *id;
} nv_linux_app_menu_ud;

static void nv_linux_app_menu_ud_destroy(gpointer data) {
  nv_linux_app_menu_ud *ud = (nv_linux_app_menu_ud *)data;
  if (!ud) return;
  g_free(ud->id);
  g_free(ud);
}

static void nv_linux_send_menu_json_id(nv_window_t *win, const char *id) {
  GString *gs = g_string_new("{\"id\":\"");
  const unsigned char *p = (const unsigned char *)(id ? id : "");
  for (; *p; p++) {
    if (*p == '\\' || *p == '"') {
      g_string_append_c(gs, '\\');
    }
    if (*p < 0x20) {
      continue;
    }
    g_string_append_c(gs, (char)*p);
  }
  g_string_append(gs, "\"}");
  nv_send(win, "app.menuAction", gs->str);
  g_string_free(gs, TRUE);
}

static void nv_linux_app_menu_activate(GtkMenuItem *item, gpointer user_data) {
  (void)item;
  nv_linux_app_menu_ud *ud = (nv_linux_app_menu_ud *)user_data;
  if (!ud || !ud->win) return;
  const char *nid = ud->id ? ud->id : "";
  if (strcmp(nid, "edit.cut") == 0) {
    nv_linux_on_edit_cut(NULL, ud->wv);
    return;
  }
  if (strcmp(nid, "edit.copy") == 0) {
    nv_linux_on_edit_copy(NULL, ud->wv);
    return;
  }
  if (strcmp(nid, "edit.paste") == 0) {
    nv_linux_on_edit_paste(NULL, ud->wv);
    return;
  }
  if (strcmp(nid, "edit.selectAll") == 0) {
    nv_linux_on_edit_select_all(NULL, ud->wv);
    return;
  }
  if (strcmp(nid, "debug.devtools") == 0) {
    nv_linux_on_debug_devtools(NULL, ud->wv);
    return;
  }
  nv_linux_send_menu_json_id(ud->win, nid);
}

static void nv_linux_parse_shortcut(const char *shortcut, GdkModifierType *mods, guint *key) {
  *mods = 0;
  *key = 0;
  if (!shortcut || !shortcut[0]) return;
  char buf[160];
  size_t n = strlen(shortcut);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  memcpy(buf, shortcut, n);
  buf[n] = '\0';
  char *save = NULL;
  char *tok = strtok_r(buf, "+", &save);
  char last[64];
  last[0] = '\0';
  while (tok) {
    char *next = strtok_r(NULL, "+", &save);
    if (!next) {
      g_strlcpy(last, tok, sizeof(last));
      break;
    }
    char low[48];
    g_strlcpy(low, tok, sizeof(low));
    g_ascii_strdown(low, (gssize)strlen(low));
    if (strcmp(low, "cmdorctrl") == 0 || strcmp(low, "commandorcontrol") == 0) {
      *mods |= GDK_CONTROL_MASK;
    } else if (strcmp(low, "ctrl") == 0 || strcmp(low, "control") == 0) {
      *mods |= GDK_CONTROL_MASK;
    } else if (strcmp(low, "cmd") == 0 || strcmp(low, "command") == 0) {
      *mods |= GDK_META_MASK;
    } else if (strcmp(low, "shift") == 0) {
      *mods |= GDK_SHIFT_MASK;
    } else if (strcmp(low, "alt") == 0 || strcmp(low, "option") == 0) {
      *mods |= GDK_MOD1_MASK;
    }
    tok = next;
  }
  if (last[0] == '\0') return;
  if (strlen(last) == 1) {
    *key = (guint)gdk_unicode_to_keyval((guint32)g_ascii_tolower((int)last[0]));
    return;
  }
  char nm[64];
  g_strlcpy(nm, last, sizeof(nm));
  *key = gdk_keyval_from_name(nm);
  if (*key == GDK_KEY_VoidSymbol) {
    g_ascii_strup(nm, (gssize)strlen(nm));
    *key = gdk_keyval_from_name(nm);
  }
}

static void nv_linux_append_menu_items(GtkMenuShell *shell, const nv_menu_item_t *items, int count,
                                       nv_linux_window_platform_t *plat) {
  if (!shell || !plat) return;
  WebKitWebView *wv = plat->webview;
  for (int i = 0; i < count; i++) {
    const nv_menu_item_t *it = &items[i];
    if (it->separator) {
      GtkWidget *sep = gtk_separator_menu_item_new();
      gtk_menu_shell_append(shell, sep);
      continue;
    }
    if (it->child_count > 0) {
      const char *lbl = (it->label && it->label[0]) ? it->label : "_";
      GtkWidget *mi = gtk_menu_item_new_with_mnemonic(lbl);
      GtkWidget *sub = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi), sub);
      nv_linux_append_menu_items(GTK_MENU_SHELL(sub), it->children, it->child_count, plat);
      gtk_menu_shell_append(shell, mi);
      continue;
    }
    const char *lbl2 = (it->label && it->label[0]) ? it->label : "_";
    GtkWidget *mi = gtk_menu_item_new_with_mnemonic(lbl2);
    gtk_widget_set_sensitive(mi, it->enabled ? TRUE : FALSE);
    nv_linux_app_menu_ud *ud = g_new0(nv_linux_app_menu_ud, 1);
    ud->win = plat->c_window;
    ud->wv = wv;
    ud->id = g_strdup(it->id ? it->id : "");
    g_signal_connect_data(mi, "activate", G_CALLBACK(nv_linux_app_menu_activate), ud,
                          (GClosureNotify)nv_linux_app_menu_ud_destroy, (GConnectFlags)0);
    GdkModifierType md = 0;
    guint ky = 0;
    nv_linux_parse_shortcut(it->shortcut, &md, &ky);
    if (plat->accel_group && ky != 0) {
      gtk_widget_add_accelerator(mi, "activate", plat->accel_group, (guint)ky, md, GTK_ACCEL_VISIBLE);
    }
    if (it->id && strcmp(it->id, "debug.devtools") == 0 && plat->accel_group) {
      gtk_widget_add_accelerator(mi, "activate", plat->accel_group, GDK_KEY_I,
                                 (GdkModifierType)(GDK_CONTROL_MASK | GDK_SHIFT_MASK), GTK_ACCEL_VISIBLE);
    }
    gtk_menu_shell_append(shell, mi);
  }
}

static void nv_linux_fill_default_menu(nv_menu_item_t *edit_children, nv_menu_item_t *dbg_children,
                                       nv_menu_item_t *roots, int *nroots) {
  edit_children[0] =
      (nv_menu_item_t){"edit.cut", "Cu_t", "CmdOrCtrl+X", 1, 0, NULL, 0};
  edit_children[1] =
      (nv_menu_item_t){"edit.copy", "_Copy", "CmdOrCtrl+C", 1, 0, NULL, 0};
  edit_children[2] =
      (nv_menu_item_t){"edit.paste", "_Paste", "CmdOrCtrl+V", 1, 0, NULL, 0};
  edit_children[3] = (nv_menu_item_t){"edit.selectAll", "Select _All", "CmdOrCtrl+A", 1, 0, NULL, 0};
  roots[0] = (nv_menu_item_t){"edit", "_Edit", NULL, 1, 0, edit_children, 4};
  int n = 1;
  if (nv_linux_webview_debug_enabled()) {
    dbg_children[0] = (nv_menu_item_t){"debug.devtools", "_DevTools", "F12", 1, 0, NULL, 0};
    roots[n++] = (nv_menu_item_t){"debug", "_Debug", NULL, 1, 0, dbg_children, 1};
  }
  *nroots = n;
}

/* JS bridge: script-message-received::nvBridge */
NV_INTERNAL static void nv_linux_on_script_message(
    WebKitUserContentManager *manager,
    WebKitJavascriptResult   *result,
    gpointer                  user_data) {
  (void)manager;

  nv_window_t *window = (nv_window_t *)user_data;
  if (!window) return;

  nv_ipc_state_t *ipc = nv_window_get_ipc(window);
  if (!ipc) {
    NV_ERR("Linux: no IPC state for window");
    return;
  }

  gchar *json = NULL;
#if WEBKIT_CHECK_VERSION(2, 38, 0)
  JSCValue *js_value = webkit_javascript_result_get_js_value(result);
  if (js_value) {
    json = jsc_value_to_string(js_value);
  }
#else
  json = webkit_javascript_result_to_string(result);
#endif
  if (!json) {
    NV_ERR("Linux: failed to convert JS message to string");
    return;
  }

  nv_ipc_dispatch(window, ipc, json);
  g_free(json);
}

#ifdef NV_DEBUG
NV_INTERNAL static gboolean nv_linux_on_console_message(
    WebKitWebView       *webview,
    gpointer             message,
    gpointer             user_data) {
  (void)webview;
  (void)message;
  (void)user_data;
  NV_ERR("WebKit console message");
  return FALSE; /* allow default handling as well */
}
#endif

/* Forward declarations */
static void nv_linux_notifications_shutdown(void);
static gboolean nv_linux_on_window_delete(GtkWidget *widget,
                                          GdkEvent  *event,
                                          gpointer    user_data);
#ifdef NV_DEBUG
static gboolean nv_linux_on_console_message(WebKitWebView* webview,
                                            gpointer message,
                                            gpointer user_data);
#endif

/* =============================================================================
 * App Platform Hooks
 * =============================================================================
 */

NV_INTERNAL int nv_linux_shell_open_url(const char *url);
NV_INTERNAL int nv_linux_shell_open_path(const char *path);
NV_INTERNAL int nv_linux_shell_show_in_folder(const char *path);
NV_INTERNAL nv_shell_result_t* nv_linux_shell_exec(const char *cmd);
NV_INTERNAL char* nv_linux_clipboard_read_text(void);
NV_INTERNAL int nv_linux_clipboard_write_text(const char* utf8);
NV_INTERNAL void nv_linux_clipboard_clear(void);
NV_INTERNAL int nv_linux_clipboard_has_text(void);
NV_INTERNAL char* nv_linux_clipboard_read_image(int* out_w, int* out_h);
NV_INTERNAL int nv_linux_clipboard_write_image(const char* base64_png);
NV_INTERNAL int nv_linux_clipboard_has_image(void);
NV_INTERNAL int nv_linux_notification_show(long long id, const char* title, const char* body,
                                          const char* icon_path, nv_window_t* w);
NV_INTERNAL int nv_linux_notification_close(long long id);
NV_INTERNAL char* nv_linux_screen_get_all(void);
NV_INTERNAL char* nv_linux_screen_get_primary(void);
NV_INTERNAL char* nv_linux_screen_get_cursor(void);
NV_INTERNAL int nv_linux_tray_create(long long id, const char* icon_path, const char* tooltip, nv_window_t* w);
NV_INTERNAL int nv_linux_tray_destroy(long long id);
NV_INTERNAL int nv_linux_tray_set_icon(long long id, const char* icon_path);
NV_INTERNAL int nv_linux_tray_set_tooltip(long long id, const char* tooltip);
NV_INTERNAL int nv_linux_tray_set_menu(long long id, const char** labels, const long long* item_ids, int count);
NV_INTERNAL int nv_linux_fs_watch_start(long long id, const char* path, nv_window_t* w);
NV_INTERNAL void nv_linux_fs_watch_stop(long long id);
NV_INTERNAL void nv_linux_fs_watch_detach_for_window(nv_window_t* w);
NV_INTERNAL void nv_linux_dialog_open_file_async(int allow_multiple, nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback);
NV_INTERNAL void nv_linux_dialog_save_file_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback);
NV_INTERNAL void nv_linux_dialog_open_folder_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t callback);
NV_INTERNAL void nv_linux_dialog_message_async(const char* title, const char* body, const char* type,
                                               const char** buttons, size_t btn_count, nv_dialog_ctx_t* ctx,
                                               nv_dialog_cb_t callback);
NV_INTERNAL void nv_linux_dialog_confirm_async(const char* title, const char* body, nv_dialog_ctx_t* ctx,
                                               nv_dialog_cb_t callback);
NV_INTERNAL char* nv_linux_get_data_dir(void);
NV_INTERNAL char* nv_linux_get_exe_path(void);
NV_INTERNAL char* nv_linux_get_resource_dir(void);
NV_INTERNAL char* nv_linux_get_locale(void);
NV_INTERNAL void nv_linux_app_set_menu(nv_window_t* w, const nv_menu_item_t* items, int count);

NV_INTERNAL void nv_app_platform_init(nv_app_t *app) {
  if (!app) return;

  app->platform_api.platform_name = "linux";
  app->platform_api.shell_open_url = nv_linux_shell_open_url;
  app->platform_api.shell_open_path = nv_linux_shell_open_path;
  app->platform_api.shell_show_in_folder = nv_linux_shell_show_in_folder;
  app->platform_api.shell_exec = nv_linux_shell_exec;
  app->platform_api.clipboard_read_text = nv_linux_clipboard_read_text;
  app->platform_api.clipboard_write_text = nv_linux_clipboard_write_text;
  app->platform_api.clipboard_clear = nv_linux_clipboard_clear;
  app->platform_api.clipboard_has_text = nv_linux_clipboard_has_text;
  app->platform_api.clipboard_read_image = nv_linux_clipboard_read_image;
  app->platform_api.clipboard_write_image = nv_linux_clipboard_write_image;
  app->platform_api.clipboard_has_image = nv_linux_clipboard_has_image;
  app->platform_api.notification_show = nv_linux_notification_show;
  app->platform_api.notification_close = nv_linux_notification_close;
  app->platform_api.screen_get_all = nv_linux_screen_get_all;
  app->platform_api.screen_get_primary = nv_linux_screen_get_primary;
  app->platform_api.screen_get_cursor = nv_linux_screen_get_cursor;
  app->platform_api.tray_create = nv_linux_tray_create;
  app->platform_api.tray_destroy = nv_linux_tray_destroy;
  app->platform_api.tray_set_icon = nv_linux_tray_set_icon;
  app->platform_api.tray_set_tooltip = nv_linux_tray_set_tooltip;
  app->platform_api.tray_set_menu = nv_linux_tray_set_menu;
  app->platform_api.fs_watch_start = nv_linux_fs_watch_start;
  app->platform_api.fs_watch_stop = nv_linux_fs_watch_stop;
  app->platform_api.dialog_open_file_async = nv_linux_dialog_open_file_async;
  app->platform_api.dialog_save_file_async = nv_linux_dialog_save_file_async;
  app->platform_api.dialog_open_folder_async = nv_linux_dialog_open_folder_async;
  app->platform_api.dialog_message_async = nv_linux_dialog_message_async;
  app->platform_api.dialog_confirm_async = nv_linux_dialog_confirm_async;
  app->platform_api.app_get_data_dir = nv_linux_get_data_dir;
  app->platform_api.app_get_exe_path = nv_linux_get_exe_path;
  app->platform_api.app_get_resource_dir = nv_linux_get_resource_dir;
  app->platform_api.app_get_locale = nv_linux_get_locale;
  app->platform_api.app_set_menu = nv_linux_app_set_menu;
  app->platform_api.window_create = nv_window_platform_create;
  app->platform_api.window_destroy = nv_window_platform_destroy;
  app->platform_api.window_show = nv_window_platform_show;
  app->platform_api.window_hide = nv_window_platform_hide;
  app->platform_api.window_set_modal = nv_window_platform_set_modal;
  app->platform_api.window_set_title = nv_window_platform_set_title;
  app->platform_api.window_set_size = nv_window_platform_set_size;
  app->platform_api.window_get_size = nv_window_platform_get_size;
  app->platform_api.window_set_position = nv_window_platform_set_position;
  app->platform_api.window_get_position = nv_window_platform_get_position;
  app->platform_api.window_center = nv_window_platform_center;
  app->platform_api.window_minimize = nv_window_platform_minimize;
  app->platform_api.window_maximize = nv_window_platform_maximize;
  app->platform_api.window_restore = nv_window_platform_restore;
  app->platform_api.window_fullscreen = nv_window_platform_fullscreen;
  app->platform_api.window_is_fullscreen = nv_window_platform_is_fullscreen;
  app->platform_api.window_focus = nv_window_platform_focus;
  app->platform_api.window_is_focused = nv_window_platform_is_focused;
  app->platform_api.window_set_resizable = nv_window_platform_set_resizable;
  app->platform_api.window_set_always_on_top = nv_window_platform_set_always_on_top;
  app->platform_api.window_set_opacity = nv_window_platform_set_opacity;
  app->platform_api.window_set_zoom_factor = nv_window_platform_set_zoom_factor;
  app->platform_api.window_close = nv_window_platform_close;
  app->platform_api.window_load_html = nv_window_platform_load_html;
  app->platform_api.window_load_url = nv_window_platform_load_url;
  app->platform_api.window_eval_js = nv_window_platform_eval_js;
  app->platform_api.window_set_context_menu = nv_linux_window_set_context_menu;
  app->platform_api.hotkey_register = nv_linux_register_hotkey;
  app->platform_api.hotkey_unregister = nv_linux_unregister_hotkey;

  static int s_gtk_inited = 0;
  if (!s_gtk_inited) {
    int argc = 0;
    char **argv = NULL;
    gtk_init(&argc, &argv);
    s_gtk_inited = 1;
  }

#ifdef NV_HAS_LIBNOTIFY
  if (!notify_init("nativeview")) {
    NV_ERR("Linux: notify_init failed (D-Bus session or notifications unavailable)");
  }
#endif

  nv_linux_app_platform_t *platform =
      g_new0(nv_linux_app_platform_t, 1);
  if (!platform) {
    NV_ERR("Linux: failed to allocate app platform");
    return;
  }

  platform->context = webkit_web_context_new();
  if (!platform->context) {
    NV_ERR("Linux: webkit_web_context_new failed");
    g_free(platform);
    return;
  }

  platform->settings = webkit_settings_new();
  if (!platform->settings) {
    NV_ERR("Linux: webkit_settings_new failed");
    g_object_unref(platform->context);
    g_free(platform);
    return;
  }

  /* Disable spell checking (removed in newer WebKitGTK; guard for compatibility) */
#if WEBKIT_CHECK_VERSION(2, 40, 0)
  /* spell-checking property was removed in WebKitGTK 2.40 */
#else
  webkit_settings_set_enable_spell_checking(platform->settings, FALSE);
#endif

  /* Disable media capabilities unless explicitly enabled */
  const gchar *enable_media = g_getenv("NV_ENABLE_MEDIA");
  if (!enable_media || !*enable_media) {
    /* These APIs exist in modern WebKitGTK; if building against older
       versions, they may need conditional compilation. */
    webkit_settings_set_enable_mediasource(platform->settings, FALSE);
    webkit_settings_set_enable_encrypted_media(platform->settings, FALSE);
  }

  webkit_settings_set_enable_developer_extras(platform->settings,
                                             nv_linux_webview_debug_enabled());

  nv_app_set_platform(app, platform);
}

NV_INTERNAL void nv_app_platform_run(nv_app_t *app) {
  (void)app;
  gtk_main();
}

NV_INTERNAL void nv_app_platform_quit(nv_app_t *app) {
  (void)app;
  nv_linux_notifications_shutdown();
  gtk_main_quit();
}

/* =============================================================================
 * Window Platform Hooks
 * =============================================================================
 */

NV_INTERNAL void nv_window_platform_create(nv_window_t *window) {
  if (!window) return;

  /* Internal tests create windows without an app; skip real GUI. */
  if (!window->app) {
    return;
  }

  nv_linux_app_platform_t *app_platform = nv_linux_get_app_platform(window);
  if (!app_platform || !app_platform->context || !app_platform->settings) {
    NV_ERR("Linux: app platform not initialized");
    return;
  }

  GtkWidget *gtk_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (!gtk_window) {
    NV_ERR("Linux: gtk_window_new failed");
    return;
  }

  gtk_window_set_default_size(GTK_WINDOW(gtk_window),
                              window->cfg.width, window->cfg.height);

  if (window->cfg.title) {
    gtk_window_set_title(GTK_WINDOW(gtk_window), window->cfg.title);
  } else {
    gtk_window_set_title(GTK_WINDOW(gtk_window), "App");
  }

  gtk_window_set_resizable(GTK_WINDOW(gtk_window),
                           window->cfg.resizable ? TRUE : FALSE);
  gtk_window_set_decorated(GTK_WINDOW(gtk_window),
                           window->cfg.frameless ? FALSE : TRUE);

  /* intercept close requests from the window manager */
  g_signal_connect(gtk_window, "delete-event",
                   G_CALLBACK(nv_linux_on_window_delete), window);

  /* Create WebKitWebView with shared context */
  WebKitWebView *webview =
      WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(app_platform->context));
  if (!webview) {
    NV_ERR("Linux: webkit_web_view_new_with_context failed");
    gtk_widget_destroy(gtk_window);
    return;
  }

  /* Apply shared settings */
  webkit_web_view_set_settings(webview, app_platform->settings);

  g_signal_connect(webview, "context-menu", G_CALLBACK(nv_linux_on_context_menu), window);

  WebKitUserContentManager *ucm =
      webkit_web_view_get_user_content_manager(webview);
  if (!ucm) {
    NV_ERR("Linux: webkit_web_view_get_user_content_manager failed");
    gtk_widget_destroy(gtk_window);
    g_object_unref(webview);
    return;
  }
  g_object_ref(ucm); /* store our own reference */

  /* Register JS→C bridge */
  webkit_user_content_manager_register_script_message_handler(ucm, "nvBridge");
  g_signal_connect(ucm, "script-message-received::nvBridge",
                   G_CALLBACK(nv_linux_on_script_message), window);

  /* Inject bootstrap script */
  const char *raw = nv_ipc_inject_script();
  if (raw) {
    GString *source = g_string_new(raw);
    if (source) {
      const gchar *needle = "/*{NV_POST}*/";
      const gchar *replacement =
          "window.webkit.messageHandlers.nvBridge.postMessage";
      g_string_replace(source, needle, replacement, 0);

      WebKitUserScript *script = webkit_user_script_new(
          source->str,
          WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
          WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
          NULL,
          NULL);
      if (script) {
        webkit_user_content_manager_add_script(ucm, script);
        g_object_unref(script);
      }

      g_string_free(source, TRUE);
    }
  }

#ifdef NV_DEBUG
  if (g_signal_lookup("console-message", G_OBJECT_TYPE(webview)) != 0) {
    g_signal_connect(webview, "console-message",
                     G_CALLBACK(nv_linux_on_console_message), NULL);
  }
#endif

  GtkAccelGroup *accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(gtk_window), accel_group);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(gtk_window), vbox);

  nv_linux_window_platform_t *platform = g_new0(nv_linux_window_platform_t, 1);
  if (!platform) {
    NV_ERR("Linux: failed to allocate window platform");
    gtk_widget_destroy(gtk_window);
    g_object_unref(webview);
    g_object_unref(ucm);
    return;
  }

  platform->window = gtk_window;
  platform->webview = webview;
  platform->ucm = ucm;
  platform->c_window = window;
  platform->root_vbox = vbox;
  platform->accel_group = accel_group;

  GtkWidget *menubar = gtk_menu_bar_new();
  platform->menubar = menubar;
  nv_menu_item_t edit_children[4];
  nv_menu_item_t dbg_children[1];
  nv_menu_item_t roots[2];
  int nroots = 0;
  nv_linux_fill_default_menu(edit_children, dbg_children, roots, &nroots);
  nv_linux_append_menu_items(GTK_MENU_SHELL(menubar), roots, nroots, platform);

  gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(webview), TRUE, TRUE, 0);

  nv_window_set_platform(window, platform);

  /* Observe load finished for ready callback */
  g_signal_connect(webview, "load-changed",
                   G_CALLBACK(nv_linux_on_load_changed), window);
}

NV_INTERNAL void nv_linux_window_set_context_menu(nv_window_t *w, const nv_menu_item_t *items, int count) {
  if (!w) return;
  nv_linux_window_platform_t *p = nv_linux_get_window_platform(w);
  if (!p || !p->webview) return;
  if (p->ctx_menu_actions) {
    gtk_widget_insert_action_group(GTK_WIDGET(p->webview), "nvctx", NULL);
    g_clear_object(&p->ctx_menu_actions);
  }
  if (count <= 0 || !items) return;
  GSimpleActionGroup *grp = g_simple_action_group_new();
  if (!grp) return;
  for (int i = 0; i < count; i++) {
    const nv_menu_item_t *it = &items[i];
    if (it->separator) continue;
    gchar *an = g_strdup_printf("ctx%d", i);
    if (!an) continue;
    GSimpleAction *act = g_simple_action_new(an, NULL);
    g_free(an);
    if (!act) continue;
    g_object_set_data(G_OBJECT(act), "nv-win", w);
    g_object_set_data(G_OBJECT(act), "nv-ctx-id", (gpointer)(it->id ? it->id : ""));
    g_signal_connect(act, "activate", G_CALLBACK(nv_linux_ctx_menu_item_activated), NULL);
    g_simple_action_set_enabled(act, it->enabled ? TRUE : FALSE);
    g_action_map_add_action(G_ACTION_MAP(grp), G_ACTION(act));
    g_object_unref(act);
  }
  p->ctx_menu_actions = grp;
  gtk_widget_insert_action_group(GTK_WIDGET(p->webview), "nvctx", G_ACTION_GROUP(grp));
}

NV_INTERNAL void nv_linux_app_set_menu(nv_window_t *w, const nv_menu_item_t *items, int count) {
  if (!w) return;
  nv_linux_window_platform_t *p = nv_linux_get_window_platform(w);
  if (!p || !p->root_vbox || !p->webview) return;
  if (p->menubar) {
    gtk_container_remove(GTK_CONTAINER(p->root_vbox), p->menubar);
    gtk_widget_destroy(p->menubar);
    p->menubar = NULL;
  }
  GtkWidget *menubar = gtk_menu_bar_new();
  p->menubar = menubar;
  if (items && count > 0) {
    nv_linux_append_menu_items(GTK_MENU_SHELL(menubar), items, count, p);
  }
  gtk_box_pack_start(GTK_BOX(p->root_vbox), menubar, FALSE, FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(p->root_vbox), menubar, 0);
  gtk_widget_show_all(menubar);
}

NV_INTERNAL void nv_window_platform_destroy(nv_window_t *window) {
  if (!window) return;

  nv_linux_fs_watch_detach_for_window(window);

  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform) return;

  if (platform->ctx_menu_actions) {
    if (platform->webview) {
      gtk_widget_insert_action_group(GTK_WIDGET(platform->webview), "nvctx", NULL);
    }
    g_clear_object(&platform->ctx_menu_actions);
  }

  if (platform->ucm) {
    g_object_unref(platform->ucm);
  }
  if (platform->window) {
    gtk_widget_destroy(platform->window);
  }

  g_free(platform);
  nv_window_set_platform(window, NULL);
}

NV_INTERNAL void nv_window_platform_show(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;

  gtk_widget_show_all(platform->window);
  window->visible = 1;
}

NV_INTERNAL void nv_window_platform_hide(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;

  gtk_widget_hide(platform->window);
  window->visible = 0;
}

/* invoked when user requests window close (e.g. click X) */
static gboolean nv_linux_on_window_delete(GtkWidget *widget,
                                          GdkEvent  *event,
                                          gpointer    user_data) {
    (void)widget; (void)event;
    nv_window_t *window = (nv_window_t*)user_data;
    const char *id = nv_wm_get_id_by_window(window);
    if (id) {
        nv_arena_t *arena = nv_arena_create(1024);
        nv_json_t *before = nv_json_object(arena);
        nv_json_str(before, "id", id);
        nv_op_windows_push_all("windows.beforeClose", before, arena);
        nv_arena_destroy(arena);
    }
    nv_window_destroy(window);
    return TRUE; /* we handled destruction */
}

NV_INTERNAL void nv_window_platform_set_modal(nv_window_t *window, int enable) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;

  gtk_window_set_modal(GTK_WINDOW(platform->window), enable ? TRUE : FALSE);
}

NV_INTERNAL void nv_window_platform_set_title(nv_window_t *window, const char *title) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_set_title(GTK_WINDOW(platform->window), title ? title : "App");
}

NV_INTERNAL void nv_window_platform_center(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_set_position(GTK_WINDOW(platform->window), GTK_WIN_POS_CENTER);
}

NV_INTERNAL void nv_window_platform_set_size(nv_window_t *window, int width, int height) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_resize(GTK_WINDOW(platform->window), width, height);
}

NV_INTERNAL void nv_window_platform_get_size(nv_window_t *window, int *out_w, int *out_h) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  if (!gtk_widget_get_realized(platform->window)) return;
  int w = 0, h = 0;
  gtk_window_get_size(GTK_WINDOW(platform->window), &w, &h);
  if (out_w) *out_w = w;
  if (out_h) *out_h = h;
}

NV_INTERNAL void nv_window_platform_set_position(nv_window_t *window, int x, int y) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_move(GTK_WINDOW(platform->window), x, y);
}

NV_INTERNAL void nv_window_platform_get_position(nv_window_t *window, int *out_x, int *out_y) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  int x = 0, y = 0;
  gtk_window_get_position(GTK_WINDOW(platform->window), &x, &y);
  if (out_x) *out_x = x;
  if (out_y) *out_y = y;
}

NV_INTERNAL void nv_window_platform_fullscreen(nv_window_t *window, int enable) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  if (enable)
    gtk_window_fullscreen(GTK_WINDOW(platform->window));
  else
    gtk_window_unfullscreen(GTK_WINDOW(platform->window));
}

NV_INTERNAL int nv_window_platform_is_fullscreen(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return 0;
  return (gtk_window_get_window_type(GTK_WINDOW(platform->window)) == GTK_WINDOW_TOPLEVEL)
         && gtk_window_get_decorated(GTK_WINDOW(platform->window));
}

NV_INTERNAL void nv_window_platform_minimize(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_iconify(GTK_WINDOW(platform->window));
}

NV_INTERNAL void nv_window_platform_maximize(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_maximize(GTK_WINDOW(platform->window));
}

NV_INTERNAL void nv_window_platform_restore(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_unmaximize(GTK_WINDOW(platform->window));
  gtk_window_deiconify(GTK_WINDOW(platform->window));
}

NV_INTERNAL void nv_window_platform_focus(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_present(GTK_WINDOW(platform->window));
}

NV_INTERNAL int nv_window_platform_is_focused(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return 0;
  return gtk_window_is_active(GTK_WINDOW(platform->window)) ? 1 : 0;
}

NV_INTERNAL void nv_window_platform_set_resizable(nv_window_t *window, int enable) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_set_resizable(GTK_WINDOW(platform->window), enable ? TRUE : FALSE);
}

NV_INTERNAL void nv_window_platform_set_always_on_top(nv_window_t *window, int enable) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_set_keep_above(GTK_WINDOW(platform->window), enable ? TRUE : FALSE);
}

NV_INTERNAL void nv_window_platform_set_opacity(nv_window_t *window, int opacity_pct) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_window_set_opacity(GTK_WINDOW(platform->window), opacity_pct / 100.0);
}

NV_INTERNAL void nv_window_platform_set_zoom_factor(nv_window_t *window, double factor) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->webview) return;
  if (factor < 0.25) factor = 0.25;
  if (factor > 5.0) factor = 5.0;
  webkit_web_view_set_zoom_level(platform->webview, factor);
}

NV_INTERNAL void nv_window_platform_close(nv_window_t *window) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->window) return;
  gtk_widget_destroy(platform->window);
}

/* Public content/eval hooks called from cross-platform layer */
NV_INTERNAL void nv_window_platform_load_html(nv_window_t *window, const char *html, const char *base_url) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->webview || !html) return;
  webkit_web_view_load_html(platform->webview, html, base_url ? base_url : NULL);
}

NV_INTERNAL void nv_window_platform_load_url(nv_window_t *window, const char *url) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->webview || !url) return;
  webkit_web_view_load_uri(platform->webview, url);
}

NV_INTERNAL void nv_window_platform_eval_js(nv_window_t *window, const char *js) {
  nv_linux_window_platform_t *platform = nv_linux_get_window_platform(window);
  if (!platform || !platform->webview || !js) return;
#if WEBKIT_CHECK_VERSION(2, 22, 0)
  webkit_web_view_run_javascript(platform->webview, js, NULL, NULL, NULL);
#else
  webkit_web_view_execute_script(platform->webview, js);
#endif
}

/* =============================================================================
 * App paths & locale (UTF-8 via malloc; caller frees)
 * Weak on GCC/Clang so nv-ops tests can override with stubs.
 * =============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)
#define NV_LINUX_APP_PATH_ATTR __attribute__((weak))
#else
#define NV_LINUX_APP_PATH_ATTR
#endif

static int nv_linux_read_proc_self_exe(char *buf, size_t cap) {
  ssize_t n;

  if (!buf || cap < 2) return -1;
  n = readlink("/proc/self/exe", buf, cap - 1);
  if (n < 0) return -1;
  buf[n] = '\0';
  return 0;
}

NV_INTERNAL NV_LINUX_APP_PATH_ATTR char *nv_linux_get_exe_path(void) {
  char buf[PATH_MAX];

  if (nv_linux_read_proc_self_exe(buf, sizeof buf) != 0) return NULL;
  return strdup(buf);
}

NV_INTERNAL NV_LINUX_APP_PATH_ATTR char *nv_linux_get_resource_dir(void) {
  char   buf[PATH_MAX];
  char  *copy;
  char  *dir;
  char  *out;

  if (nv_linux_read_proc_self_exe(buf, sizeof buf) != 0) return NULL;
  copy = strdup(buf);
  if (!copy) return NULL;
  dir = dirname(copy);
  out = strdup(dir ? dir : ".");
  free(copy);
  return out;
}

NV_INTERNAL NV_LINUX_APP_PATH_ATTR char *nv_linux_get_data_dir(void) {
  const gchar *xdg = g_getenv("XDG_DATA_HOME");
  gchar       *base = NULL;
  gchar       *full;
  char        *out;

  if (xdg && xdg[0]) {
    base = g_strdup(xdg);
  } else {
    const gchar *home = g_getenv("HOME");
    if (!home || !home[0]) return NULL;
    base = g_build_filename(home, ".local", "share", NULL);
  }
  if (!base) return NULL;
  full = g_build_filename(base, "nativeview", NULL);
  g_free(base);
  if (!full) return NULL;
  (void)g_mkdir_with_parents(full, 0700);
  out = strdup(full);
  g_free(full);
  return out;
}

NV_INTERNAL NV_LINUX_APP_PATH_ATTR char *nv_linux_get_locale(void) {
  const char *raw = setlocale(LC_MESSAGES, "");
  const char *dot;
  size_t      n;
  char       *s;

  if (!raw || !raw[0]) return NULL;
  dot = strchr(raw, '.');
  n = dot ? (size_t)(dot - raw) : strlen(raw);
  s = (char *)malloc(n + 1);
  if (!s) return NULL;
  memcpy(s, raw, n);
  s[n] = '\0';
  for (size_t i = 0; s[i]; i++) {
    if (s[i] == '_') s[i] = '-';
  }
  return s;
}

/* =============================================================================
 * Shell (open URL/path, reveal parent folder) — GTK 3.22+ then GIO fallback
 *
 * Weak symbols: unit tests may override with strong stubs (see test_op_shell).
 * =============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)
#define NV_LINUX_SHELL_ATTR __attribute__((weak))
#else
#define NV_LINUX_SHELL_ATTR
#endif

static int nv_linux_shell_show_uri(const char *uri) {
  GError *err = NULL;

  if (!uri || !*uri) return -1;
#if GTK_CHECK_VERSION(3, 22, 0)
  if (gtk_show_uri_on_window(NULL, uri, GDK_CURRENT_TIME, &err)) return 0;
  g_clear_error(&err);
#endif
  if (g_app_info_launch_default_for_uri(uri, NULL, &err)) return 0;
  g_clear_error(&err);
  return -1;
}

NV_INTERNAL NV_LINUX_SHELL_ATTR int nv_linux_shell_open_url(const char *url) {
  if (!url || !*url) return -1;
  return nv_linux_shell_show_uri(url);
}

NV_INTERNAL NV_LINUX_SHELL_ATTR int nv_linux_shell_open_path(const char *path) {
  gchar *uri;

  if (!path) return -1;
  uri = g_filename_to_uri(path, NULL, NULL);
  if (!uri) return -1;
  {
    int rc = nv_linux_shell_show_uri(uri);
    g_free(uri);
    return rc;
  }
}

NV_INTERNAL NV_LINUX_SHELL_ATTR int nv_linux_shell_show_in_folder(const char *path) {
  gchar *dirname;
  gchar *uri;
  int rc;

  if (!path) return -1;
  dirname = g_path_get_dirname(path);
  if (!dirname) return -1;
  uri = g_filename_to_uri(dirname, NULL, NULL);
  g_free(dirname);
  if (!uri) return -1;
  rc = nv_linux_shell_show_uri(uri);
  g_free(uri);
  return rc;
}

extern char **environ;

enum { nv_linux_shell_capture_cap = 1048576 };

typedef struct {
  char *data;
  size_t len;
  size_t cap;
  int truncated;
} nv_linux_gbuf_t;

static int nv_linux_gbuf_append(nv_linux_gbuf_t *g, const char *src, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (g->truncated) return 0;
    if (g->len >= (size_t)nv_linux_shell_capture_cap) {
      g->truncated = 1;
      return 0;
    }
    if (g->len + 1 > g->cap) {
      size_t ncap = g->cap ? g->cap : 16;
      while (g->len + 1 > ncap) ncap *= 2;
      char *nb = (char *)realloc(g->data, ncap);
      if (!nb) return -1;
      g->data = nb;
      g->cap = ncap;
    }
    g->data[g->len++] = (unsigned char)src[i];
  }
  return 0;
}

static int nv_linux_read_both_pipes(int out_fd, int err_fd, char **out_s, char **err_s,
                                    int *out_trunc, int *err_trunc) {
  struct pollfd pf[2];
  nv_linux_gbuf_t go = {0};
  nv_linux_gbuf_t ge = {0};
  int out_done = 0;
  int err_done = 0;

  pf[0].fd = out_fd;
  pf[0].events = POLLIN;
  pf[1].fd = err_fd;
  pf[1].events = POLLIN;

  while (!out_done || !err_done) {
    int pr;
    do {
      pr = poll(pf, 2, -1);
    } while (pr < 0 && errno == EINTR);
    if (pr < 0) goto fail;

    for (int k = 0; k < 2; k++) {
      nv_linux_gbuf_t *g = (k == 0) ? &go : &ge;
      int *done = (k == 0) ? &out_done : &err_done;
      if (*done) continue;
      if (pf[k].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) {
        char temp[8192];
        ssize_t nr;
        do {
          nr = read((k == 0) ? out_fd : err_fd, temp, sizeof temp);
        } while (nr < 0 && errno == EINTR);
        if (nr < 0) goto fail;
        if (nr == 0) {
          *done = 1;
          pf[k].fd = -1;
        } else {
          if (nv_linux_gbuf_append(g, temp, (size_t)nr) != 0) goto fail;
        }
      }
    }
  }

  {
    char *fo = (char *)realloc(go.data, go.len + 1);
    char *fe = (char *)realloc(ge.data, ge.len + 1);
    if (!fo) {
      free(go.data);
      free(ge.data);
      return -1;
    }
    if (!fe) {
      free(fo);
      free(ge.data);
      return -1;
    }
    fo[go.len] = '\0';
    fe[ge.len] = '\0';
    *out_s = fo;
    *err_s = fe;
    *out_trunc = go.truncated;
    *err_trunc = ge.truncated;
  }
  return 0;

fail:
  free(go.data);
  free(ge.data);
  return -1;
}

NV_INTERNAL NV_LINUX_SHELL_ATTR nv_shell_result_t *nv_linux_shell_exec(const char *cmd) {
  int out_pipe[2] = {-1, -1};
  int err_pipe[2] = {-1, -1};
  int null_fd = -1;
  posix_spawn_file_actions_t fa;
  pid_t pid = 0;
  nv_shell_result_t *res = NULL;
  char *argv[] = {"sh", "-c", (char *)cmd, NULL};
  int st = 0;
  int out_trunc = 0;
  int err_trunc = 0;

  if (!cmd) return NULL;

  if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) goto fail;
  null_fd = open("/dev/null", O_RDONLY);
  if (null_fd < 0) goto fail;

  if (posix_spawn_file_actions_init(&fa) != 0) goto fail;
  st = 1;
  if (posix_spawn_file_actions_adddup2(&fa, null_fd, STDIN_FILENO) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, null_fd) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, out_pipe[0]) != 0) goto fail;
  if (posix_spawn_file_actions_adddup2(&fa, out_pipe[1], STDOUT_FILENO) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, out_pipe[1]) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, err_pipe[0]) != 0) goto fail;
  if (posix_spawn_file_actions_adddup2(&fa, err_pipe[1], STDERR_FILENO) != 0) goto fail;
  if (posix_spawn_file_actions_addclose(&fa, err_pipe[1]) != 0) goto fail;

  if (posix_spawn(&pid, "/bin/sh", &fa, NULL, argv, environ) != 0) goto fail;

  close(null_fd);
  null_fd = -1;
  close(out_pipe[1]);
  out_pipe[1] = -1;
  close(err_pipe[1]);
  err_pipe[1] = -1;
  posix_spawn_file_actions_destroy(&fa);
  st = 0;

  res = (nv_shell_result_t *)calloc(1, sizeof(nv_shell_result_t));
  if (!res) goto fail;

  if (nv_linux_read_both_pipes(out_pipe[0], err_pipe[0], &res->out, &res->err, &out_trunc,
                                 &err_trunc) != 0)
    goto fail;

  close(out_pipe[0]);
  out_pipe[0] = -1;
  close(err_pipe[0]);
  err_pipe[0] = -1;

  {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) goto fail;
    pid = 0;
    if (WIFEXITED(status)) {
      res->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      res->exit_code = 128 + WTERMSIG(status);
    } else {
      res->exit_code = -1;
    }
  }
  res->truncated = (out_trunc || err_trunc) ? 1 : 0;
  return res;

fail:
  if (pid > 0) {
    if (out_pipe[0] >= 0) {
      close(out_pipe[0]);
      out_pipe[0] = -1;
    }
    if (err_pipe[0] >= 0) {
      close(err_pipe[0]);
      err_pipe[0] = -1;
    }
    (void)waitpid(pid, NULL, 0);
    pid = 0;
  }
  if (st) posix_spawn_file_actions_destroy(&fa);
  if (null_fd >= 0) close(null_fd);
  if (out_pipe[0] >= 0) close(out_pipe[0]);
  if (out_pipe[1] >= 0) close(out_pipe[1]);
  if (err_pipe[0] >= 0) close(err_pipe[0]);
  if (err_pipe[1] >= 0) close(err_pipe[1]);
  if (res) {
    free(res->out);
    free(res->err);
    free(res);
  }
  return NULL;
}

/* =============================================================================
 * Modal file / message dialogs (GTK 3)
 *
 * Callback strings are heap-allocated (malloc) UTF-8 so nv_op_dialog can free
 * uniformly. Multi-select layout matches macOS: count in size_t immediately
 * before the char* array pointer.
 * =============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)
#define NV_LINUX_DLG_ATTR __attribute__((weak))
#else
#define NV_LINUX_DLG_ATTR
#endif

static char* nv_linux_dialog_strdup_utf8(const char* s) {
  const char* u = s ? s : "";
  size_t n = strlen(u) + 1;
  char* out = (char*)malloc(n);
  if (!out) return NULL;
  memcpy(out, u, n);
  return out;
}

static GtkWindow* nv_linux_dialog_parent_window(nv_dialog_ctx_t* ctx) {
  if (!ctx || !ctx->window) return NULL;
  nv_linux_window_platform_t* wp = nv_linux_get_window_platform(ctx->window);
  if (!wp || !wp->window || !GTK_IS_WINDOW(wp->window)) return NULL;
  return GTK_WINDOW(wp->window);
}

static GtkMessageType nv_linux_dialog_message_type(const char* type) {
  if (type && strcmp(type, "error") == 0) return GTK_MESSAGE_ERROR;
  if (type && strcmp(type, "warning") == 0) return GTK_MESSAGE_WARNING;
  return GTK_MESSAGE_INFO;
}

NV_INTERNAL NV_LINUX_DLG_ATTR void nv_linux_dialog_open_file_async(int allow_multiple,
                                                                   nv_dialog_ctx_t* ctx,
                                                                   nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (gtk_main_level() < 1) {
    callback(ctx, 1, NULL);
    return;
  }

  GtkWindow* parent = nv_linux_dialog_parent_window(ctx);
  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "Open File", parent, GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel", GTK_RESPONSE_CANCEL, "_Open",
      GTK_RESPONSE_ACCEPT, NULL);
  if (!dialog) {
    callback(ctx, 1, NULL);
    return;
  }

  GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
  gtk_file_chooser_set_select_multiple(chooser, allow_multiple ? TRUE : FALSE);

  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  int canceled = (response != GTK_RESPONSE_ACCEPT);
  char** paths = NULL;

  if (!canceled) {
    GSList* list = gtk_file_chooser_get_filenames(chooser);
    size_t count = list ? g_slist_length(list) : 0;
    if (count == 0) {
      canceled = 1;
      if (list) g_slist_free(list);
    } else {
      void* block = malloc(sizeof(size_t) + sizeof(char*) * count);
      if (!block) {
        canceled = 1;
        g_slist_free_full(list, g_free);
      } else {
        *(size_t*)block = count;
        paths = (char**)((size_t*)block + 1);
        GSList* it = list;
        size_t i = 0;
        for (; it; it = it->next, i++) {
          paths[i] = nv_linux_dialog_strdup_utf8((const char*)it->data);
          if (!paths[i]) {
            for (size_t j = 0; j < i; j++) free(paths[j]);
            free(block);
            paths = NULL;
            canceled = 1;
            break;
          }
        }
        g_slist_free_full(list, g_free);
      }
    }
  }

  gtk_widget_destroy(dialog);
  callback(ctx, canceled, paths);
}

NV_INTERNAL NV_LINUX_DLG_ATTR void nv_linux_dialog_save_file_async(nv_dialog_ctx_t* ctx,
                                                                  nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (gtk_main_level() < 1) {
    callback(ctx, 1, NULL);
    return;
  }

  GtkWindow* parent = nv_linux_dialog_parent_window(ctx);
  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "Save File", parent, GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save",
      GTK_RESPONSE_ACCEPT, NULL);
  if (!dialog) {
    callback(ctx, 1, NULL);
    return;
  }

  GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  int canceled = (response != GTK_RESPONSE_ACCEPT);
  char* path = NULL;

  if (!canceled) {
    gchar* gpath = gtk_file_chooser_get_filename(chooser);
    if (gpath) {
      path = nv_linux_dialog_strdup_utf8(gpath);
      g_free(gpath);
    }
    if (!path) canceled = 1;
  }

  gtk_widget_destroy(dialog);
  callback(ctx, canceled, path);
}

NV_INTERNAL NV_LINUX_DLG_ATTR void nv_linux_dialog_open_folder_async(nv_dialog_ctx_t* ctx,
                                                                     nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (gtk_main_level() < 1) {
    callback(ctx, 1, NULL);
    return;
  }

  GtkWindow* parent = nv_linux_dialog_parent_window(ctx);
  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "Select Folder", parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Cancel", GTK_RESPONSE_CANCEL,
      "_Select", GTK_RESPONSE_ACCEPT, NULL);
  if (!dialog) {
    callback(ctx, 1, NULL);
    return;
  }

  GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
  int response = gtk_dialog_run(GTK_DIALOG(dialog));
  int canceled = (response != GTK_RESPONSE_ACCEPT);
  char* path = NULL;

  if (!canceled) {
    gchar* gpath = gtk_file_chooser_get_filename(chooser);
    if (gpath) {
      path = nv_linux_dialog_strdup_utf8(gpath);
      g_free(gpath);
    }
    if (!path) canceled = 1;
  }

  gtk_widget_destroy(dialog);
  callback(ctx, canceled, path);
}

NV_INTERNAL NV_LINUX_DLG_ATTR void nv_linux_dialog_message_async(const char* title, const char* body,
                                                                 const char* type, const char** buttons,
                                                                 size_t btn_count, nv_dialog_ctx_t* ctx,
                                                                 nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (gtk_main_level() < 1) {
    int* idx = (int*)malloc(sizeof(int));
    if (idx) *idx = 0;
    callback(ctx, 0, (void*)idx);
    return;
  }

  GtkWindow* parent = nv_linux_dialog_parent_window(ctx);
  GtkMessageType mt = nv_linux_dialog_message_type(type);
  const char* primary = title ? title : "Message";
  const char* btn0 = (buttons && buttons[0]) ? buttons[0] : "OK";
  const char* btn1 = (buttons && btn_count > 1) ? buttons[1] : NULL;

  GtkWidget* dlg;
  if (!btn1 || !btn1[0]) {
    dlg = gtk_message_dialog_new(GTK_WINDOW(parent),
                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, mt,
                                 GTK_BUTTONS_NONE, "%s", primary);
    if (body && body[0])
      gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", body);
    gtk_dialog_add_button(GTK_DIALOG(dlg), btn0, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
  } else {
    dlg = gtk_message_dialog_new(GTK_WINDOW(parent),
                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, mt,
                                 GTK_BUTTONS_NONE, "%s", primary);
    if (body && body[0])
      gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", body);
    gtk_dialog_add_button(GTK_DIALOG(dlg), btn0, 100);
    gtk_dialog_add_button(GTK_DIALOG(dlg), btn1, 101);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), 100);
  }

  int response = gtk_dialog_run(GTK_DIALOG(dlg));
  int* idx = (int*)malloc(sizeof(int));
  if (!idx) {
    gtk_widget_destroy(dlg);
    callback(ctx, 0, NULL);
    return;
  }
  if (!btn1 || !btn1[0]) {
    (void)response;
    *idx = 0;
  } else {
    *idx = (response == 101) ? 1 : 0;
  }
  gtk_widget_destroy(dlg);
  callback(ctx, 0, (void*)idx);
}

NV_INTERNAL NV_LINUX_DLG_ATTR void nv_linux_dialog_confirm_async(const char* title, const char* body,
                                                                 nv_dialog_ctx_t* ctx,
                                                                 nv_dialog_cb_t callback) {
  if (!ctx || !callback) return;
  if (gtk_main_level() < 1) {
    int* yes = (int*)malloc(sizeof(int));
    if (yes) *yes = 0;
    callback(ctx, 0, (void*)yes);
    return;
  }

  GtkWindow* parent = nv_linux_dialog_parent_window(ctx);
  const char* primary = title ? title : "Confirm";
  GtkWidget* dlg = gtk_message_dialog_new(
      GTK_WINDOW(parent), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_YES_NO, "%s", primary);
  if (body && body[0])
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", body);

  int response = gtk_dialog_run(GTK_DIALOG(dlg));
  int* yes = (int*)malloc(sizeof(int));
  if (!yes) {
    gtk_widget_destroy(dlg);
    callback(ctx, 0, NULL);
    return;
  }
  *yes = (response == GTK_RESPONSE_YES) ? 1 : 0;
  gtk_widget_destroy(dlg);
  callback(ctx, 0, (void*)yes);
}

/* =============================================================================
 * Clipboard (GTK clipboard selection)
 *
 * read_text returns a malloc'd UTF-8 string; caller frees after use.
 * On GCC/Clang definitions are weak so unit tests can override with stubs.
 * Must run on the GTK main thread (same as op handlers).
 * =============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)
#define NV_LINUX_CLIP_ATTR __attribute__((weak))
#else
#define NV_LINUX_CLIP_ATTR
#endif

NV_INTERNAL NV_LINUX_CLIP_ATTR char* nv_linux_clipboard_read_text(void) {
  GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!cb) return NULL;
  gchar *gtext = gtk_clipboard_wait_for_text(cb);
  if (!gtext) return NULL;
  size_t n = strlen(gtext) + 1;
  char *out = (char*)malloc(n);
  if (!out) {
    g_free(gtext);
    return NULL;
  }
  memcpy(out, gtext, n);
  g_free(gtext);
  return out;
}

NV_INTERNAL NV_LINUX_CLIP_ATTR int nv_linux_clipboard_write_text(const char* utf8) {
  if (!utf8) return -1;
  GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!cb) return -1;
  gtk_clipboard_set_text(cb, utf8, -1);
  return 0;
}

NV_INTERNAL NV_LINUX_CLIP_ATTR void nv_linux_clipboard_clear(void) {
  GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!cb) return;
  gtk_clipboard_clear(cb);
}

NV_INTERNAL NV_LINUX_CLIP_ATTR int nv_linux_clipboard_has_text(void) {
  GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!cb) return 0;
  return gtk_clipboard_wait_is_text_available(cb) ? 1 : 0;
}

static void nv_linux_png_ihdr_dims(const guchar *p, gint len, int *w, int *h) {
  static const guchar sig[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
  if (w) *w = 0;
  if (h) *h = 0;
  if (!p || len < 24) return;
  if (memcmp(p, sig, 8u) != 0) return;
  if (memcmp(p + 12, "IHDR", 4u) != 0) return;
  if (w)
    *w = (int)((guint)p[16] << 24 | (guint)p[17] << 16 | (guint)p[18] << 8 | (guint)p[19]);
  if (h)
    *h = (int)((guint)p[20] << 24 | (guint)p[21] << 16 | (guint)p[22] << 8 | (guint)p[23]);
}

NV_INTERNAL NV_LINUX_CLIP_ATTR char *nv_linux_clipboard_read_image(int *out_w, int *out_h) {
  GtkClipboard *cb;
  GdkAtom       pnga;
  int           w = 0, h = 0;
  guchar       *pngbuf = NULL;
  gsize         pnglen = 0;
  GError       *err = NULL;

  if (out_w) *out_w = 0;
  if (out_h) *out_h = 0;
  cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!cb) return NULL;

  pnga = gdk_atom_intern_static_string("image/png");
  {
    GtkSelectionData *sel = gtk_clipboard_wait_for_contents(cb, pnga);
    if (sel) {
      const guchar *d = gtk_selection_data_get_data(sel);
      gint          ln = gtk_selection_data_get_length(sel);
      if (d && ln > 24) {
        nv_linux_png_ihdr_dims(d, ln, &w, &h);
        if (w > 0 && h > 0) {
          pngbuf = (guchar *)g_malloc((gsize)ln);
          if (!pngbuf) {
            gtk_selection_data_free(sel);
            return NULL;
          }
          memcpy(pngbuf, d, (gsize)ln);
          pnglen = (gsize)ln;
        }
      }
      gtk_selection_data_free(sel);
    }
  }

  if (!pngbuf) {
    GdkPixbuf *pb = gtk_clipboard_wait_for_image(cb);
    gchar    *sbuf = NULL;
    if (!pb) return NULL;
    w = gdk_pixbuf_get_width(pb);
    h = gdk_pixbuf_get_height(pb);
    if (!gdk_pixbuf_save_to_buffer(pb, &sbuf, &pnglen, "png", &err, NULL)) {
      g_clear_error(&err);
      g_object_unref(pb);
      return NULL;
    }
    g_object_unref(pb);
    pngbuf = (guchar *)sbuf;
    if (w <= 0 || h <= 0) nv_linux_png_ihdr_dims(pngbuf, (gint)pnglen, &w, &h);
  }

  if (!pngbuf || pnglen == 0 || w <= 0 || h <= 0) {
    g_free(pngbuf);
    return NULL;
  }

  {
    size_t  encb = nv_base64_encode_bound(pnglen);
    char   *out = (char *)malloc(encb);
    int     encn;
    if (!out) {
      g_free(pngbuf);
      return NULL;
    }
    encn = nv_base64_encode((const uint8_t *)pngbuf, pnglen, out, encb);
    g_free(pngbuf);
    if (encn < 0) {
      free(out);
      return NULL;
    }
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return out;
  }
}

NV_INTERNAL NV_LINUX_CLIP_ATTR int nv_linux_clipboard_write_image(const char *base64_png) {
  GtkClipboard     *cb;
  GdkPixbufLoader  *loader;
  GdkPixbuf        *pb;
  GError           *err = NULL;
  uint8_t          stack[16384];
  uint8_t          *raw = stack;
  size_t            maxd, dlen = 0;
  int               rc = -1;

  if (!base64_png) return -1;
  cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!cb) return -1;

  maxd = nv_base64_decode_max_out(strlen(base64_png));
  if (maxd > sizeof(stack)) {
    raw = (uint8_t *)malloc(maxd ? maxd : 1u);
    if (!raw) return -1;
  }
  if (nv_base64_decode(base64_png, strlen(base64_png), raw, maxd ? maxd : 1u, &dlen) != 0) goto done;
  if (dlen < 24u) goto done;

  loader = gdk_pixbuf_loader_new();
  if (!loader) goto done;
  if (!gdk_pixbuf_loader_write(loader, raw, (gsize)dlen, &err)) {
    g_clear_error(&err);
    g_object_unref(loader);
    goto done;
  }
  if (!gdk_pixbuf_loader_close(loader, &err)) {
    g_clear_error(&err);
    g_object_unref(loader);
    goto done;
  }
  pb = gdk_pixbuf_loader_get_pixbuf(loader);
  if (!pb) {
    g_object_unref(loader);
    goto done;
  }
  g_object_ref(pb);
  g_object_unref(loader);
  gtk_clipboard_set_image(cb, pb);
  g_object_unref(pb);
  rc = 0;

done:
  if (raw != stack) free(raw);
  return rc;
}

NV_INTERNAL NV_LINUX_CLIP_ATTR int nv_linux_clipboard_has_image(void) {
  GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  if (!cb) return 0;
  if (gtk_clipboard_wait_is_target_available(cb, gdk_atom_intern_static_string("image/png"))) return 1;
  return gtk_clipboard_wait_is_image_available(cb) ? 1 : 0;
}

/* =============================================================================
 * Screen / monitor info (JSON strings for nv_op_screen; caller frees with free)
 * GdkMonitor API requires GTK 3.22+ (WebKitGTK stack).
 * =============================================================================
 */

#if GTK_CHECK_VERSION(3, 22, 0)

static void nv_linux_screen_json_append_esc(GString *s, const char *utf8) {
  const unsigned char *p = (const unsigned char *)(utf8 ? utf8 : "");

  g_string_append_c(s, '"');
  while (*p) {
    unsigned char c = *p;
    if (c == '"' || c == '\\') {
      g_string_append_c(s, '\\');
      g_string_append_c(s, (char)c);
    } else if (c < 0x20u) {
      g_string_append_printf(s, "\\u%04x", (unsigned int)c);
    } else {
      g_string_append_len(s, (const char *)p, 1);
    }
    p++;
  }
  g_string_append_c(s, '"');
}

static char *nv_linux_screen_gstring_to_strdup(GString *gs) {
  gchar *gstr;
  char   *out;

  if (!gs) return NULL;
  gstr = g_string_free(gs, FALSE);
  if (!gstr) return NULL;
  out = strdup(gstr);
  g_free(gstr);
  return out;
}

/* id_override >= 0 forces JSON "id" (getPrimary uses 0 per macOS/Windows). */
static int nv_linux_screen_append_display(GString *out, GdkDisplay *disp, int idx, int need_comma,
                                          int id_override) {
  GdkMonitor   *mon;
  GdkRectangle  geo;
  GdkRectangle  work;
  const char   *model;
  int           scale_i;
  double        scale;
  int           is_primary;
  int           id;
  GdkMonitor   *primary_mon;

  mon = gdk_display_get_monitor(disp, idx);
  if (!mon) return -1;

  gdk_monitor_get_geometry(mon, &geo);
  gdk_monitor_get_workarea(mon, &work);
  model = gdk_monitor_get_model(mon);
  scale_i = gdk_monitor_get_scale_factor(mon);
  scale = (scale_i > 0) ? (double)scale_i : 1.0;
  primary_mon = gdk_display_get_primary_monitor(disp);
  is_primary = (primary_mon != NULL && mon == primary_mon);
  id = (id_override >= 0) ? id_override : idx;

  if (need_comma) g_string_append_c(out, ',');

  g_string_append_printf(out,
                         "{\"id\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"scaleFactor\":%.9g,"
                         "\"localizedName\":",
                         id, geo.x, geo.y, geo.width, geo.height, scale);
  nv_linux_screen_json_append_esc(out, (model && model[0]) ? model : "");
  g_string_append_printf(out,
                         ",\"isPrimary\":%s,\"bounds\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d},"
                         "\"workArea\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}}",
                         is_primary ? "true" : "false", geo.x, geo.y, geo.width, geo.height, work.x, work.y,
                         work.width, work.height);
  return 0;
}

NV_INTERNAL char *nv_linux_screen_get_all(void) {
  GdkDisplay *disp;
  int         n;
  GString    *out;

  disp = gdk_display_get_default();
  if (!disp) return NULL;
  n = gdk_display_get_n_monitors(disp);
  out = g_string_new("[");
  if (!out) return NULL;
  for (int i = 0; i < n; i++) {
    if (nv_linux_screen_append_display(out, disp, i, i > 0, -1) != 0) {
      g_string_free(out, TRUE);
      return NULL;
    }
  }
  g_string_append_c(out, ']');
  return nv_linux_screen_gstring_to_strdup(out);
}

NV_INTERNAL char *nv_linux_screen_get_primary(void) {
  GdkDisplay *disp;
  gint        idx;
  int         n;
  GString    *out;

  disp = gdk_display_get_default();
  if (!disp) return NULL;
  n = gdk_display_get_n_monitors(disp);
  if (n <= 0) return NULL;
  {
    GdkMonitor *primary_mon = gdk_display_get_primary_monitor(disp);
    idx = 0;
    if (primary_mon) {
      int found = 0;
      for (int i = 0; i < n; i++) {
        if (gdk_display_get_monitor(disp, i) == primary_mon) {
          idx = (gint)i;
          found = 1;
          break;
        }
      }
      if (!found) idx = 0;
    }
  }

  out = g_string_new(NULL);
  if (!out) return NULL;
  if (nv_linux_screen_append_display(out, disp, (int)idx, 0, 0) != 0) {
    g_string_free(out, TRUE);
    return NULL;
  }
  return nv_linux_screen_gstring_to_strdup(out);
}

NV_INTERNAL char *nv_linux_screen_get_cursor(void) {
  GdkDisplay *disp;
  gint        x = 0;
  gint        y = 0;
  GString    *gs;

  disp = gdk_display_get_default();
  if (!disp) return NULL;

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  gdk_display_get_pointer(disp, NULL, &x, &y, NULL);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

  gs = g_string_new(NULL);
  if (!gs) return NULL;
  g_string_append_printf(gs, "{\"x\":%d,\"y\":%d}", (int)x, (int)y);
  return nv_linux_screen_gstring_to_strdup(gs);
}

#else /* GTK < 3.22 */

NV_INTERNAL char *nv_linux_screen_get_all(void) { return NULL; }

NV_INTERNAL char *nv_linux_screen_get_primary(void) { return NULL; }

NV_INTERNAL char *nv_linux_screen_get_cursor(void) { return NULL; }

#endif /* GTK_CHECK_VERSION(3, 22, 0) */

/* =============================================================================
 * Desktop notifications (libnotify → org.freedesktop.Notifications)
 * =============================================================================
 */

#if defined(__GNUC__) || defined(__clang__)
#define NV_LINUX_NOTIF_ATTR __attribute__((weak))
#else
#define NV_LINUX_NOTIF_ATTR
#endif

#ifdef NV_HAS_LIBNOTIFY
static GHashTable *g_nv_linux_notifications;
#endif

static void nv_linux_notifications_shutdown(void) {
#ifdef NV_HAS_LIBNOTIFY
  if (g_nv_linux_notifications) {
    g_hash_table_destroy(g_nv_linux_notifications);
    g_nv_linux_notifications = NULL;
  }
  if (notify_is_initted()) {
    notify_uninit();
  }
#endif
}

#ifdef NV_HAS_LIBNOTIFY

static void nv_linux_notif_send_id_event(nv_window_t *w, const char *event, long long nid) {
  char buf[64];

  if (!w || nid <= 0 || !event) {
    return;
  }
  if (snprintf(buf, sizeof buf, "{\"id\":%lld}", nid) < 0) {
    return;
  }
  nv_send(w, event, buf);
}

static int nv_linux_notif_caps_include_actions(void) {
  GList *caps;
  GList *l;

  caps = notify_get_server_caps();
  for (l = caps; l; l = l->next) {
    if (g_strcmp0((const char *)l->data, "actions") == 0) {
      return 1;
    }
  }
  return 0;
}

typedef struct {
  long long    id;
  nv_window_t *window;
} nv_linux_notif_action_ctx_t;

static void nv_linux_notif_action_ctx_free(gpointer p) {
  g_free(p);
}

static void nv_linux_notif_on_default_action(NotifyNotification *n, char *action, gpointer user_data) {
  nv_linux_notif_action_ctx_t *ctx = (nv_linux_notif_action_ctx_t *)user_data;

  (void)n;
  (void)action;
  if (!ctx || !ctx->window) {
    return;
  }
  nv_linux_notif_send_id_event(ctx->window, "notification.clicked", ctx->id);
}

static void nv_linux_notif_on_closed(NotifyNotification *n, gpointer user_data) {
  long long                    id = (long long)(intptr_t)user_data;
  gpointer                     key = (gpointer)(intptr_t)id;
  NotifyNotification *stored;

  if (!g_nv_linux_notifications || id <= 0) {
    return;
  }
  stored = (NotifyNotification *)g_hash_table_lookup(g_nv_linux_notifications, key);
  if (stored != n) {
    return;
  }
  g_hash_table_steal(g_nv_linux_notifications, key);
  g_object_unref(n);
}

static void nv_linux_notif_set_icon_from_file(NotifyNotification *n, const char *path) {
  GError     *err = NULL;
  GdkPixbuf  *pb;

  if (!n || !path || !path[0]) {
    return;
  }
  pb = gdk_pixbuf_new_from_file(path, &err);
  if (!pb) {
    g_clear_error(&err);
    return;
  }
  notify_notification_set_image_from_pixbuf(n, pb);
  g_object_unref(pb);
}

static NotifyNotification *nv_linux_notif_build(long long id, const char *title, const char *body,
                                                const char *icon_path, nv_window_t *w) {
  const char           *t = (title && title[0]) ? title : " ";
  const char           *b = body ? body : "";
  NotifyNotification   *n;
  int                   want_actions;

  (void)id;
  if (icon_path && icon_path[0] && g_file_test(icon_path, G_FILE_TEST_IS_REGULAR)) {
    n = notify_notification_new(t, b, NULL);
    if (n) {
      nv_linux_notif_set_icon_from_file(n, icon_path);
    }
  } else {
    n = notify_notification_new(t, b, (icon_path && icon_path[0]) ? icon_path : NULL);
  }
  if (!n) {
    return NULL;
  }

  g_object_set_data(G_OBJECT(n), "nv-win", w);
  g_signal_connect(n, "closed", G_CALLBACK(nv_linux_notif_on_closed), (gpointer)(intptr_t)id);

  want_actions = nv_linux_notif_caps_include_actions();
  if (want_actions && w) {
    nv_linux_notif_action_ctx_t *act = g_new0(nv_linux_notif_action_ctx_t, 1);
    if (act) {
      act->id = id;
      act->window = w;
      if (!notify_notification_add_action(n, "default", "default", nv_linux_notif_on_default_action, act,
                                           nv_linux_notif_action_ctx_free)) {
        g_free(act);
      }
    }
  }

  return n;
}

NV_INTERNAL NV_LINUX_NOTIF_ATTR int nv_linux_notification_show(long long id, const char *title,
                                                                 const char *body, const char *icon_path,
                                                                 nv_window_t *w) {
  NotifyNotification *n;
  GError             *err = NULL;
  gpointer            key;

  if (id <= 0 || !w) {
    return -1;
  }
  if (!notify_is_initted()) {
    if (!notify_init("nativeview")) {
      return -2;
    }
  }
  if (!g_nv_linux_notifications) {
    g_nv_linux_notifications =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_object_unref);
    if (!g_nv_linux_notifications) {
      return -2;
    }
  }

  key = (gpointer)(intptr_t)id;
  n = (NotifyNotification *)g_hash_table_lookup(g_nv_linux_notifications, key);
  if (n) {
    g_hash_table_steal(g_nv_linux_notifications, key);
    notify_notification_close(n, NULL);
    g_object_unref(n);
  }

  n = nv_linux_notif_build(id, title, body, icon_path, w);
  if (!n) {
    return -2;
  }

  if (!notify_notification_show(n, &err)) {
    g_clear_error(&err);
    g_object_unref(n);
    return -2;
  }

  g_hash_table_insert(g_nv_linux_notifications, key, n);
  return 0;
}

NV_INTERNAL NV_LINUX_NOTIF_ATTR int nv_linux_notification_close(long long id) {
  gpointer            key = (gpointer)(intptr_t)id;
  NotifyNotification *n;
  GError             *err = NULL;

  if (id <= 0) {
    return -1;
  }
  if (!g_nv_linux_notifications) {
    return 0;
  }
  n = (NotifyNotification *)g_hash_table_lookup(g_nv_linux_notifications, key);
  if (!n) {
    return 0;
  }
  g_hash_table_steal(g_nv_linux_notifications, key);
  if (!notify_notification_close(n, &err)) {
    g_clear_error(&err);
  }
  g_object_unref(n);
  return 0;
}

#else /* !NV_HAS_LIBNOTIFY */

NV_INTERNAL NV_LINUX_NOTIF_ATTR int nv_linux_notification_show(long long id, const char *title,
                                                               const char *body, const char *icon_path,
                                                               nv_window_t *w) {
  (void)id;
  (void)title;
  (void)body;
  (void)icon_path;
  (void)w;
  return NV_PLATFORM_RC_NOT_SUPPORTED;
}

NV_INTERNAL NV_LINUX_NOTIF_ATTR int nv_linux_notification_close(long long id) {
  (void)id;
  return NV_PLATFORM_RC_NOT_SUPPORTED;
}

#endif /* NV_HAS_LIBNOTIFY */

#endif /* defined(__linux__) && !defined(__APPLE__) */
