# AGENTS.md — nv-platform-linux

## Scope
Linux-only WebKitGTK backend. Implements the platform hooks declared in
`nv_core_internal.h` and `nv_window_internal.h`.

## Owns
- `src/nv_linux.c` — GTK + WebKitGTK backend (GObject memory model)
- `src/nv_linux_tray.c` — system tray (`nv_linux_tray_*`): optional `ayatana-appindicator-0.1` or `appindicator3-0.1` (defines `NV_HAS_APPINDICATOR`); otherwise deprecated `GtkStatusIcon` (GTK 3). Symbols are weak so `nv-ops` tests can override with strong mocks.

## Allowed dependencies
- `nv-runtime` (and its full transitive chain)
- `webkit2gtk-4.1` (via pkg-config)
- GLib/GObject reference counting
- Optional: `libayatana-appindicator3` / `libappindicator3` (pkg-config `ayatana-appindicator-0.1` or `appindicator3-0.1`) for StatusNotifier/AppIndicator tray; if absent and `GtkStatusIcon` is unavailable at runtime, tray ops return `NV_TRAY_RC_NOT_SUPPORTED` (IPC `ERR_NOT_SUPPORTED`).
- Optional: `libnotify` (pkg-config `libnotify`, defines `NV_HAS_LIBNOTIFY`) for `notification.show` / `notification.close` via `org.freedesktop.Notifications`. If not found at build time, those ops return IPC `ERR_NOT_SUPPORTED`. `notify_init("nativeview")` runs once from `nv_app_platform_init`.
- **Known limitation:** `notification.clicked` is emitted only when the notification server advertises the `actions` capability and `notify_notification_add_action` succeeds; many environments omit `actions`, so click-to-activate may not surface as a JS event on Linux.

## Forbidden
- Do NOT add cross-platform code here — use `nv-runtime` or `nv-ops`
- Do NOT use bare `malloc`/`free` for GObject types — use `g_object_unref`
- Do NOT change the LICENSE file
- Do NOT add macOS or Windows `#ifdef` blocks

## Coding rules
- C11, `-Wall -Wextra` clean
- All GObject references must be balanced (`g_object_ref` / `g_object_unref`)
- GTK must run on the main thread — no cross-thread WebView calls
- Max 500 lines in `nv_linux.c`

## Task labels
- `[platform-linux]`          — scoped to this file
- `[platform-linux][webkit]`  — WebKitGTK API changes
- `[platform-linux][gtk]`     — GTK window management changes
- `[platform-linux][tray]`    — tray / status notifier
