#ifndef NV_OP_IPC_BUS_H
#define NV_OP_IPC_BUS_H

#include "nv.h"
#include "nv_json.h"
#include "nv_window.h"

// Routes messages between windows
NV_INTERNAL void nv_op_ipc_bus_send(nv_window_t* sender, int seq, const nv_json_val_t* args, nv_arena_t* arena);

#endif // NV_OP_IPC_BUS_H
