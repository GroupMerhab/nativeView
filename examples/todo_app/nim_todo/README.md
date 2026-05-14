# Todo app (Nim + nativeview)

Pure Nim port of [../c_todo](../c_todo): same SQLite schema and Vue `__nv` bridge, using [../../bindings/nim/nativeview.nim](../../bindings/nim/nativeview.nim) for the window. Persistence uses [db_connector](https://github.com/nim-lang/db_connector) (`nimble install db_connector`).

## Build

From this directory:

```bash
./build_static.sh
```

- **Full UI**: runs `npm install` + `npm run build` in [../ui](../ui), embeds `dist/index.html`, configures CMake for static nativeview, links SQLite (`pkg-config sqlite3` or `-lsqlite3`).
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh` embeds [../ui/fallback_index.html](../ui/fallback_index.html) only.

Output binary: `./nim_todo`. Optional DB path: first CLI argument (default `./todo_app.db`).

## Tests

```bash
./run_tests.sh
```

## HTML embed

[tools/embed_html_nim.py](tools/embed_html_nim.py) writes [generated/todo_html_embed.nim](generated/todo_html_embed.nim) (byte array for `nv_load_html_ref`). The C example’s [../c_todo/tools/embed_html.py](../c_todo/tools/embed_html.py) is **not** modified.

## macOS

Static linking may crash before the window appears; use shared `libnativeview` and `-d:nativeviewShared` as in [examples/nim/README.md](../../nim/README.md).
