# Todo app (Zig + nativeview)

Zig port of [../nim_todo](../nim_todo): same SQLite schema and Vue `__nv` bridge, using [../../../bindings/zig/nativeview.zig](../../../bindings/zig/nativeview.zig) for the window. SQLite is linked via the system C library (`libsqlite3`).

## Build

From this directory:

```bash
./build_static.sh
```

- **Full UI**: runs `npm install` + `npm run build` in [../ui](../ui), embeds `dist/index.html`, configures CMake for static nativeview, links SQLite (`pkg-config sqlite3` or `-lsqlite3`).
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh` embeds [../ui/fallback_index.html](../ui/fallback_index.html) only.

Output binary: `./zig_todo`. Optional DB path: first CLI argument (default `./todo_app.db`).

## Tests

```bash
./run_tests.sh
```

## HTML embed

[tools/embed_html_zig.py](tools/embed_html_zig.py) writes [generated/todo_html_embed.zig](generated/todo_html_embed.zig) (byte array for `nv_load_html_ref`). The C example’s [../c_todo/tools/embed_html.py](../c_todo/tools/embed_html.py) is **not** modified.

## macOS

`build_static.sh` static-links nativeview the same way as the C todo example (archives + frameworks). For an optional **`libnativeview.dylib`** workflow, see [docs/Zig.md](../../../docs/Zig.md).

## Zig version

This tree is tested with **Zig 0.17** (nightly-style `main(std.process.Init.Minimal)`, `std.json.Stringify.valueAlloc`, `std.array_list.Managed`, and the `-M` module graph used in `build_static.sh`). Older Zig releases may need small API adjustments.
