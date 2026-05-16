# Todo app (Swift + nativeview)

Swift port of [../c_todo](../c_todo): same SQLite schema and Vue `__nv` bridge, using [../../../bindings/swift](../../../bindings/swift) for the window. Persistence uses `SQLite3` from the Swift toolchain plus a small `TodoBackend` module (store + JSON bridge).

## Layout

- **Root `Package.swift`** — `TodoBackend`, `TodoHtmlEmbed` libraries and **`swift test`** targets (no nativeview link).
- **`app/Package.swift`** — `swift_todo` executable that links **Nativeview** + static archives (built via `build_static.sh`).

## Build

From this directory:

```bash
chmod +x build_static.sh run_tests.sh
./build_static.sh
```

- **Full UI**: runs `npm install` + `npm run build` in [../ui](../ui), embeds `dist/index.html`, configures CMake for static nativeview, links SQLite (`-lsqlite3`).
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh` embeds [../ui/fallback_index.html](../ui/fallback_index.html) only.

Output binary: **`./app/.build/release/swift_todo`**. Optional DB path: first CLI argument (default `./todo_app.db`).

## Tests

```bash
./run_tests.sh
```

## HTML embed

[tools/embed_html_swift.py](tools/embed_html_swift.py) writes [Sources/TodoHtmlEmbed/TodoHtmlEmbed.swift](Sources/TodoHtmlEmbed/TodoHtmlEmbed.swift) (byte array for `nv_load_html_ref`). The C example’s [../c_todo/tools/embed_html.py](../c_todo/tools/embed_html.py) is **not** modified.

## macOS

`build_static.sh` mirrors [examples/swift/build_static.sh](../../../examples/swift/build_static.sh) (archive order + frameworks). Run the app on the main thread (`nv_app_run` blocks there).

## See also

- [docs/Swift.md](../../../docs/Swift.md)
