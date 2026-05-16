# Agent Guidelines — nativeview

This is the master index for all AI agents working on this repository.
Read this file before reading any module-level `AGENTS.md`.

---

## Global Rules (apply to every module)

1. Do NOT change the LICENSE file.
2. Do NOT add bare `malloc`/`free` in new code — use arena allocators.
3. Do NOT add platform `#ifdef` blocks outside `modules/nv-platform-*/`.
4. All public functions must be NULL-safe.
5. C11, `-Wall -Wextra` clean — no new warnings.
6. Every new public function needs a unit test.
7. Max 800 lines per `.c` file — split if exceeded.

---

## Three Key Design Decisions

### 1. Modular Structure
Code lives in `modules/nv-*/`, not in the flat `src/` directory.
Each module has its own `CMakeLists.txt` and `AGENTS.md` and can be opened
standalone in an IDE. The dependency order is a strict DAG — no cycles.

```
nv-core → nv-ipc → nv-ops → nv-runtime → nv-platform-{mac,win,linux}
       → nv-http    └→ nv-ui → nv-designer
```

The old `src/` layout is kept during transition but new work goes in `modules/`.

### 2. VCL-Inspired Component Model (nv-ui)
The UI layer uses a Delphi VCL-inspired architecture adapted for C:
- **Owner/Parent duality**: owner frees memory, parent controls layout
- **Vtable in C**: `nv_vtable_t` struct of function pointers per component class
- **Reactive state**: `NV_STATE(type, name, default)` macro — changing state triggers a minimal DOM diff patch sent to the WebView, not a full re-render
- **No HWND per control**: controls are virtual, rendered to WebView DOM

### 3. Pure C Designer Output (nv-designer)
The visual designer writes standard `.c` files directly.
There is NO `.nvui` intermediate format and NO `nv-codegen` tool.

Two files per form:
- `myform_ui.c` — designer-owned, regenerated on save, never manually edited
- `myform.c` — user-owned, contains event handlers, never touched by designer

The designer round-trips by parsing between `NV_DESIGNER_BEGIN` and
`NV_DESIGNER_END` comment markers in `myform_ui.c`.

---

## Module Index

| Module | Status | Location | Depends On |
|---|---|---|---|
| `nv-core` | built | `modules/nv-core/` | nothing (C11 stdlib only) |
| `nv-http` | built | `modules/nv-http/` | nv-core |
| `nv-ipc` | built | `modules/nv-ipc/` | nv-core |
| `nv-ops` | built | `modules/nv-ops/` | nv-ipc |
| `nv-runtime` | built | `modules/nv-runtime/` | nv-ops |
| `nv-platform-mac` | built | `modules/nv-platform-mac/` | nv-runtime |
| `nv-platform-win` | built | `modules/nv-platform-win/` | nv-runtime |
| `nv-platform-linux` | built | `modules/nv-platform-linux/` | nv-runtime |
| `nv-ui` | planned | `modules/nv-ui/` | nv-ipc |
| `nv-designer` | planned | `modules/nv-designer/` | nv-ui |
| `nv-sdk` | planned | `modules/nv-sdk/` | all above |

---

## Module Scope Summary

### nv-core
Zero-dependency utility layer. Arena allocator, string helpers, JSON
parser/composer, utilities, structured logging. No platform code, no UI code.
See [`modules/nv-core/AGENTS.md`](modules/nv-core/AGENTS.md).

### nv-http
HTTP server: platform-neutral TCP sockets (WinSock2/POSIX), HTTP protocol,
routing, file serving. Uses arena allocators. See
[`modules/nv-http/AGENTS.md`](modules/nv-http/AGENTS.md).

### nv-ipc
IPC message routing between C and JavaScript. JSON-RPC dispatch, JS script
injection. Both `nv-runtime` and `nv-ui` depend on this module.
See [`modules/nv-ipc/AGENTS.md`](modules/nv-ipc/AGENTS.md).

### nv-ops
Native operation handlers: one file per capability (fs, dialog, clipboard,
shell, screen, tray, notification, window, windows, ipc-bus).
See [`modules/nv-ops/AGENTS.md`](modules/nv-ops/AGENTS.md).

### nv-runtime
App and window lifecycle. Wires the full stack. Platform backends link against
this module. Owns `nv_app_t`, `nv_window_t`, window manager.
See [`modules/nv-runtime/AGENTS.md`](modules/nv-runtime/AGENTS.md).

### nv-platform-mac / nv-platform-win / nv-platform-linux
Platform-specific WebView backends. Each implements the platform hooks
declared in `nv_core_internal.h` and `nv_window_internal.h`.
See the respective `AGENTS.md` in each module directory.

### nv-ui (planned)
Reactive component model. VCL-inspired owner/parent tree, vtable dispatch,
`NV_STATE` reactive macro, DOM diff engine. Depends on `nv-ipc`.

### nv-designer (planned)
RAD visual drag-and-drop tool. Outputs `myform_ui.c` files using the
`NV_DESIGNER_BEGIN/END` marker pattern. Depends on `nv-ui`.

### nv-sdk (planned)
Meta-package. Links all modules into a single convenient target for
application developers. Depends on all modules above.

---

## Licensing
Apache 2.0. See [`LICENSE`](LICENSE). A [CLA](CLA.md) is required for contributors.
Do NOT change the LICENSE file under any circumstances.
