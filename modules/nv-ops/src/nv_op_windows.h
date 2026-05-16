/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef NV_OP_WINDOWS_H
#define NV_OP_WINDOWS_H

#include "nv.h"
#include "nv_json.h"
#include "nv_arena.h"

/*
 * Handlers for windows.* operations dispatched from JS.
 * All handlers follow the signature:
 * void handler(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);
 */

NV_INTERNAL void nv_op_windows_push_all(const char* event, nv_json_t* data, nv_arena_t* arena);

NV_INTERNAL void nv_op_windows_open(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_windows_close(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_windows_focus(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_windows_list(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_windows_get_info(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_windows_set_title(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);

#endif // NV_OP_WINDOWS_H
