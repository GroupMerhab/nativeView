# NativeView — AI Agent Implementation Prompts

> **Team-internal.** This file contains AI implementation prompts for maintainers.
> For human contributors, start with [CONTRIBUTING.md](../CONTRIBUTING.md) and
> [docs/index.md](index.md).

## Global Rules (apply to every prompt below)

- **Language**: C11, `-Wall -Wextra` clean, zero UB, zero memory leaks
- **Memory**: Use arena allocators (`nv_arena_alloc`). No bare `malloc`/`free` in op
  handlers. Platform hook functions that must return heap strings (OS APIs that
  allocate) may use `malloc`; callers must `free()` them immediately after copying
  into the arena.
- **File size**: No `.c` file may exceed **800 lines**. Split into
  `_part1.c` / `_part2.c` before reaching the limit.
- **Platform isolation**: Platform-specific code belongs only in
  `modules/nv-platform-{mac,win,linux}/src/`. Op handlers in `nv-ops` call
  platform hooks via forward-declared `NV_INTERNAL` function pointers guarded by
  `#ifdef __APPLE__` / `#ifdef _WIN32` / `#else` blocks.
- **Tests first**: Write or update the test for the feature **before** running it.
  Do not mark a prompt done until `ctest --test-dir build -R <test_name>` passes.
  Do not start the next prompt while any test is failing.
- **Error replies**: Every op that cannot complete must call
  `nv_ipc_reply_err(w, seq, "ERR_CODE", "human message", arena)`. Never silently
  succeed when nothing happened.
- **Docs**: After each prompt, update `docs/project_structure.md` if new files
  were added and `docs/Implementation.md` if a stage item is now complete.
- **AGENTS.md**: If you add new platform hook symbols, list them in the relevant
  `modules/nv-platform-*/AGENTS.md`.
- **No #ifdef blocks in nv-ops**: op files must forward-declare all three
  platform symbols and call them inside `#ifdef` guards at the call site only.

---

## P-00 — Convert TODO-platform Stubs to Honest Errors

**Goal**: Replace every silent-success stub with `ERR_NOT_IMPLEMENTED` so broken
ops are discoverable at runtime instead of silently lying.

**Files to modify**:
- `modules/nv-ops/src/nv_op_tray.c`
- `modules/nv-ops/src/nv_op_notification.c`
- `modules/nv-ops/src/nv_op_screen.c`
- `modules/nv-ops/src/nv_op_shell.c` (only `shell.exec`)
- `modules/nv-ops/src/nv_op_clipboard.c` (only `readImage`, `writeImage`, `hasImage`)

**Instructions**:
1. For every function body that contains `NV_LOG("...: TODO platform")` and then
   calls `nv_ipc_reply_ok`, replace with:
   ```c
   nv_ipc_reply_err(w, seq, "ERR_NOT_IMPLEMENTED",
                    "<op_name> not implemented on this platform", arena);
   return;
   ```
2. Remove the fake event pushes that immediately fire `tray.clicked` on
   `tray.create` and `notification.clicked` on `notification.show`. These
   fabricated events are the most dangerous part of the current stubs.
3. `nv_op_tray_set_menu`: remove the fake `tray.menuAction` fire.
4. `nv_op_notification_close`: remove the fake `notification.closed` fire.
5. `nv_op_shell_exec`: keep the metacharacter guard; change the body after it to
   `nv_ipc_reply_err(...)`.
6. `nv_op_screen_get_all/get_primary/get_cursor`: replace with
   `nv_ipc_reply_err(...)`.

**Tests**: Update `modules/nv-ops/tests/test_op_tray.c`,
`test_op_notification.c`, `test_op_screen.c` — assert that calling the stubbed
ops with valid args now returns a JSON object containing `"error"` key (not a
success response).

---

## P-01 — Windows Clipboard Text

**Goal**: Implement clipboard read/write/clear/hasText on Windows in `nv_win.c`.

**Files to modify**:
- `modules/nv-platform-win/src/nv_win.c` — add implementations
- `modules/nv-ops/src/nv_op_clipboard.c` — add `#ifdef _WIN32` call sites

**New symbols to add in `nv_win.c`**:
```c
NV_INTERNAL char* nv_win_clipboard_read_text(void);   /* returns malloc'd UTF-8; caller frees */
NV_INTERNAL int   nv_win_clipboard_write_text(const char* utf8);
NV_INTERNAL void  nv_win_clipboard_clear(void);
NV_INTERNAL int   nv_win_clipboard_has_text(void);
```

**Implementation notes**:
- Use `OpenClipboard(NULL)` / `GetClipboardData(CF_UNICODETEXT)` /
  `CloseClipboard()`.
- Convert UTF-16 `WCHAR*` → UTF-8 via `WideCharToMultiByte(CP_UTF8, ...)`.
  `malloc` the UTF-8 buffer; caller `free`s immediately after
  `nv_arena_str_dup`.
- Write: `EmptyClipboard()` + `SetClipboardData(CF_UNICODETEXT, hGlobal)` with
  `GlobalAlloc(GMEM_MOVEABLE, ...)`.
- `has_text`: `IsClipboardFormatAvailable(CF_UNICODETEXT)`.

**Call sites** in `nv_op_clipboard.c` — add under existing `#ifdef __APPLE__`
blocks:
```c
#elif defined(_WIN32)
  char* t = nv_win_clipboard_read_text();
  ...
```

**Tests**: Add `test_op_clipboard_win` section in `test_op_clipboard.c` (or a
new `test_clipboard_win.c`) that stubs the platform hooks and verifies the op
handler packs and replies correctly. The test compiles on all platforms; the
actual Win32 calls are only exercised in Windows CI.

---

## P-02 — Linux Clipboard Text

**Goal**: Implement clipboard read/write/clear/hasText on Linux using GTK.

**Files to modify**:
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_clipboard.c`

**New symbols**:
```c
NV_INTERNAL char* nv_linux_clipboard_read_text(void);
NV_INTERNAL int   nv_linux_clipboard_write_text(const char* utf8);
NV_INTERNAL void  nv_linux_clipboard_clear(void);
NV_INTERNAL int   nv_linux_clipboard_has_text(void);
```

**Implementation notes**:
- Use `gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)`.
- Read: `gtk_clipboard_wait_for_text(cb)` — returns `g_malloc`'d string; copy
  to `malloc` buffer so caller can `free()` uniformly.
- Write: `gtk_clipboard_set_text(cb, utf8, -1)`.
- Clear: `gtk_clipboard_clear(cb)`.
- Has text: `gtk_clipboard_wait_is_text_available(cb)`.
- These calls require the GTK main thread. They are safe to call from op
  handlers because ops are dispatched on the main thread.

**Call sites**: add `#else` branch in `nv_op_clipboard.c` after `_WIN32`.

**Tests**: Same pattern as P-01 — mock-based unit test that verifies op handler
behaviour without requiring a real clipboard.

---

## P-03 — Windows Shell Ops (openUrl, openPath, showInFolder)

**Goal**: Implement `shell.openUrl`, `shell.openPath`, `shell.showInFolder` on
Windows.

**Files to modify**:
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-ops/src/nv_op_shell.c`

**New symbols**:
```c
NV_INTERNAL int nv_win_shell_open_url(const char* url);
NV_INTERNAL int nv_win_shell_open_path(const char* path);
NV_INTERNAL int nv_win_shell_show_in_folder(const char* path);
```

**Implementation notes**:
- `open_url` / `open_path`: `ShellExecuteW(NULL, L"open", wpath, NULL, NULL, SW_SHOWNORMAL)`.
  Convert UTF-8 arg to `WCHAR` via `MultiByteToWideChar` on the stack (MAX_PATH).
- `show_in_folder`: `SHOpenFolderAndSelectItems` (preferred) or fall back to
  opening the parent folder via `ShellExecuteW`.
- Return 0 on success, -1 on error (`ShellExecuteW` returns > 32 on success).

**Call sites**: add `#elif defined(_WIN32)` in `nv_op_shell.c`.

**Tests**: Add cases to `modules/nv-ops/tests/test_op_shell.c` testing that
valid args route to the platform hook and invalid args return `ERR_INVALID_ARG`
without calling the hook.

---

## P-04 — Linux Shell Ops (openUrl, openPath, showInFolder)

**Goal**: Implement `shell.openUrl`, `shell.openPath`, `shell.showInFolder` on
Linux.

**Files to modify**:
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_shell.c`

**New symbols**:
```c
NV_INTERNAL int nv_linux_shell_open_url(const char* url);
NV_INTERNAL int nv_linux_shell_open_path(const char* path);
NV_INTERNAL int nv_linux_shell_show_in_folder(const char* path);
```

**Implementation notes**:
- Use `gtk_show_uri_on_window(NULL, uri, GDK_CURRENT_TIME, NULL)` (GTK 3.22+).
- Fallback: `g_app_info_launch_default_for_uri(uri, NULL, NULL)`.
- `show_in_folder`: open the parent directory URI (`file:///parent/dir`).
- All three ultimately call the same `gtk_show_uri` wrapper with a
  `file://` or `http://` URI.

**Call sites**: add `#else` in `nv_op_shell.c`.

**Tests**: Same pattern as P-03.

---

## P-05 — Shell Exec (All Platforms)

**Goal**: Implement `shell.exec` on macOS, Windows, and Linux. Run a command,
capture stdout/stderr, reply with `{ exitCode, stdout, stderr }`.

**Files to modify**:
- `modules/nv-platform-mac/src/nv_mac.m`
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_shell.c`

**New symbols** (all platforms):
```c
/* Returns malloc'd nv_shell_result_t; caller frees struct and its char* fields */
typedef struct { int exit_code; char* out; char* err; } nv_shell_result_t;
NV_INTERNAL nv_shell_result_t* nv_mac_shell_exec(const char* cmd);
NV_INTERNAL nv_shell_result_t* nv_win_shell_exec(const char* cmd);
NV_INTERNAL nv_shell_result_t* nv_linux_shell_exec(const char* cmd);
```

**Implementation notes**:
- macOS / Linux: `popen`-based double-pipe pattern or `posix_spawn` +
  `pipe` pairs to capture both stdout and stderr separately.
  Read into a growing `malloc` buffer; return struct.
- Windows: `CreatePipe` + `CreateProcessW` + `ReadFile` loop.
- The **metacharacter guard** in `nv_op_shell_exec` already rejects shell
  injection unless `NV_ALLOW_SHELL_EXEC` is defined; keep it. Do not bypass it.
- Op handler: call platform hook, copy strings into arena via
  `nv_arena_str_dup`, free the struct and its `char*` fields, then reply.
- If output exceeds 1 MB, truncate and add `"truncated": true` to the reply.

**Tests**:
- Add `test_op_shell_exec` in `modules/nv-ops/tests/test_op_shell.c`.
- Test: metacharacter rejection, missing `cmd` arg, NULL args.
- Integration test (macOS/Linux CI): run `echo hello`, assert `exitCode == 0`
  and `stdout == "hello\n"`.

---

## P-06 — Windows Dialog Ops

**Goal**: Implement all five async dialog ops on Windows using the Common Item
Dialog COM API.

**Files to modify**:
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-ops/src/nv_op_dialog.c`

**New symbols**:
```c
NV_INTERNAL void nv_win_dialog_open_file_async(int allow_multiple, nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
NV_INTERNAL void nv_win_dialog_save_file_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
NV_INTERNAL void nv_win_dialog_open_folder_async(nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
NV_INTERNAL void nv_win_dialog_message_async(const char* title, const char* body,
    const char* type, const char** buttons, size_t btn_count,
    nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
NV_INTERNAL void nv_win_dialog_confirm_async(const char* title, const char* body,
    nv_dialog_ctx_t* ctx, nv_dialog_cb_t cb);
```

**Implementation notes**:
- File / folder: `IFileOpenDialog` / `IFileSaveDialog` / `IFileOpenDialog` with
  `FOS_PICKFOLDERS`. These are COM calls — must run on an STA thread. Since nv
  runs a Win32 message loop on the main thread (which is already COM STA after
  `CoInitializeEx`), call them directly. The dialogs are modal-by-design and
  block the thread while open. Call the callback before returning from the
  function (synchronous async — same pattern as macOS `@autoreleasepool` blocks).
- Message / confirm: `MessageBoxW` with appropriate `uType` flags.
- Convert all `WCHAR*` results to UTF-8 with `WideCharToMultiByte`; `malloc` the
  UTF-8 string; pass to callback as `result`; callback frees.
- If the dialog module grows beyond 800 lines, split into
  `nv_win_dialog_file.c` and `nv_win_dialog_message.c`.

**Call sites**: add `#elif defined(_WIN32)` in each `nv_op_dialog_*` function.

**Tests**: Add `modules/nv-ops/tests/test_op_dialog.c` with mock-hook tests for
missing `title` / `body` arg validation and cancellation path.

---

## P-07 — Linux Dialog Ops

**Goal**: Implement all five async dialog ops on Linux using GTK.

**Files to modify**:
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_dialog.c`

**New symbols**: Same signatures as P-06 but prefixed `nv_linux_`.

**Implementation notes**:
- File dialogs: `GtkFileChooserDialog` with `GTK_FILE_CHOOSER_ACTION_OPEN` /
  `_SAVE` / `_SELECT_FOLDER`. Run with `gtk_dialog_run()` which blocks the
  GTK main loop (acceptable — same behaviour as macOS sheet).
- Multi-select: `gtk_file_chooser_set_select_multiple(TRUE)` +
  `gtk_file_chooser_get_filenames()` returns a `GSList*`; convert to the same
  `char**` + count format the macOS impl uses, then `g_free` the GSList.
- Message: `GtkMessageDialog` with appropriate `GtkMessageType`.
- Confirm: `GtkMessageDialog` with `GTK_BUTTONS_YES_NO`.
- All strings passed to the callback must be `malloc`'d UTF-8 so the callback
  can `free()` them uniformly.

**Call sites**: add `#else` in `nv_op_dialog.c`.

**Tests**: Same pattern as P-06.

---

## P-08 — Windows App Paths & Locale

**Goal**: Implement `app.getDataDir`, `app.getExePath`, `app.getResourceDir`,
`app.getLocale` on Windows.

**Files to modify**:
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-ops/src/nv_op_app.c`

**New symbols**:
```c
NV_INTERNAL char* nv_win_get_data_dir(void);       /* %APPDATA%\<AppName> */
NV_INTERNAL char* nv_win_get_exe_path(void);        /* full path to .exe */
NV_INTERNAL char* nv_win_get_resource_dir(void);    /* directory containing .exe */
NV_INTERNAL char* nv_win_get_locale(void);          /* e.g. "en-US" */
```

**Implementation notes**:
- `get_exe_path`: `GetModuleFileNameW(NULL, buf, MAX_PATH)` → UTF-8.
- `get_resource_dir`: same as exe path but strip filename.
- `get_data_dir`: `SHGetFolderPathW(NULL, CSIDL_APPDATA, ...)` +
  `\\nativeview` suffix; create dir if absent.
- `get_locale`: `GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH)` →
  UTF-8 (already BCP-47 format).
- All return `malloc`'d UTF-8; callers `free()` immediately.

**Call sites**: add `#elif defined(_WIN32)` in `nv_op_app.c`.

**Tests**: Add to `modules/nv-ops/tests/test_op_app.c`. Mock the platform hook,
verify the reply JSON has a non-empty `"path"` / `"locale"` field.

---

## P-09 — Linux App Paths & Locale

**Goal**: Implement `app.getDataDir`, `app.getExePath`, `app.getResourceDir`,
`app.getLocale` on Linux.

**Files to modify**:
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_app.c`

**New symbols**: Same signatures as P-08 prefixed `nv_linux_`.

**Implementation notes**:
- `get_exe_path`: `readlink("/proc/self/exe", buf, PATH_MAX)`.
- `get_resource_dir`: `dirname` of exe path.
- `get_data_dir`: `$XDG_DATA_HOME` (or `$HOME/.local/share`) + `/nativeview`.
  Create with `mkdir -p` semantics.
- `get_locale`: `setlocale(LC_MESSAGES, "")` returns a POSIX locale string.
  Strip encoding suffix (`en_US.UTF-8` → `en-US`), replace `_` with `-`.

**Call sites**: `#else` branch in `nv_op_app.c`.

**Tests**: Same pattern as P-08.

---

## P-10 — Screen Info — macOS

**Goal**: Implement `screen.getAll`, `screen.getPrimary`, `screen.getCursorPos`
on macOS.

**Files to modify**:
- `modules/nv-platform-mac/src/nv_mac.m`
- `modules/nv-ops/src/nv_op_screen.c`

**New symbols**:
```c
/* Each returns malloc'd JSON string: "{\"x\":0,\"y\":0,\"width\":1920,...}" */
NV_INTERNAL char* nv_mac_screen_get_all(void);
NV_INTERNAL char* nv_mac_screen_get_primary(void);
NV_INTERNAL char* nv_mac_screen_get_cursor(void);
```

**Implementation notes**:
- `get_all`: iterate `[NSScreen screens]`. For each: `frame`, `backingScaleFactor`,
  `localizedName`. Build a JSON array string in a stack `char` buffer (use
  `snprintf` loops); `malloc`-copy the final string.
- `get_primary`: `[NSScreen mainScreen]`.
- `get_cursor`: `[NSEvent mouseLocation]` (flipped Y — AppKit uses bottom-left
  origin; convert to top-left).
- Op handler: call hook, parse the returned JSON string with `nv_json_parse`,
  reply with the parsed object. Then `free` the hook's string.

**Alternatively** (simpler): have each hook fill a caller-supplied
`nv_arena_t*` and return `nv_json_t*` directly. Choose one pattern and use it
consistently across all three platforms.

**Tests**: Add `modules/nv-ops/tests/test_op_screen.c`. Mock hook returns known
JSON; assert op reply fields are correct.

---

## P-11 — Screen Info — Windows

**Goal**: Same as P-10 for Windows.

**Files to modify**:
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-ops/src/nv_op_screen.c`

**New symbols**: `nv_win_screen_get_all`, `nv_win_screen_get_primary`,
`nv_win_screen_get_cursor`.

**Implementation notes**:
- `get_all`: `EnumDisplayMonitors(NULL, NULL, monitorEnumProc, ...)`. In the
  callback, call `GetMonitorInfoW` to retrieve `MONITORINFOEX.rcMonitor` and
  `rcWork`. Scale factor via `GetDpiForMonitor` (shcore.dll; dynamically load
  to keep Win7 compat).
- `get_primary`: filter where `MONITORINFOEX.dwFlags & MONITORINFOF_PRIMARY`.
- `get_cursor`: `GetCursorPos(&pt)`.

---

## P-12 — Screen Info — Linux

**Goal**: Same as P-10 for Linux.

**Files to modify**:
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_screen.c`

**New symbols**: `nv_linux_screen_get_all`, `nv_linux_screen_get_primary`,
`nv_linux_screen_get_cursor`.

**Implementation notes**:
- Use `GdkDisplay` / `GdkScreen` (GTK3) or `GdkSeat` cursor position.
- `gdk_display_get_default()` + `gdk_display_get_n_monitors()` +
  `gdk_display_get_monitor(display, i)`.
- `gdk_monitor_get_geometry()`, `gdk_monitor_get_scale_factor()`.
- Primary: `gdk_display_get_primary_monitor()`.
- Cursor: `gdk_display_get_pointer(display, NULL, &x, &y, NULL)`.

---

## P-13 — Tray Icon — macOS

**Goal**: Implement all five tray ops using `NSStatusBar` on macOS. This is
the highest-priority real feature — it validates the hook pattern for P-14/P-15.

**Files to modify**:
- `modules/nv-platform-mac/src/nv_mac.m`
- `modules/nv-ops/src/nv_op_tray.c`

**Internal state** (in `nv_mac.m`, file-scope static array):
```c
#define NV_MAX_TRAYS 8
typedef struct {
    long long id;
    NSStatusItem* item;   /* strong ref retained by status bar */
    nv_window_t* window;  /* for event callbacks */
} nv_mac_tray_t;
static nv_mac_tray_t g_trays[NV_MAX_TRAYS];
static int g_tray_count = 0;
```

**New symbols**:
```c
NV_INTERNAL int  nv_mac_tray_create(long long id, const char* icon_path,
                                     const char* tooltip, nv_window_t* w);
NV_INTERNAL int  nv_mac_tray_destroy(long long id);
NV_INTERNAL int  nv_mac_tray_set_icon(long long id, const char* icon_path);
NV_INTERNAL int  nv_mac_tray_set_tooltip(long long id, const char* tooltip);
NV_INTERNAL int  nv_mac_tray_set_menu(long long id,
                                       const char** labels, const long long* item_ids,
                                       int count);
```

**Implementation notes**:
- `create`: `[[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength]`.
  Set `button.image` from `icon_path` (load with `[NSImage imageWithContentsOfFile:]`).
  Set `button.toolTip`. Add click target/action via `NSStatusBarButton.action`.
- Click callback: look up tray by `NSStatusItem`, call
  `nv_send(window, "tray.clicked", payload)` with `{"id": id}`.
- `set_menu`: build `NSMenu` from label/id arrays. Menu item action sends
  `nv_send(window, "tray.menuAction", {"id":id, "itemId":item_id})`.
- `destroy`: `[[NSStatusBar systemStatusBar] removeStatusItem:item]`, remove
  from `g_trays`.
- Op handler in `nv_op_tray.c`: call appropriate platform hook under
  `#ifdef __APPLE__`. On success reply ok; on non-zero return reply err.
- **Menu item struct**: pass parallel `const char** labels` and
  `const long long* item_ids` arrays. Parse from the JSON `items` array in the
  op handler before calling the hook.

**Tests**:
- `modules/nv-ops/tests/test_op_tray.c`: update existing tests to assert
  `ERR_NOT_IMPLEMENTED` is gone and a success reply is returned when the mock
  hook returns 0.
- Separate manual smoke test in `examples/` is acceptable (no headless test for
  NSStatusItem).

---

## P-14 — Tray Icon — Windows

**Goal**: Implement tray ops using `Shell_NotifyIconW` on Windows.

**Files to modify**:
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-ops/src/nv_op_tray.c`

**New symbols**: Same signatures as P-13 prefixed `nv_win_`.

**Implementation notes**:
- Use `NOTIFYICONDATAW`. `uCallbackMessage = WM_NV_TRAY_MSG` (define a custom
  `WM_USER + N` constant).
- Handle `WM_NV_TRAY_MSG` in the existing `WndProc`. On `NIN_SELECT` or
  `WM_LBUTTONUP` call `nv_send` for `tray.clicked`.
- `set_menu`: build `HMENU` with `CreatePopupMenu` + `AppendMenuW`. Track item
  IDs so `WM_COMMAND` in `WndProc` can look up the tray id and item id.
- Icons: load with `LoadImageW` → `HICON`. Convert from file path.
- Tooltip: `szTip` field in `NOTIFYICONDATAW` (max 128 chars).
- Store state in a file-scope `nv_win_tray_t g_trays[NV_MAX_TRAYS]` array
  (same pattern as macOS).

**Call sites**: add `#elif defined(_WIN32)` in `nv_op_tray.c`.

**Tests**: Same mock-based approach as P-13.

---

## P-15 — Tray Icon — Linux

**Goal**: Implement tray ops on Linux. Use `libayatana-appindicator3` if
present; fall back to `GtkStatusIcon` (deprecated but widely available).

**Files to modify**:
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_tray.c`
- `modules/nv-platform-linux/CMakeLists.txt` — add optional `libayatana-appindicator3`
  find_package; set `NV_HAS_APPINDICATOR` compile definition when found.

**New symbols**: Same signatures as P-13 prefixed `nv_linux_`.

**Implementation notes**:
- `#ifdef NV_HAS_APPINDICATOR`: use `AppIndicator`. `app_indicator_new()`,
  `app_indicator_set_icon_full()`, `app_indicator_set_menu()`.
- `#else`: use `GtkStatusIcon` (fallback). `gtk_status_icon_new_from_file()`,
  `gtk_status_icon_set_tooltip_text()`, `gtk_status_icon_connect_activate()`.
- For both paths: build `GtkMenu` from label/id arrays. Connect `activate`
  signal to a handler that calls `nv_send` for `tray.menuAction`.
- Return `ERR_NOT_SUPPORTED` if neither library is available (check at runtime
  or compile-time). Do not crash.
- Document the dependency in `modules/nv-platform-linux/AGENTS.md`.

---

## P-16 — Notifications — macOS

**Goal**: Implement `notification.show` and `notification.close` on macOS
using `UNUserNotificationCenter` (macOS 10.14+) with `NSUserNotification`
fallback.

**Files to modify**:
- `modules/nv-platform-mac/src/nv_mac.m`
- `modules/nv-ops/src/nv_op_notification.c`

**New symbols**:
```c
NV_INTERNAL int  nv_mac_notification_show(long long id, const char* title,
                                           const char* body, const char* icon_path,
                                           nv_window_t* w);
NV_INTERNAL int  nv_mac_notification_close(long long id);
```

**Implementation notes**:
- Use `UNUserNotificationCenter` (10.14+). Request authorization once
  (`requestAuthorizationWithOptions:completionHandler:`). On denial, return -1.
- `show`: create `UNMutableNotificationContent`, set `title`, `body`.
  Use a string representation of `id` as the notification identifier.
  `UNUserNotificationCenterDelegate` method
  `userNotificationCenter:didReceiveNotificationResponse:` fires `nv_send` for
  `notification.clicked`.
  `userNotificationCenter:willPresentNotification:` fires `notification.shown`
  (if needed).
- `close`: `removePendingNotificationRequestsWithIdentifiers` +
  `removeDeliveredNotificationsWithIdentifiers`.
- Store `nv_window_t*` in a file-scope map keyed by notification id for the
  delegate callback.

**Call sites**: add `#ifdef __APPLE__` in `nv_op_notification.c`.

**Tests**: Add `modules/nv-ops/tests/test_op_notification.c` mock test. Assert
success reply format and that no fake events are fired.

---

## P-17 — Notifications — Windows

**Goal**: Implement `notification.show` / `notification.close` on Windows using
WinRT Toast Notifications.

**Files to modify**:
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-ops/src/nv_op_notification.c`

**New symbols**: Same signatures as P-16 prefixed `nv_win_`.

**Implementation notes**:
- Use the WinRT `ToastNotification` API via COM. The simplest approach without
  a C++/CX dependency is to use the XML-based API:
  `Windows::UI::Notifications::ToastNotificationManager::CreateToastNotifier`.
- Since this requires COM/WinRT, wrap in a dedicated file
  `nv_win_notification.c` (separate from `nv_win.c` to respect 800-line limit).
- Alternative simpler fallback: use `Shell_NotifyIcon` balloon tips
  (`NIM_MODIFY` with `NIIF_INFO`) if the tray icon exists, or a basic
  `MessageBox` async variant. Document the chosen approach in
  `modules/nv-platform-win/AGENTS.md`.
- `notification.close`: WinRT provides `ToastNotificationHistory.Remove`.

**File split**: if `nv_win.c` would exceed 800 lines, move notification and
tray code to `nv_win_notification.c` and `nv_win_tray.c` respectively.

---

## P-18 — Notifications — Linux

**Goal**: Implement `notification.show` / `notification.close` on Linux via
`libnotify` (D-Bus org.freedesktop.Notifications).

**Files to modify**:
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_notification.c`
- `modules/nv-platform-linux/CMakeLists.txt` — `find_package(libnotify)`

**New symbols**: Same signatures as P-16 prefixed `nv_linux_`.

**Implementation notes**:
- `notify_init("nativeview")` once at app init.
- `show`: `notify_notification_new(title, body, icon_or_null)` +
  `notify_notification_show()`. Store `NotifyNotification*` in a file-scope
  map keyed by `id`.
- `close`: `notify_notification_close()` + remove from map.
- `clicked` signal via `notify_notification_add_action` if the desktop supports
  it; otherwise no `notification.clicked` event on Linux (document as known
  limitation in `AGENTS.md`).
- If `libnotify` is not available, return `ERR_NOT_SUPPORTED`.

---

## P-19 — File System Watch (All Platforms)

**Goal**: Implement `fs.watch` so it actually fires `fs.changed` events when
files or directories change.

**Files to modify**:
- `modules/nv-platform-mac/src/nv_mac.m` (FSEvents)
- `modules/nv-platform-win/src/nv_win.c` (ReadDirectoryChangesW)
- `modules/nv-platform-linux/src/nv_linux.c` (inotify)
- `modules/nv-ops/src/nv_op_fs.c`

**New symbols** (implement on each platform):
```c
NV_INTERNAL int  nv_mac_fs_watch_start(long long id, const char* path,
                                        nv_window_t* w);
NV_INTERNAL void nv_mac_fs_watch_stop(long long id);
/* same for nv_win_ and nv_linux_ */
```

**Implementation notes**:

*macOS (FSEvents)*:
- `FSEventStreamCreate` with a C callback. `FSEventStreamScheduleWithRunLoop` on
  the main run loop. Fire `nv_send(w, "fs.changed", ...)` from the callback.
  `FSEventStreamStop` + `FSEventStreamInvalidate` + `FSEventStreamRelease` on
  `watch_stop`.

*Windows (ReadDirectoryChangesW)*:
- Create a worker thread per watch. Use `ReadDirectoryChangesW` with
  `FILE_NOTIFY_CHANGE_*` flags in a loop. Post results to main thread via
  `PostMessage(hwnd, WM_NV_FS_CHANGE, ...)`. Handle `WM_NV_FS_CHANGE` in
  `WndProc` to call `nv_send`. Stop: set a stop event, join the thread.

*Linux (inotify)*:
- Add `inotify_init()` fd to the GLib main loop via `g_io_add_watch`. In the
  callback, read events and call `nv_send`. On `watch_stop`,
  `inotify_rm_watch` + remove source.

**Op handler change** in `nv_op_fs.c`:
- Replace the TODO comment block with actual platform hook calls under `#ifdef`.
- Keep the `g_watches` array for `unwatch` (stop by id).

**`fs.changed` payload**:
```json
{ "id": 1, "path": "/watched/dir", "type": "modified" }
```
Where `type` is one of `"modified"`, `"created"`, `"deleted"`, `"renamed"`.

**Tests**:
- `modules/nv-ops/tests/test_op_fs_watch.c`: unit test with mock hooks —
  verify that `watch` calls the hook and `unwatch` calls stop.
- Integration test in macOS/Linux CI: write a temp file, assert event fires
  within 500 ms.

---

## P-20 — Clipboard Image (All Platforms)

**Goal**: Implement `clipboard.readImage`, `clipboard.writeImage`,
`clipboard.hasImage` on all three platforms.

**Files to modify**:
- `modules/nv-platform-mac/src/nv_mac.m`
- `modules/nv-platform-win/src/nv_win.c`
- `modules/nv-platform-linux/src/nv_linux.c`
- `modules/nv-ops/src/nv_op_clipboard.c`

**Wire format**: images are transferred as base64-encoded PNG.
```json
// readImage reply:
{ "width": 100, "height": 50, "format": "png", "data": "<base64>" }
// writeImage args:
{ "format": "png", "data": "<base64>" }
```

**New symbols** (per platform):
```c
/* Returns malloc'd base64 PNG string + fills *w, *h; caller frees */
NV_INTERNAL char* nv_mac_clipboard_read_image(int* out_w, int* out_h);
NV_INTERNAL int   nv_mac_clipboard_write_image(const char* base64_png);
NV_INTERNAL int   nv_mac_clipboard_has_image(void);
/* same for nv_win_ and nv_linux_ */
```

**Base64 helper**: add `nv_base64_encode` / `nv_base64_decode` to `nv-core`
(`modules/nv-core/src/nv_base64.c` + `nv_base64.h`). Max 200 lines. Add tests
in `modules/nv-core/tests/test_base64.c`.

**Platform notes**:
- macOS: `NSPasteboard` with `NSPasteboardTypePNG`. Encode `NSData` → base64
  via `[data base64EncodedStringWithOptions:0]`.
- Windows: `CF_DIB` or `CF_PNG` (register custom format). Convert DIB to PNG
  using GDI+ (`Gdiplus::Bitmap`).
- Linux: GTK clipboard `GDK_SELECTION_CLIPBOARD` with target `image/png`.
  `gtk_clipboard_request_contents` with the PNG target.

**Tests**: Add base64 round-trip test in `test_base64.c`. Add mock-hook tests
in `test_op_clipboard.c`.

---

## P-21 — App Menu Bar API

**Goal**: Add a C API for defining the application menu bar (macOS) and
window-level menu (Linux). Windows WebView2 apps are typically chromeless;
skip Windows for now and return `ERR_NOT_SUPPORTED`.

**New public header**: `include/nv_menu.h`
```c
typedef struct nv_menu_item {
    const char* id;
    const char* label;
    const char* shortcut;  /* e.g. "CmdOrCtrl+S" */
    int enabled;
    int separator;         /* if 1, other fields ignored */
    struct nv_menu_item* children;
    int child_count;
} nv_menu_item_t;

NV_API void nv_app_set_menu(nv_app_t* app, const nv_menu_item_t* items, int count);
```

**New op**: `"app.setMenu"` in `nv_op_app.c` + registry entry in
`nv_op_registry.c`. Payload: JSON array of menu item objects.

**New platform hooks**:
```c
NV_INTERNAL void nv_mac_app_set_menu(const nv_menu_item_t* items, int count);
NV_INTERNAL void nv_linux_app_set_menu(nv_window_t* w, const nv_menu_item_t* items, int count);
```

**Event**: when a menu item is clicked, fire `nv_send(w, "app.menuAction",
{"id":"file.save"})`.

**macOS implementation** in `nv_mac.m`:
- Build `NSMenu` hierarchy from the items array. Use
  `[NSApp setMainMenu:menu]`. Connect each `NSMenuItem` action to an ObjC
  method that calls `nv_send`.
- Shortcuts: parse `"CmdOrCtrl+S"` → `NSEventModifierFlagCommand` +
  `keyEquivalent:@"s"`.

**Linux implementation** in `nv_linux.c`:
- Replace the current hardcoded Edit menu with a programmatic menu builder.
- Parse the items array into `GtkMenuBar` + `GtkMenu` + `GtkMenuItem` hierarchy.
- Connect `activate` signal to `nv_send`.

**File limit**: if `nv_mac.m` exceeds 800 lines, extract menu code to
`nv_mac_menu.m`.

**Tests**: `modules/nv-ops/tests/test_op_app_menu.c` — parse a JSON menu
array, verify items are parsed correctly; mock `nv_mac_app_set_menu` call
count.

---

## P-22 — Custom Context Menu API

**Goal**: Let C code define custom right-click context menu items for the
WebView, replacing the current env-variable-only toggle.

**New op**: `"window.setContextMenu"` in `nv_op_window.c`.
Payload: `{ "items": [ { "id": "copy", "label": "Copy", "enabled": true }, ... ] }`.

**Event**: `"window.contextMenuAction"` fires when an item is clicked with
`{ "id": "copy" }`.

**New platform hooks**:
```c
NV_INTERNAL void nv_mac_window_set_context_menu(nv_window_t* w,
    const nv_menu_item_t* items, int count);
NV_INTERNAL void nv_win_window_set_context_menu(nv_window_t* w,
    const nv_menu_item_t* items, int count);
NV_INTERNAL void nv_linux_window_set_context_menu(nv_window_t* w,
    const nv_menu_item_t* items, int count);
```

**macOS** (`nv_mac.m`): in `contextMenuItemsForElement:defaultMenuItems:`,
return a custom `NSArray<NSMenuItem*>` built from the per-window menu items
stored in `NVWindowPlatform`. Connect each item's action to `nv_send`.

**Windows** (`nv_win.c`): hook into the WebView2
`add_ContextMenuRequested` event (WebView2 SDK 1.0.1185+). In the handler,
build `ICoreWebView2ContextMenuItemCollection` or show a custom `HMENU` via
`TrackPopupMenu`.

**Linux** (`nv_linux.c`): in the existing
`nv_linux_on_context_menu` callback, if items are registered for this window,
clear the default menu and append custom `WebKitContextMenuItem`s.

**Shared**: add a `nv_menu_item_t* ctx_menu_items` and `int ctx_menu_count`
field to `nv_window_t` internal struct (in `nv_window_internal.h`). The op
handler parses the JSON array into an arena-allocated array and stores it.

**Tests**: `modules/nv-ops/tests/test_op_window_menu.c`. Verify JSON parsing,
item count, and that platform hook is called with correct item list.

---

## P-23 — Global Hotkey API

**Goal**: Register / unregister system-wide keyboard shortcuts that fire even
when the app is not focused.

**New public header**: add to `include/nv_menu.h` (or a new `include/nv_hotkey.h`):
```c
NV_API int  nv_window_register_hotkey(nv_window_t* w, const char* id,
                                       const char* combo);   /* e.g. "CmdOrCtrl+Shift+K" */
NV_API void nv_window_unregister_hotkey(nv_window_t* w, const char* id);
```

**New ops**: `"window.registerHotkey"` / `"window.unregisterHotkey"` in
`nv_op_window.c`.

**Event**: `"window.hotkeyFired"` with `{ "id": "my-hotkey" }`.

**New platform hooks**:
```c
NV_INTERNAL int  nv_mac_register_hotkey(long long handle, const char* combo,
                                          void (*cb)(long long handle, void* ctx),
                                          void* ctx);
NV_INTERNAL void nv_mac_unregister_hotkey(long long handle);
/* same for nv_win_ and nv_linux_ */
```

**macOS**: `RegisterEventHotKey` (Carbon; still works on macOS 14). Parse
combo string → `UInt32 modifiers` + `UInt32 keyCode`. Event handler calls
`nv_send`.

**Windows**: `RegisterHotKey(hwnd, id_int, fsModifiers, vk)`. Handle
`WM_HOTKEY` in `WndProc`.

**Linux**: `XGrabKey` (X11) or `gdk_window_add_filter` approach. Note: no
reliable global hotkey API on Wayland; document this limitation. Return
`ERR_NOT_SUPPORTED` if display is Wayland (`gdk_display_get_name` starts with
"wayland").

**Tests**: `modules/nv-ops/tests/test_op_hotkey.c`. Test combo parsing, id
validation, duplicate registration returns `ERR_ALREADY_EXISTS`.

---

## P-24 — Legacy API Cleanup

**Goal**: Remove dead public API symbols that silently do nothing, preventing
user confusion.

**Files to modify**:
- `modules/nv-runtime/src/nv_window.c`
- `modules/nv-runtime/src/nv_core.c`
- `include/nv.h`
- `include/nv_core.h`

**Changes**:

1. `nv_window_load_url`, `nv_window_load_html`, `nv_window_eval_script`,
   `nv_window_get_url` — these do nothing. Route them to the real internal
   functions (`nv_window_platform_load_url` etc.) or `#pragma deprecated` them
   with a comment pointing to the correct API. Do not silently succeed.

2. `nv_run` in `nv_core.c` — exits immediately. Either:
   - Route it to `nv_app_run` (requires storing the `nv_app_t*` in
     `nv_context_t`), or
   - Mark it `deprecated` and `NV_ERR` + return `-1` so callers know to
     migrate.

3. `nv_ipc_parse` in `nv_ipc.c` — logs `"not yet implemented"`. Implement it
   using `nv_json_parse`: read `id`, `method`, `params`, `error` fields from
   the JSON string. This is needed for C-side IPC testing. If full
   implementation is deferred, at minimum change it to `ERR_NOT_IMPLEMENTED`
   reply (it currently logs `NV_ERR` and returns NULL, which is acceptable —
   just remove the misleading log message that implies it will be implemented
   later).

4. `nv_component_resize` in `nv_component.c` — stores w/h in state but
   doesn't propagate to the WebView. After updating the state, call
   `nv_ui_schedule_diff(form)` so the geometry change triggers a DOM patch.
   This is a one-line fix.

**Tests**: For each changed function, add or update a test that verifies the
new behaviour (routing or error). For `nv_component_resize`, add a test in
`modules/nv-ui/tests/test_component.c` that verifies `schedule_diff` is called
after resize.

---

## P-25 — IPC Hardening (Timeouts and Error Normalization)

**Goal**: Add a request timeout mechanism so JS awaiting a C op reply does not
hang forever if the op panics or blocks.

**Files to modify**:
- `modules/nv-ipc/src/nv_ipc.c`
- `modules/nv-ipc/src/nv_ipc_script.c`
- `include/nv_ipc.h`
- `js/src/core/request.js`

**C side**:
1. Add `uint32_t timeout_ms` to `nv_ipc_state_t` (default 10000).
2. On each dispatched request, record the call time. In the main-thread idle
   callback (platform run loop), scan pending requests and reply
   `ERR_TIMEOUT` to any that have exceeded `timeout_ms`.
3. Add `nv_ipc_set_timeout(nv_window_t* w, uint32_t ms)` public API.

**JS side** (`js/src/core/request.js`):
- Add a `Promise.race` with a `setTimeout` fallback that rejects with
  `{ error: "ERR_TIMEOUT" }` after the configured timeout (default 10s).
  This provides a client-side safety net independent of the C side.

**Error normalization**:
- Audit all op files. Every error reply must use one of the registered error
  codes: `ERR_INVALID_ARG`, `ERR_IO`, `ERR_PERMISSION`, `ERR_NOT_FOUND`,
  `ERR_NOT_SUPPORTED`, `ERR_NOT_IMPLEMENTED`, `ERR_TIMEOUT`,
  `ERR_ALREADY_EXISTS`. Add this list to `include/nv_ipc.h` as constants.
- Any op using ad-hoc error strings must be updated to use a registered code.

**Tests**: `modules/nv-ipc/tests/test_ipc_timeout.c`. Create a mock op that
never replies; advance the clock; assert `ERR_TIMEOUT` is sent.

---

## P-26 — File Watch: Integration Test

**Goal**: Automated integration test for `fs.watch` / `fs.changed` that runs
in macOS and Linux CI.

**Files to create**:
- `modules/nv-ops/tests/test_op_fs_watch_integration.c`

**Instructions**:
1. Create a temp file with `tmpnam` / `mkstemp`.
2. Register a mock `nv_send` capture.
3. Call `nv_mac_fs_watch_start` (or Linux equivalent) directly.
4. Modify the temp file with `fwrite`.
5. Spin the run loop for up to 500 ms waiting for the captured `nv_send` call.
6. Assert event arrived with correct `id` and `path`.
7. Call `watch_stop`. Delete temp file.

Register this test in `CMakeLists.txt` under `ctest -L integration`.

---

## P-27 — Drag & Drop File Bridge

**Goal**: When the user drags files onto the WebView window, C fires a
`window.fileDrop` event with the list of dropped paths.

**New event**: `"window.fileDrop"` payload: `{ "paths": ["/a/b.txt", ...] }`.

**New platform hooks**:
```c
NV_INTERNAL void nv_mac_window_enable_file_drop(nv_window_t* w);
NV_INTERNAL void nv_win_window_enable_file_drop(nv_window_t* w);
NV_INTERNAL void nv_linux_window_enable_file_drop(nv_window_t* w);
```

**New op**: `"window.enableFileDrop"` in `nv_op_window.c`. Calling it once
enables drop for the window.

**macOS**: Set `NVWindowPlatform` as the `NSDraggingDestination` delegate.
Override `draggingEntered:`, `performDragOperation:`. Extract
`NSFilenamesPboardType` paths, call `nv_send`.

**Windows**: Call `DragAcceptFiles(hwnd, TRUE)`. Handle `WM_DROPFILES` in
`WndProc`. Use `DragQueryFileW` to enumerate files.

**Linux**: `gtk_drag_dest_set` + `GTK_DEST_DEFAULT_ALL` with
`GDK_ACTION_COPY`. Connect `drag-data-received` signal. Parse `text/uri-list`
target into file paths.

**Tests**: `modules/nv-ops/tests/test_op_file_drop.c`. Test JSON serialization
of path list. Platform integration is manual / CI smoke only.

---

## P-28 — App Icon API

**Goal**: Let the C app set the window / dock / taskbar icon from a file path
or embedded PNG bytes.

**New public API** (add to `include/nv.h`):
```c
NV_API void nv_window_set_icon(nv_window_t* w, const char* png_path);
```

**New op**: `"window.setIcon"` in `nv_op_window.c`. Arg: `{ "path": "..." }`.

**New platform hooks**:
```c
NV_INTERNAL void nv_mac_window_set_icon(nv_window_t* w, const char* path);
NV_INTERNAL void nv_win_window_set_icon(nv_window_t* w, const char* path);
NV_INTERNAL void nv_linux_window_set_icon(nv_window_t* w, const char* path);
```

**macOS**: `[[window.nsWindow standardWindowButton:NSWindowDocumentIconButton] setImage:]` +
`[[NSApplication sharedApplication] setApplicationIconImage:]`.

**Windows**: Load PNG via `LoadImageW` or GDI+ → `HICON`. Call
`SendMessageW(hwnd, WM_SETICON, ICON_BIG, ...)`.

**Linux**: `gtk_window_set_icon_from_file(GTK_WINDOW(win), path, NULL)`.

**Tests**: `test_op_window.c` — mock hook call count; validate `ERR_INVALID_ARG`
on missing path.

---

## P-29 — CI / CD Pipeline

**Goal**: Add a GitHub Actions matrix build that compiles and runs the test
suite on macOS and Linux on every push and pull request.

**File to create**: `.github/workflows/ci.yml`

**Matrix**:
```
os: [macos-latest, ubuntu-22.04]
```

**Steps per job**:
1. Install dependencies:
   - macOS: `brew install cmake` (WebKit is built in).
   - Ubuntu: `sudo apt-get install cmake libwebkit2gtk-4.1-dev
     libayatana-appindicator3-dev libnotify-dev`.
2. Configure: `cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNV_BUILD_TESTS=ON
   -DNV_BUILD_UI=ON`.
3. Build: `cmake --build build --parallel`.
4. Test: `ctest --test-dir build --output-on-failure`.

**Smoke test** (separate job, runs after matrix):
- Build `examples/counter_ui` in headless mode (no display required for build,
  only compilation).

**Failure notification**: standard GitHub PR check — red X blocks merge.

**Notes**:
- Windows CI is optional for now — add as a third matrix entry once P-06 and
  P-08 are complete.
- Mark failing tests `WILL_FAIL` in CMake rather than excluding them so CI
  tracks regressions.

---

## P-30 — Documentation Completion

**Goal**: Fill the remaining doc gaps identified in `TODO.md` Phase 8.

**Files to create** (all in `docs/`):

### `docs/widget-reference.md`
One section per widget (`NV_COMP_BUTTON`, `NV_COMP_LABEL`, `NV_COMP_INPUT`,
`NV_COMP_CHECKBOX`, `NV_COMP_SELECT`, `NV_COMP_SLIDER`, `NV_COMP_TEXTAREA`,
`NV_COMP_VUE`). Each section: fields table, cfg struct, events, minimal
C example.

### `docs/vue-slot-guide.md`
How to use `NV_COMP_VUE`: create a slot, call `nv_vue_load_component`, call
`nv_vue_set_props`, handle JS→C events. Include the `vue_slot` example walkthrough.

### `docs/architecture.md`
- Module DAG diagram (text-based)
- Vue bridge: scaffold generation → first flush → incremental patch cycle
- Hybrid IPC: postMessage vs SharedArrayBuffer paths
- Owner/parent duality in nv-ui
- Wire protocol version handshake

### `docs/binary-size.md`
After P-13–P-18 are implemented: run `strip` + `size` on the built library for
each platform. Record per-module contribution using `nm -S`. Table format:
`| Module | macOS | Linux | Windows |`.

### `docs/ai-agent-guide.md`
- How to read `AGENTS.md` files
- The 800-line rule and split pattern
- The ask-before-technology rule
- Task label conventions
- Arena allocator usage guide for new contributors

---

## Prompt Execution Order (Dependency Graph)

```
P-00  (unblocks all others — do this first)
  ├── P-01, P-02  (clipboard text Win/Linux)
  ├── P-03, P-04  (shell ops Win/Linux)
  ├── P-05        (shell exec — all platforms)
  ├── P-06, P-07  (dialogs Win/Linux)
  ├── P-08, P-09  (app paths Win/Linux)
  ├── P-10..P-12  (screen info — all platforms)
  ├── P-20        (base64 helper — needed by clipboard image)
  │     └── P-20 continued (clipboard image — all platforms)
  ├── P-13        (tray macOS — establishes hook pattern)
  │     ├── P-14  (tray Windows)
  │     └── P-15  (tray Linux)
  ├── P-16        (notifications macOS)
  │     ├── P-17  (notifications Windows)
  │     └── P-18  (notifications Linux)
  ├── P-19        (fs.watch — all platforms)
  │     └── P-26  (fs.watch integration test)
  ├── P-21        (menu bar API — depends on nv_menu.h)
  │     └── P-22  (context menu API — reuses nv_menu_item_t)
  ├── P-23        (global hotkeys)
  ├── P-24        (legacy cleanup — can be done anytime)
  ├── P-25        (IPC hardening — can be done anytime)
  ├── P-27        (drag & drop)
  ├── P-28        (app icon)
  └── P-29, P-30  (CI + docs — do last)
```

**Rule**: Mark each prompt complete in `docs/Implementation.md` by changing its
stage item from `[ ]` to `[X]`. Do not mark complete until `ctest` passes.
