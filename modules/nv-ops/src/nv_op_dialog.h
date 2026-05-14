#ifndef NV_OP_DIALOG_H
#define NV_OP_DIALOG_H

#include "util/nv_log.h"
#include "nv_core_internal.h"
#include "nv_ipc.h"
#include "nv_json.h"
#include "nv_window.h"

NV_INTERNAL void nv_op_dialog_open_file(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_dialog_save_file(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_dialog_open_folder(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_dialog_message(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_dialog_confirm(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);

#endif
