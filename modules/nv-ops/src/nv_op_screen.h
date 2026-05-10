#ifndef NV_OP_SCREEN_H
#define NV_OP_SCREEN_H

#include "util/nv_log.h"
#include "nv_ipc.h"
#include "nv_json.h"
#include "nv_window.h"

NV_INTERNAL void nv_op_screen_get_all(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_screen_get_primary(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_screen_get_cursor(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);

#endif
