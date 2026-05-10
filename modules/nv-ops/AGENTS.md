# AGENTS.md — nv-ops

## Scope
Native operation handlers: one file per capability. Each op receives a JSON
payload from the IPC layer, performs the native action, and replies via IPC.

## Owns
- `src/nv_op_app.c/h`          — app lifecycle ops (quit, version)
- `src/nv_op_clipboard.c/h`    — read/write clipboard
- `src/nv_op_dialog.c/h`       — open/save file dialogs
- `src/nv_op_fs.c/h`           — file system read/write/list
- `src/nv_op_ipc_bus.c/h`      — cross-window message bus
- `src/nv_op_notification.c/h` — system notifications
- `src/nv_op_registry.c/h`     — op dispatch table (aggregates all ops)
- `src/nv_op_screen.c/h`       — screen/display info
- `src/nv_op_shell.c/h`        — shell command execution
- `src/nv_op_tray.c/h`         — system tray icon
- `src/nv_op_window.c/h`       — single-window ops (resize, title, `window.setContextMenu`, etc.)
- `src/nv_op_windows.c/h`      — multi-window management
- `tests/`                      — unit tests

## Allowed dependencies
- `nv-ipc` → `nv-core`
- `nv_window_internal.h`, `nv_window_manager.h`
- C11 stdlib + POSIX (for fs/shell ops)

## Forbidden
- Do NOT call WebView APIs directly (that is nv-platform-* territory)
- Do NOT add ops that bypass the IPC registry
- Do NOT change the LICENSE file
- Do NOT add synchronous blocking calls on the main thread

## Coding rules
- C11, `-Wall -Wextra` clean
- Each new op must be registered in `nv_op_registry.c`
- Each new op must have a test in `tests/test_op_<name>.c`
- Max 250 lines per op `.c` file
- Error replies must use the standard JSON error format: `{"error": "..."}`

## Task labels
- `[nv-ops]`           — scoped to this module
- `[nv-ops][new-op]`   — adding a new native capability
- `[nv-ops][security]` — changes to fs/shell/dialog (require extra review)
