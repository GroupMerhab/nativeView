/* =============================================================================
 * nv_hotkey.c — Global hotkey registration (public API + platform dispatch)
 * =============================================================================
 */

#include "nv_window_internal.h"
#include "nv_hotkey.h"
#include "nv.h"
#include "nv_arena.h"
#include <ctype.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

typedef struct nv_hotkey_cb_ctx {
  nv_window_t* window;
  const char* id;
} nv_hotkey_cb_ctx_t;

static _Atomic long long g_hotkey_next_handle = 1;

static void nv_hotkey_on_fired(long long handle, void* ctx) {
  (void)handle;
  nv_hotkey_cb_ctx_t* c = (nv_hotkey_cb_ctx_t*)ctx;
  if (!c || !c->window || !c->id) return;
  char buf[160];
  snprintf(buf, sizeof(buf), "{\"id\":\"%s\"}", c->id);
  nv_send(c->window, "window.hotkeyFired", buf);
}

static int validate_hotkey_id(const char* id) {
  if (!id || id[0] == '\0') return -1;
  size_t n = strlen(id);
  if (n > 64) return -1;
  for (size_t i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)id[i];
    if (isalnum(ch) || ch == '_' || ch == '.' || ch == '-') continue;
    return -1;
  }
  return 0;
}

static int find_hotkey_index(nv_window_t* w, const char* id) {
  if (!w || !id || !w->hotkey_slots) return -1;
  for (int i = 0; i < w->hotkey_slot_count; i++) {
    if (w->hotkey_slots[i].id && strcmp(w->hotkey_slots[i].id, id) == 0) return i;
  }
  return -1;
}

static void hotkey_remove_at(nv_window_t* w, int idx) {
  if (!w || !w->hotkey_slots || idx < 0 || idx >= w->hotkey_slot_count) return;
  int tail = w->hotkey_slot_count - idx - 1;
  if (tail > 0) {
    memmove(&w->hotkey_slots[idx], &w->hotkey_slots[idx + 1],
            (size_t)tail * sizeof(nv_hotkey_slot_t));
  }
  w->hotkey_slot_count--;
}

NV_INTERNAL void nv_hotkey_detach_for_window(nv_window_t* w) {
  if (!w || !w->hotkey_slots || w->hotkey_slot_count <= 0) return;
  for (int i = 0; i < w->hotkey_slot_count; i++) {
    long long h = w->hotkey_slots[i].handle;
    nv_mac_unregister_hotkey(h);
    nv_win_unregister_hotkey(h);
    nv_linux_unregister_hotkey(h);
  }
  w->hotkey_slot_count = 0;
  w->hotkey_slots = NULL;
}

NV_API int nv_window_register_hotkey(nv_window_t* w, const char* id, const char* combo) {
  if (!w || !id || !combo) return NV_HOTKEY_ERR_INVALID;
  if (validate_hotkey_id(id) != 0) return NV_HOTKEY_ERR_INVALID;
  if (find_hotkey_index(w, id) >= 0) return NV_HOTKEY_ERR_ALREADY_EXISTS;

  nv_hotkey_combo_t parsed;
  if (nv_hotkey_parse_combo(combo, &parsed) != 0) return NV_HOTKEY_ERR_INVALID;

  nv_arena_t* ar = nv_window_get_arena(w);
  if (!ar) return NV_HOTKEY_ERR_INVALID;

  nv_hotkey_cb_ctx_t* ctx = (nv_hotkey_cb_ctx_t*)nv_arena_alloc(ar, sizeof(nv_hotkey_cb_ctx_t));
  const char* id_copy = nv_arena_str_dup(ar, id);
  if (!ctx || !id_copy) return NV_HOTKEY_ERR_INVALID;

  long long handle = atomic_fetch_add_explicit(&g_hotkey_next_handle, 1, memory_order_relaxed);
  if (handle == 0) handle = atomic_fetch_add_explicit(&g_hotkey_next_handle, 1, memory_order_relaxed);

  ctx->window = w;
  ctx->id = id_copy;

  int rm = nv_mac_register_hotkey(w, handle, &parsed, nv_hotkey_on_fired, ctx);
  int rw = nv_win_register_hotkey(w, handle, &parsed, nv_hotkey_on_fired, ctx);
  int rl = nv_linux_register_hotkey(w, handle, &parsed, nv_hotkey_on_fired, ctx);
  if (rm != 0 && rw != 0 && rl != 0) {
    if (rm == NV_HOTKEY_RC_NOT_SUPPORTED || rw == NV_HOTKEY_RC_NOT_SUPPORTED ||
        rl == NV_HOTKEY_RC_NOT_SUPPORTED) {
      return NV_HOTKEY_ERR_NOT_SUPPORTED;
    }
    return NV_HOTKEY_ERR_PLATFORM;
  }

  int n = w->hotkey_slot_count;
  nv_hotkey_slot_t* grown =
      (nv_hotkey_slot_t*)nv_arena_alloc(ar, (size_t)(n + 1) * sizeof(nv_hotkey_slot_t));
  if (!grown) {
    nv_mac_unregister_hotkey(handle);
    nv_win_unregister_hotkey(handle);
    nv_linux_unregister_hotkey(handle);
    return NV_HOTKEY_ERR_PLATFORM;
  }
  if (w->hotkey_slots && n > 0) {
    memcpy(grown, w->hotkey_slots, (size_t)n * sizeof(nv_hotkey_slot_t));
  }
  grown[n].handle = handle;
  grown[n].id = id_copy;
  w->hotkey_slots = grown;
  w->hotkey_slot_count = n + 1;
  return NV_HOTKEY_OK;
}

NV_API void nv_window_unregister_hotkey(nv_window_t* w, const char* id) {
  if (!w || !id) return;
  int idx = find_hotkey_index(w, id);
  if (idx < 0) return;
  long long h = w->hotkey_slots[idx].handle;
  nv_mac_unregister_hotkey(h);
  nv_win_unregister_hotkey(h);
  nv_linux_unregister_hotkey(h);
  hotkey_remove_at(w, idx);
}
