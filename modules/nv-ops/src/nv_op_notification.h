/* Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef NV_OP_NOTIFICATION_H
#define NV_OP_NOTIFICATION_H

#include "util/nv_log.h"
#include "nv_ipc.h"
#include "nv_json.h"
#include "nv_window.h"

NV_INTERNAL void nv_op_notification_show(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_notification_close(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_notification_clicked_push(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_notification_closed_push(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);

#endif
