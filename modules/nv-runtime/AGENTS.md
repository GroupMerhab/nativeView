# AGENTS.md — nv-runtime

## Scope
App and window lifecycle. Wires the full stack together. Platform backends
link against this module to get the complete C API.

## Owns
- `src/nv_core.c`           — nv_app_t lifecycle (create/run/quit/destroy)
- `src/nv_window.c`         — nv_window_t management (create/show/hide/etc.)
- `src/nv_window_manager.c` — multi-window registry (lookup by id)
- `src/nv_window_manager.h` — internal header
- `tests/`                  — integration and unit tests

## Public headers (in repo root `include/`)
- `nv.h`           — the single user-facing header
- `nv_core.h`      — app lifecycle declarations
- `nv_window.h`    — window lifecycle declarations
- `nv_core_internal.h`   — platform hook declarations
- `nv_window_internal.h` — internal window struct

## Allowed dependencies
- `nv-ops` → `nv-ipc` → `nv-core`
- C11 stdlib

## Forbidden
- Do NOT call WebView or OS GUI APIs directly (that is nv-platform-* territory)
- Do NOT change `nv.h` without a version bump comment
- Do NOT change the LICENSE file
- Do NOT add platform `#ifdef` blocks here — use the weak-symbol hook pattern

## Coding rules
- C11, `-Wall -Wextra` clean
- All public API functions must be NULL-safe (guard against NULL inputs)
- ABI stability: never change the layout of `nv_app_t` or `nv_window_t`
  without a major version bump
- Platform hooks use `__attribute__((weak))` stubs so tests compile without
  a platform backend

## Task labels
- `[nv-runtime]`          — scoped to this module
- `[nv-runtime][abi]`     — changes that affect the public struct layout
- `[nv-runtime][lifecycle]` — changes to app/window state machine

## Future modules that depend on this one

- **`nv-ui`** (planned) — reactive component model built on top of the window
  lifecycle. Will introduce `nv_form_t`, `nv_widget_t`, and the VCL-inspired
  owner/parent component tree. `nv_form_t` wraps `nv_window_t` and owns all
  child components via an arena. When `nv-ui` is implemented, do NOT move
  window lifecycle code out of `nv-runtime` — `nv-ui` depends on `nv-runtime`,
  not the other way around.
- **`nv-designer`** (planned) — RAD visual tool that outputs `myform_ui.c`
  files. Depends on `nv-ui`. No changes to `nv-runtime` are required for the
  designer.
