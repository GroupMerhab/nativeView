/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_hotkey_combo.c — Parse global hotkey accelerator strings
 * =============================================================================
 */

#include "nv_window_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int nv_hk_ieq(const char* a, const char* b) {
  if (!a || !b) return 0;
  for (; *a && *b; a++, b++) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  }
  return *a == *b;
}

static void trim_inplace(char* s) {
  char* p = s;
  while (*p && isspace((unsigned char)*p)) p++;
  if (p != s) memmove(s, p, strlen(p) + 1);
  size_t n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    s[n - 1] = '\0';
    n--;
  }
}

static int tok_key(const char* t, nv_hotkey_combo_t* out) {
  if (!t || !*t) return -1;
  if ((t[0] == 'f' || t[0] == 'F') && t[1] >= '0' && t[1] <= '9') {
    int n = atoi(t + 1);
    if (n < 1 || n > 24) return -1;
    out->is_fn = 1;
    out->fn_index = n;
    out->key_char = 0;
    out->special = NV_HOTKEY_SPECIAL_NONE;
    return 0;
  }
  if (nv_hk_ieq(t, "Space")) {
    out->is_fn = 0;
    out->fn_index = 0;
    out->key_char = 0;
    out->special = NV_HOTKEY_SPECIAL_SPACE;
    return 0;
  }
  if (nv_hk_ieq(t, "Tab")) {
    out->is_fn = 0;
    out->fn_index = 0;
    out->key_char = 0;
    out->special = NV_HOTKEY_SPECIAL_TAB;
    return 0;
  }
  if (nv_hk_ieq(t, "Return") || nv_hk_ieq(t, "Enter")) {
    out->is_fn = 0;
    out->fn_index = 0;
    out->key_char = 0;
    out->special = NV_HOTKEY_SPECIAL_RETURN;
    return 0;
  }
  if (nv_hk_ieq(t, "Escape") || nv_hk_ieq(t, "Esc")) {
    out->is_fn = 0;
    out->fn_index = 0;
    out->key_char = 0;
    out->special = NV_HOTKEY_SPECIAL_ESCAPE;
    return 0;
  }
  if (strlen(t) == 1) {
    char c = t[0];
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out->is_fn = 0;
      out->fn_index = 0;
      out->key_char = c;
      out->special = NV_HOTKEY_SPECIAL_NONE;
      return 0;
    }
  }
  return -1;
}

static int apply_mod(const char* t, nv_hotkey_combo_t* out) {
  if (!t || !*t) return -1;
  if (nv_hk_ieq(t, "Shift")) {
    out->mod_flags |= NV_HOTKEY_MOD_SHIFT;
    return 0;
  }
  if (nv_hk_ieq(t, "Control") || nv_hk_ieq(t, "Ctrl")) {
    out->mod_flags |= NV_HOTKEY_MOD_CTRL;
    return 0;
  }
  if (nv_hk_ieq(t, "Alt") || nv_hk_ieq(t, "Option")) {
    out->mod_flags |= NV_HOTKEY_MOD_ALT;
    return 0;
  }
  if (nv_hk_ieq(t, "CmdOrCtrl")) {
    out->mod_flags |= NV_HOTKEY_MOD_CMD_OR_CTRL;
    return 0;
  }
  if (nv_hk_ieq(t, "Command") || nv_hk_ieq(t, "Cmd") || nv_hk_ieq(t, "Meta") || nv_hk_ieq(t, "Super")) {
    out->mod_flags |= NV_HOTKEY_MOD_META;
    return 0;
  }
  return -1;
}

NV_INTERNAL int nv_hotkey_parse_combo(const char* combo, nv_hotkey_combo_t* out) {
  if (!combo || !out) return -1;
  memset(out, 0, sizeof(*out));
  char buf[256];
  size_t len = strlen(combo);
  if (len >= sizeof(buf)) return -1;
  memcpy(buf, combo, len + 1);
  trim_inplace(buf);
  if (buf[0] == '\0') return -1;

  char* parts[16];
  int np = 0;
  char* p = buf;
  while (np < (int)(sizeof(parts) / sizeof(parts[0]))) {
    char* plus = strchr(p, '+');
    if (plus) *plus = '\0';
    trim_inplace(p);
    if (*p) parts[np++] = p;
    if (!plus) break;
    p = plus + 1;
  }
  if (np < 1) return -1;

  int key_idx = -1;
  for (int i = 0; i < np; i++) {
    if (apply_mod(parts[i], out) != 0) {
      if (key_idx >= 0) return -1;
      if (tok_key(parts[i], out) != 0) return -1;
      key_idx = i;
    }
  }
  if (key_idx < 0) return -1;
  if (out->mod_flags == 0) return -1;
  return 0;
}
