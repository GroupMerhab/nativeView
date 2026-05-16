/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

/* =============================================================================
 * nv_op_window_hotkey.c — IPC ops for global hotkeys
 * =============================================================================
 */

#include "nv_op_window_hotkey.h"
#include "nv_hotkey.h"
#include "nv_window_internal.h"
#include "nv.h"
#include "nv_arena.h"
#include <string.h>

NV_INTERNAL void nv_ipc_reply_ok(nv_window_t* w, int seq, nv_json_t* result, nv_arena_t* arena);
NV_INTERNAL void nv_ipc_reply_err(nv_window_t* w, int seq, const char* code, const char* message,
                                  nv_arena_t* arena);

static const char* require_str(const nv_json_val_t* args, const char* key) {
  return args ? nv_json_get_str(args, key) : NULL;
}

NV_INTERNAL void nv_op_window_register_hotkey(nv_window_t* w, int seq, const nv_json_val_t* args,
                                              nv_arena_t* arena) {
  if (!w) return;
  const char* id = require_str(args, "id");
  const char* combo = require_str(args, "combo");
  if (!id || id[0] == '\0') {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'id'", arena);
    return;
  }
  if (!combo || combo[0] == '\0') {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'combo'", arena);
    return;
  }
  int rc = nv_window_register_hotkey(w, id, combo);
  if (rc == NV_HOTKEY_ERR_ALREADY_EXISTS) {
    nv_ipc_reply_err(w, seq, "ERR_ALREADY_EXISTS", "hotkey id already registered", arena);
    return;
  }
  if (rc == NV_HOTKEY_ERR_NOT_SUPPORTED) {
    nv_ipc_reply_err(w, seq, "ERR_NOT_SUPPORTED", "global hotkeys not supported on this display", arena);
    return;
  }
  if (rc == NV_HOTKEY_ERR_PLATFORM) {
    nv_ipc_reply_err(w, seq, "ERR_IO", "system refused hotkey registration", arena);
    return;
  }
  if (rc != NV_HOTKEY_OK) {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "invalid hotkey id or combo", arena);
    return;
  }
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}

NV_INTERNAL void nv_op_window_unregister_hotkey(nv_window_t* w, int seq, const nv_json_val_t* args,
                                                nv_arena_t* arena) {
  if (!w) return;
  const char* id = require_str(args, "id");
  if (!id || id[0] == '\0') {
    nv_ipc_reply_err(w, seq, "ERR_INVALID_ARG", "missing or invalid 'id'", arena);
    return;
  }
  nv_window_unregister_hotkey(w, id);
  nv_ipc_reply_ok(w, seq, nv_json_object(arena), arena);
}
