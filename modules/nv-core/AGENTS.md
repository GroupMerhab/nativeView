# AGENTS.md — nv-core

## Scope
Zero-dependency utility layer. No platform code. No UI code.
This is the foundation of the entire module DAG — every other module depends
on it directly or transitively. Keep it minimal and stable.

## Owns
- `src/nv_arena.c`  — pool/bump arena allocator
- `src/nv_str.c`    — string view helpers
- `src/nv_json.c`   — JSON parser and composer
- `src/nv_base64.c` — RFC 4648 base64 encode/decode (caller buffers; no heap)
- `src/nv_util.c`   — assert, panic, time utilities
- `src/nv_log.c`    — structured log output
- `tests/`          — unit tests for the above

## Public headers (in repo root `include/`)
- `nv_arena.h`, `nv_str.h`, `nv_json.h`, `nv_base64.h`, `nv_util.h`
- `util/nv_log.h`

## Allowed dependencies
- C11 stdlib only (`stdlib.h`, `string.h`, `stdio.h`, `math.h`, `time.h`)
- No other nv-* modules

## Forbidden
- Do NOT include any `nv_window*.h`, `nv_ipc*.h`, `nv_core*.h`
- Do NOT use platform-specific APIs (`#ifdef _WIN32`, `#ifdef __APPLE__`, etc.)
- Do NOT change the LICENSE file
- Do NOT add dynamic memory outside arena (no bare `malloc` in new code)

## Coding rules
- C11, `-Wall -Wextra` clean
- Every public function must have a unit test in `tests/`
- Max 300 lines per `.c` file
- No global mutable state except the log sink

## Task labels
- `[nv-core]` — changes scoped to this module only
- `[nv-core][breaking]` — changes that alter a public header
