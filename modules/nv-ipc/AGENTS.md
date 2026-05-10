# AGENTS.md — nv-ipc

## Scope
IPC message routing between C and JavaScript. No platform code. No UI code.
This is a shared dependency: both `nv-runtime` (window lifecycle) and `nv-ui`
(reactive component model) depend on this module.

## Owns
- `src/nv_ipc.c`        — JSON-RPC dispatch, op handler registry
- `src/nv_ipc_script.c` — JS runtime injection helpers
- `src/nv_ipc_script.h` — internal header (not public API)
- `tests/`              — unit tests

## Public headers (in repo root `include/`)
- `nv_ipc.h`
- `nv_ipc_internal.h`

## Allowed dependencies
- `nv-core` (arena, json, util, log)
- `nv_window_internal.h` (read-only access to window handle for dispatch)
- C11 stdlib

## Forbidden
- Do NOT call platform APIs directly
- Do NOT include `nv_mac.m`, `nv_win.c`, `nv_linux.c` headers
- Do NOT change the LICENSE file
- Do NOT add new global mutable state without a lock comment

## Coding rules
- C11, `-Wall -Wextra` clean
- Binary frame protocol changes must update `docs/wire_protocol.md`
- Max 400 lines per `.c` file
- All new message types need a test in `tests/test_ipc.c`

## Task labels
- `[nv-ipc]` — scoped to this module
- `[nv-ipc][protocol]` — changes to the wire format (requires spec update)
- `[nv-ipc][shm]` — SharedArrayBuffer channel work (zero-copy high-frequency path)
