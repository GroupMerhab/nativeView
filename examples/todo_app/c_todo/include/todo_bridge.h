/*
 * todo_bridge — Parse __nv IPC wire JSON and dispatch to todo_store; emit C→JS payloads.
 */

#ifndef TODO_BRIDGE_H
#define TODO_BRIDGE_H

#include "todo_store.h"

#include "nv_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*todo_bridge_emit_fn)(void* userdata, const char* event, const char* json);

/**
 * @param raw_ipc_json Full wire object {"e":"…","s":0,"d":{…}} from nv_ipc / window.__nv.send
 * @param work_arena    Arena for parse + compose (caller-owned; reset between calls is ok)
 */
int todo_bridge_handle_wire(todo_store_t* store, nv_arena_t* work_arena, const char* raw_ipc_json,
                            todo_bridge_emit_fn emit, void* emit_ud);

#ifdef __cplusplus
}
#endif

#endif /* TODO_BRIDGE_H */
