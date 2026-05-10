/*
 * Copyright (c) 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#if defined(__linux__) && !defined(__APPLE__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#ifdef NV_HAS_APPINDICATOR
#if defined(__has_include)
#if __has_include(<libayatana-appindicator/app-indicator.h>)
#include <libayatana-appindicator/app-indicator.h>
#elif __has_include(<libappindicator/app-indicator.h>)
#include <libappindicator/app-indicator.h>
#else
#include <libayatana-appindicator/app-indicator.h>
#endif
#else
#include <libayatana-appindicator/app-indicator.h>
#endif
#endif

#include "nv_core_internal.h"
#include "nv_window_internal.h"
#include "nv.h"

#if defined(__GNUC__) || defined(__clang__)
#define NV_LINUX_TRAY_ATTR __attribute__((weak))
#else
#define NV_LINUX_TRAY_ATTR
#endif

enum { nv_linux_tray_max = 8 };

typedef struct nv_linux_tray_slot {
  long long id;
  nv_window_t *window;
#ifdef NV_HAS_APPINDICATOR
  AppIndicator *indicator;
#else
  GtkStatusIcon *status_icon;
#endif
} nv_linux_tray_slot_t;

static nv_linux_tray_slot_t g_nv_linux_trays[nv_linux_tray_max];
static int g_nv_linux_tray_count;

static int nv_linux_tray_index_by_id(long long tray_id) {
  for (int i = 0; i < g_nv_linux_tray_count; i++) {
    if (g_nv_linux_trays[i].id == tray_id) {
      return i;
    }
  }
  return -1;
}

#ifndef NV_HAS_APPINDICATOR
static int nv_linux_tray_status_icon_usable(void) {
  return (g_type_from_name("GtkStatusIcon") != 0) ? 1 : 0;
}
#endif

static void nv_linux_tray_send_clicked(nv_window_t *w, long long tray_id) {
  char buf[64];

  if (!w || tray_id <= 0) {
    return;
  }
  if (snprintf(buf, sizeof buf, "{\"id\":%lld}", tray_id) < 0) {
    return;
  }
  nv_send(w, "tray.clicked", buf);
}

static void nv_linux_tray_send_menu_action(nv_window_t *w, long long tray_id, long long item_id) {
  char buf[96];

  if (!w || tray_id <= 0) {
    return;
  }
  if (snprintf(buf, sizeof buf, "{\"id\":%lld,\"itemId\":%lld}", tray_id, item_id) < 0) {
    return;
  }
  nv_send(w, "tray.menuAction", buf);
}

typedef struct nv_linux_tray_menu_ctx {
  nv_window_t *window;
  long long tray_id;
  long long item_id;
} nv_linux_tray_menu_ctx_t;

static void nv_linux_tray_on_menu_activate(GtkMenuItem *item, gpointer user_data) {
  nv_linux_tray_menu_ctx_t *ctx = (nv_linux_tray_menu_ctx_t *)user_data;

  (void)item;
  if (!ctx || !ctx->window) {
    return;
  }
  nv_linux_tray_send_menu_action(ctx->window, ctx->tray_id, ctx->item_id);
}

#ifndef NV_HAS_APPINDICATOR
static void nv_linux_tray_on_status_activate(GtkStatusIcon *icon, gpointer user_data) {
  int i;

  (void)user_data;
  for (i = 0; i < g_nv_linux_tray_count; i++) {
    if (g_nv_linux_trays[i].status_icon == icon) {
      nv_linux_tray_send_clicked(g_nv_linux_trays[i].window, g_nv_linux_trays[i].id);
      return;
    }
  }
}
#endif

static GtkMenu *nv_linux_tray_build_menu(long long tray_id, nv_window_t *w, const char **labels,
                                         const long long *item_ids, int count) {
  GtkMenu *menu = GTK_MENU(gtk_menu_new());
  int i;

  if (!menu) {
    return NULL;
  }

  for (i = 0; i < count; i++) {
    if (item_ids && item_ids[i] == -1) {
      GtkWidget *sep = gtk_separator_menu_item_new();
      if (sep) {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);
      }
      continue;
    }

    {
      const char *lbl = (labels && labels[i]) ? labels[i] : "";
      GtkWidget *mi = gtk_menu_item_new_with_label(lbl);

      if (!mi) {
        continue;
      }

      {
        nv_linux_tray_menu_ctx_t *ctx = g_new(nv_linux_tray_menu_ctx_t, 1);
        if (!ctx) {
          gtk_widget_destroy(mi);
          continue;
        }
        ctx->window = w;
        ctx->tray_id = tray_id;
        ctx->item_id = item_ids ? item_ids[i] : 0;

        g_signal_connect_data(mi, "activate", G_CALLBACK(nv_linux_tray_on_menu_activate), ctx,
                              (GClosureNotify)g_free, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
        gtk_widget_show(mi);
      }
    }
  }

  gtk_widget_show(GTK_WIDGET(menu));
  return menu;
}

static void nv_linux_tray_remove_at(int idx) {
  int tail;

  if (idx < 0 || idx >= g_nv_linux_tray_count) {
    return;
  }

#ifdef NV_HAS_APPINDICATOR
  if (g_nv_linux_trays[idx].indicator) {
    app_indicator_set_menu(g_nv_linux_trays[idx].indicator, NULL);
    g_object_unref(g_nv_linux_trays[idx].indicator);
    g_nv_linux_trays[idx].indicator = NULL;
  }
#else
  if (g_nv_linux_trays[idx].status_icon) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gtk_status_icon_set_menu(g_nv_linux_trays[idx].status_icon, NULL);
#pragma GCC diagnostic pop
    g_object_unref(g_nv_linux_trays[idx].status_icon);
    g_nv_linux_trays[idx].status_icon = NULL;
  }
#endif

  tail = g_nv_linux_tray_count - idx - 1;
  if (tail > 0) {
    memmove(&g_nv_linux_trays[idx], &g_nv_linux_trays[idx + 1], (size_t)tail * sizeof(g_nv_linux_trays[0]));
  }
  g_nv_linux_tray_count--;
  memset(&g_nv_linux_trays[g_nv_linux_tray_count], 0, sizeof(g_nv_linux_trays[0]));
}

NV_INTERNAL NV_LINUX_TRAY_ATTR int nv_linux_tray_create(long long tray_id, const char *icon_path,
                                                        const char *tooltip, nv_window_t *w) {
#ifndef NV_HAS_APPINDICATOR
  if (!nv_linux_tray_status_icon_usable()) {
    return NV_TRAY_RC_NOT_SUPPORTED;
  }
#endif

  if (tray_id <= 0 || !w) {
    return -1;
  }
  if (!gtk_init_check(0, NULL)) {
    return -1;
  }
  if (nv_linux_tray_index_by_id(tray_id) >= 0) {
    return -1;
  }
  if (g_nv_linux_tray_count >= nv_linux_tray_max) {
    return -1;
  }

#ifdef NV_HAS_APPINDICATOR
  {
    gchar *unique = g_strdup_printf("nativeview-tray-%lld", tray_id);
    AppIndicator *ind;

    if (!unique) {
      return -1;
    }
    ind = app_indicator_new(unique, "application-x-executable", APP_INDICATOR_CATEGORY_APPLICATION_STATUS,
                            NULL);
    g_free(unique);
    if (!ind) {
      return -1;
    }
    if (icon_path && icon_path[0]) {
      app_indicator_set_icon_full(ind, "nativeview-tray", icon_path);
    }
    if (tooltip && tooltip[0]) {
      app_indicator_set_title(ind, tooltip);
    }
    app_indicator_set_status(ind, APP_INDICATOR_STATUS_ACTIVE);

    g_nv_linux_trays[g_nv_linux_tray_count].id = tray_id;
    g_nv_linux_trays[g_nv_linux_tray_count].window = w;
    g_nv_linux_trays[g_nv_linux_tray_count].indicator = ind;
    g_nv_linux_tray_count++;
  }
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  {
    GtkStatusIcon *icon = NULL;

    if (icon_path && icon_path[0]) {
      icon = gtk_status_icon_new_from_file(icon_path);
    }
    if (!icon) {
      icon = gtk_status_icon_new_from_icon_name("application-x-executable");
    }
    if (!icon) {
      return -1;
    }
    if (tooltip && tooltip[0]) {
      gtk_status_icon_set_tooltip_text(icon, tooltip);
    }
    gtk_status_icon_connect_activate(icon, G_CALLBACK(nv_linux_tray_on_status_activate), NULL);

    g_nv_linux_trays[g_nv_linux_tray_count].id = tray_id;
    g_nv_linux_trays[g_nv_linux_tray_count].window = w;
    g_nv_linux_trays[g_nv_linux_tray_count].status_icon = icon;
    g_nv_linux_tray_count++;
  }
#pragma GCC diagnostic pop
#endif

  return 0;
}

NV_INTERNAL NV_LINUX_TRAY_ATTR int nv_linux_tray_destroy(long long tray_id) {
  int idx = nv_linux_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }
  nv_linux_tray_remove_at(idx);
  return 0;
}

NV_INTERNAL NV_LINUX_TRAY_ATTR int nv_linux_tray_set_icon(long long tray_id, const char *icon_path) {
  int idx;

  if (!icon_path || !icon_path[0]) {
    return -1;
  }
  idx = nv_linux_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }

#ifdef NV_HAS_APPINDICATOR
  if (!g_nv_linux_trays[idx].indicator) {
    return -1;
  }
  app_indicator_set_icon_full(g_nv_linux_trays[idx].indicator, "nativeview-tray", icon_path);
  return 0;
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if (!g_nv_linux_trays[idx].status_icon) {
    return -1;
  }
  gtk_status_icon_set_from_file(g_nv_linux_trays[idx].status_icon, icon_path);
  return 0;
#pragma GCC diagnostic pop
#endif
}

NV_INTERNAL NV_LINUX_TRAY_ATTR int nv_linux_tray_set_tooltip(long long tray_id, const char *tooltip) {
  int idx = nv_linux_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }

#ifdef NV_HAS_APPINDICATOR
  if (!g_nv_linux_trays[idx].indicator) {
    return -1;
  }
  app_indicator_set_title(g_nv_linux_trays[idx].indicator, tooltip ? tooltip : "");
  return 0;
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if (!g_nv_linux_trays[idx].status_icon) {
    return -1;
  }
  gtk_status_icon_set_tooltip_text(g_nv_linux_trays[idx].status_icon, tooltip ? tooltip : "");
  return 0;
#pragma GCC diagnostic pop
#endif
}

NV_INTERNAL NV_LINUX_TRAY_ATTR int nv_linux_tray_set_menu(long long tray_id, const char **labels,
                                                          const long long *item_ids, int count) {
  int idx;
  nv_window_t *w;
  GtkMenu *menu;

  if (count < 0) {
    return -1;
  }
  idx = nv_linux_tray_index_by_id(tray_id);
  if (idx < 0) {
    return -1;
  }
  w = g_nv_linux_trays[idx].window;
  if (!w) {
    return -1;
  }

#ifdef NV_HAS_APPINDICATOR
  if (!g_nv_linux_trays[idx].indicator) {
    return -1;
  }
  app_indicator_set_menu(g_nv_linux_trays[idx].indicator, NULL);
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  if (!g_nv_linux_trays[idx].status_icon) {
    return -1;
  }
  gtk_status_icon_set_menu(g_nv_linux_trays[idx].status_icon, NULL);
#pragma GCC diagnostic pop
#endif

  menu = nv_linux_tray_build_menu(tray_id, w, labels, item_ids, count);
  if (!menu) {
    return -1;
  }

#ifdef NV_HAS_APPINDICATOR
  app_indicator_set_menu(g_nv_linux_trays[idx].indicator, menu);
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  gtk_status_icon_set_menu(g_nv_linux_trays[idx].status_icon, menu);
#pragma GCC diagnostic pop
#endif

  return 0;
}

#endif /* defined(__linux__) && !defined(__APPLE__) */
