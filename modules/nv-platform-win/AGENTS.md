# AGENTS.md — nv-platform-win

## Scope
Windows-only WebView2 backend. Implements the platform hooks declared in
`nv_core_internal.h` and `nv_window_internal.h`.

## Owns
- `src/nv_win.c` — Win32 + WebView2 COM backend, app/window lifecycle, clipboard, shell, screen
- `src/nv_win_notification.c` — WinRT desktop toast notifications (`ToastNotificationManager` / XML `LoadXml`)
- `src/nv_win_tray.c` — `Shell_NotifyIcon` tray integration
- `src/nv_win_dialog_*.c` — file and message dialogs

## Notifications (Windows)
- **Primary path**: WinRT `Windows.UI.Notifications` via `RoGetActivationFactory` / `RoActivateInstance` (C ABI headers from the Windows SDK). Toasts use `ToastGeneric` XML (title + body) built in UTF-8, then `IXmlDocumentIO::LoadXml`. The IPC `icon` field is reserved (not yet emitted as `appLogoOverride`). Each notification `id` is the toast **tag** so `notification.close` maps to `IToastNotificationHistory::Remove`.
- **Desktop requirements**: `SetCurrentProcessExplicitAppUserModelID` with AUMID `Elkotobi.NativeView`, plus a Start Menu `.lnk` under `%APPDATA%\Microsoft\Windows\Start Menu\Programs\NativeView.lnk` with `PKEY_AppUserModel_ID` (created on first use if missing). Without this, Windows may not show unpackaged desktop toasts.
- **Return codes**: `nv_win_notification_show` returns `0` on success, `-1` when the user or policy has disabled notifications for the app (`NotificationSetting` ≠ `Enabled`), and `-2` for other failures (IPC maps `-1` → `ERR_PERMISSION`, else → `ERR_IO`).
- **Fallbacks**: Shell tray balloon tips (`Shell_NotifyIcon`) and `MessageBox` are **not** used; only WinRT toasts (or a no-op / error when the SDK headers are unavailable, e.g. some MinGW setups without WinRT C headers).

## Allowed dependencies
- `nv-runtime` (and its full transitive chain)
- Win32 API: `windows.h`, `user32`, `ole32`, `shell32`, `shlwapi`, `propsys`, `runtimeobject` (WinRT)
- WebView2 SDK (COM interfaces)

## Forbidden
- Do NOT add cross-platform code here — use `nv-runtime` or `nv-ops`
- Do NOT use raw `malloc`/`free` for COM objects — use COM reference counting
- Do NOT change the LICENSE file
- Do NOT add macOS or Linux `#ifdef` blocks

## Coding rules
- C11, `-Wall -Wextra` clean
- All COM interfaces must be properly `Release()`d
- WebView2 environment creation is async — use the event handle pattern
- Prefer keeping `nv_win.c` focused on WebView2; add new Win32 surface area in additional `nv_win_*.c` units to stay under the repo file-size limits

## Task labels
- `[platform-win]`           — scoped to this module
- `[platform-win][webview2]` — WebView2 API changes
- `[platform-win][com]`      — COM interface changes
- `[platform-win][toast]`    — toast / WinRT notifications
