/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef NV_OP_WINDOW_HOTKEY_H
#define NV_OP_WINDOW_HOTKEY_H

#include "nv_ipc.h"
#include "nv_json.h"
#include "nv_window.h"

NV_INTERNAL void nv_op_window_register_hotkey(nv_window_t* w, int seq, const nv_json_val_t* args,
                                             nv_arena_t* arena);
NV_INTERNAL void nv_op_window_unregister_hotkey(nv_window_t* w, int seq, const nv_json_val_t* args,
                                                 nv_arena_t* arena);

#endif
