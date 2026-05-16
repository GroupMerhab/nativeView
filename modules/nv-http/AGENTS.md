# AGENTS.md — nv-http

## Scope
HTTP server module: platform-neutral TCP sockets, HTTP parsing, routing, file serving.
Part of the industrial-grade HTTP server implementation for nativeView.

## Owns

### Prompt 1: Socket Layer
- `include/socket_layer.h`      — Public API (platform-neutral)
- `src/socket_layer_internal.h` — Internal structs, platform API
- `src/socket_layer_common.c`   — Wrappers, arena lifecycle, init/cleanup
- `src/platform/socket_layer_windows.c` — WinSock2 implementation
- `src/platform/socket_layer_posix.c`   — POSIX (Linux/macOS) implementation
- `tests/test_socket_layer.c`   — Socket unit tests

### Prompt 2: HTTP Protocol
- `include/http_protocol.h`    — Request/response structs, API
- `src/http_protocol.c`        — Parsing and serialization (arena-based)
- `tests/test_http_protocol.c` — Protocol unit tests

### Prompt 3: HTTP Core
- `include/http_core.h`        — Router, dispatch, server lifecycle
- `src/http_core.c`            — Request loop, routing (arena-based)
- `tests/test_http_core.c`     — Core unit tests

### Prompt 5: URL Parser
- `include/url_parser.h`       — Path, query, fragment parsing; encode/decode
- `src/url_parser.c`           — Implementation (no heap allocation)
- `tests/test_url_parser.c`    — Unit tests

## Dependencies
- nv-core (arena, util)

## Allowed
- C11 stdlib, platform sockets (WinSock2, POSIX)
- Arena allocators for all socket-related allocations (no bare malloc)

## Forbidden
- Bare malloc/free — use nv_arena_alloc, nv_arena_destroy
- Platform #ifdef in public headers
- External libraries

## Coding rules
- C11, `-Wall -Wextra` clean
- Max 800 lines per `.c` file
- Every public function must have a unit test

## Task labels
- `[nv-http]` — scoped to this module
- `[nv-http][socket]` — socket layer changes
