# JavaScript bridge — nativeview (desktop)

The bundled script **`nativeview.js`** (built from `js/src/` via `tools/js_build.py`) exposes the global **`NativeView`** API inside the WebView. Methods map to C op handlers in `modules/nv-ops/` unless noted.

**TypeScript types:** `js/types/nativeview.d.ts`  
**Wire registry:** `modules/nv-ops/src/nv_op_registry.c`  
**Android (Java ops):** [Android.md](Android.md)

---

## Initialization

After the page loads, the bridge calls **`app.handshake`** with `{ wireVersion: <number> }`. Wait for handshake before relying on ops.

| Property | Type | Description |
|----------|------|-------------|
| `NativeView.version` | `string` | Library version string |
| `NativeView.wireVersion` | `number` | IPC wire protocol version |

### Core IPC

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| `NativeView.invoke(method, data?, timeoutMs?)` | Method name, optional JSON payload, optional timeout | `Promise<result>` | Request/response |
| `NativeView.send(event, data?)` | Event name, optional payload | `void` | Fire-and-forget to native |
| `NativeView.on(event, fn)` | Event name, handler | `void` | Native → JS push |
| `NativeView.off(event, fn)` | Event name, handler | `void` | Unsubscribe |
| `NativeView.once(event, fn)` | Event name, handler | `void` | One-shot subscribe |

Legacy alias: `window.__nv` mirrors the same surface in many examples.

---

## Error codes

Rejections use `NativeView.NVError` with `code`:

| Code | Meaning |
|------|---------|
| `ERR_TIMEOUT` | Request timed out |
| `ERR_NOT_FOUND` | Unknown method or target |
| `ERR_PERMISSION` | Permission denied |
| `ERR_INVALID_ARG` | Invalid arguments |
| `ERR_NOT_READY` | Bridge not ready (handshake pending) |
| `ERR_NOT_SUPPORTED` | Op not implemented on this platform |
| `ERR_VERSION_MISMATCH` | Wire version incompatible |
| `ERR_IO` | File or I/O failure |
| `ERR_UNKNOWN` | Unclassified error |

---

## `app` namespace

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| `app.quit()` | — | `void` | Exit application event loop |
| `app.getVersion()` | — | `{ version: string }` | |
| `app.getName()` | — | `{ name: string }` | |
| `app.getDataDir()` | — | `{ path: string }` | Per-user data directory |
| `app.getExePath()` | — | `{ path: string }` | Executable path |
| `app.getResourceDir()` | — | `{ path: string }` | Resource directory |
| `app.getPlatform()` | — | `{ platform: 'mac' \| 'win' \| 'linux' }` | |
| `app.getLocale()` | — | `{ locale: string }` | |

Wire: `app.handshake`, `app.setMenu` (native menu; platform-specific).

---

## `window` namespace

| Method | Parameters | Returns |
|--------|------------|---------|
| `window.setTitle(title)` | `string` | `{}` |
| `window.setSize(w, h)` | numbers | `{}` |
| `window.getSize()` | — | `{ w, h }` |
| `window.setPosition(x, y)` | numbers | `{}` |
| `window.getPosition()` | — | `{ x, y }` |
| `window.setMinSize(w, h)` | numbers | `{}` |
| `window.center()` | — | `{}` |
| `window.minimize()` | — | `{}` |
| `window.maximize()` | — | `{}` |
| `window.restore()` | — | `{}` |
| `window.fullscreen(enable)` | `boolean` | `{}` |
| `window.isFullscreen()` | — | `{ value: boolean }` |
| `window.close()` | — | `{}` |
| `window.focus()` | — | `{}` |
| `window.isFocused()` | — | `{ value: boolean }` |
| `window.setResizable(enable)` | `boolean` | `{}` |
| `window.setAlwaysOnTop(enable)` | `boolean` | `{}` |
| `window.setOpacity(value)` | `number` 0–1 | `{}` |
| `window.setZoomFactor(factor)` | `number` | `{}` |

**Push events (subscribe via `NativeView.on`):** `window.resized`, `window.moved`, `window.focused`, `window.contextMenuAction`, `window.hotkeyFired`, `window.fileDrop` (platform-dependent).

**Additional wire methods:** `window.setContextMenu`, `window.registerHotkey`, `window.unregisterHotkey`.

---

## `fs` namespace

| Method | Parameters | Returns |
|--------|------------|---------|
| `fs.readText(path)` | `string` | `{ text: string }` |
| `fs.writeText(path, text)` | `string`, `string` | `{}` |
| `fs.readBinary(path)` | `string` | `{ bytes: string }` (base64) |
| `fs.writeBinary(path, b64)` | `string`, `string` | `{}` |
| `fs.exists(path)` | `string` | `{ exists: boolean }` |
| `fs.remove(path)` | `string` | `{}` |
| `fs.move(src, dst)` | `string`, `string` | `{}` |
| `fs.copy(src, dst)` | `string`, `string` | `{}` |
| `fs.stat(path)` | `string` | `{ size, isFile, isDir }` |
| `fs.readDir(path)` | `string` | `{ entries: array }` |
| `fs.mkdir(path, recursive?)` | `string`, `boolean?` | `{}` |
| `fs.rmdir(path, recursive?)` | `string`, `boolean?` | `{}` |
| `fs.watch(path, id)` | `string`, `number` | `{}` |
| `fs.unwatch(id)` | `number` | `{}` |

**Push:** `fs.changed` — `{ id, path, ... }` (platform watcher).

**Security:** Paths are not sandboxed by default; restrict in your host application.

---

## `dialog` namespace

| Method | Parameters | Returns |
|--------|------------|---------|
| `dialog.openFile(title?, filters?, multiple?)` | optional strings/filters | `{ canceled, paths[] }` |
| `dialog.saveFile(title?, filters?, defaultName?)` | optional | `{ path }` |
| `dialog.openFolder(title?)` | optional | `{ path }` |
| `dialog.message(title, body, type?, buttons?)` | strings | `{}` |
| `dialog.confirm(title, body)` | strings | `{ confirmed: boolean }` |

`type`: `'info' | 'warning' | 'error'`.

---

## `clipboard` namespace

| Method | Parameters | Returns |
|--------|------------|---------|
| `clipboard.readText()` | — | `{ text: string }` |
| `clipboard.writeText(text)` | `string` | `{}` |
| `clipboard.readImage()` | — | `{ width, height, format, data }` |
| `clipboard.writeImage(b64OrOpts)` | base64 or object | `{}` |
| `clipboard.clear()` | — | `void` |
| `clipboard.hasText()` | — | `{ value: boolean }` |
| `clipboard.hasImage()` | — | `{ value: boolean }` |

---

## `shell` namespace

| Method | Parameters | Returns | Notes |
|--------|------------|---------|--------|
| `shell.openUrl(url)` | `string` | `void` | Default browser |
| `shell.openPath(path)` | `string` | `void` | Open file with default app |
| `shell.showInFolder(path)` | `string` | `void` | Reveal in file manager |
| `shell.exec(cmd, args?, cwd?)` | `string`, `string[]?`, `string?` | `{ exitCode }` | **Privileged** |

---

## `screen` namespace

| Method | Returns |
|--------|---------|
| `screen.getPrimary()` | `{ width, height, scale }` |
| `screen.getAll()` | `{ displays: [...] }` |
| `screen.getCursorPos()` | `{ x, y }` |

---

## `notification` namespace

| Method | Parameters | Returns |
|--------|------------|---------|
| `notification.show(id, title, body, icon?)` | numbers/strings | `{ id }` |
| `notification.close(id)` | `number` | `void` |

**Push:** `notification.clicked`, `notification.closed` — `{ id }`.

Helper: `notification.onClicked(fn)`, `notification.onClosed(fn)`.

---

## `tray` namespace

| Method | Parameters | Returns |
|--------|------------|---------|
| `tray.create(id, icon, tooltip?)` | `number`, base64/icon, `string?` | `{ id }` |
| `tray.destroy(id)` | `number` | `void` |
| `tray.setIcon(id, icon)` | `number`, icon | `{}` |
| `tray.setTooltip(id, tooltip)` | `number`, `string` | `{}` |
| `tray.setMenu(id, items)` | `number`, `MenuItem[]` | `{}` |

**Push:** `tray.clicked`, `tray.menuAction`.

---

## `windows` namespace (multi-window)

| Method | Parameters | Returns |
|--------|------------|---------|
| `windows.open(cfg)` | Window config object | Promise (new window info) |
| `windows.close(id)` | window id | `{}` |
| `windows.focus(id)` | window id | `{}` |
| `windows.list()` | — | `{ windows: [...] }` |
| `windows.getInfo(id)` | window id | window metadata |
| `windows.setTitle(id, title)` | id, title | `{}` |

**`windows.open` config fields:** `id`, `title`, `url`, `width`, `height`, `minWidth`, `minHeight`, `resizable`, `frameless`, `transparent`, `devtools`, `modal`.

**Push:** `windows.opened`, `windows.beforeClose`, `windows.closed`, `windows.focused`, `windows.blurred`.

---

## `ipc_bus` namespace

| Method | Parameters | Returns |
|--------|------------|---------|
| `ipc_bus.send(toWindowId, event, data)` | target id or `"*"`, event, payload | `{ delivered }` |
| `ipc_bus.broadcast(event, data)` | event, payload | same as `send("*", ...)` |
| `ipc_bus.on(event, fn)` | event, `(data, fromWindowId) => void` | — |

**Push:** `ipc_bus.message` — incoming cross-window messages.

---

## C ↔ JS without `NativeView.*` namespaces

From C:

- `nv_send(window, event, json)` — push to JS (`NativeView.on`)
- `nv_on_message(window, callback, userdata)` — receive custom events from JS (`NativeView.send` / `__nv.send`)

From JS (minimal):

```javascript
NativeView.send("my_event", { action: "go" });
NativeView.on("my_reply", (data) => console.log(data));
```

---

## Building the bundle

```bash
python3 tools/js_build.py
```

Output is linked or copied into platform assets (e.g. Android `assets/nativeview.js`). Production builds strip debug blocks via `tools/js_minify.py` when configured in the manifest pipeline.
