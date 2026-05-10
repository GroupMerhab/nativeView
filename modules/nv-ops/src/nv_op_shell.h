#ifndef NV_OP_SHELL_H
#define NV_OP_SHELL_H

#include "util/nv_log.h"
#include "nv_ipc.h"
#include "nv_json.h"
#include "nv_window.h"

NV_INTERNAL void nv_op_shell_open_url(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_shell_open_path(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_shell_show_in_folder(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);
NV_INTERNAL void nv_op_shell_exec(nv_window_t* w, int seq, const nv_json_val_t* args, nv_arena_t* arena);

#endif
