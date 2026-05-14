# Todo app (Rust + nativeview)

Rust port of [../c_todo](../c_todo): same SQLite schema and Vue `__nv` bridge, using the [nativeview Rust bindings](../../../bindings/rust) (`link-none` + CMake static archives; see [build_static.sh](build_static.sh)). Persistence uses [rusqlite](https://github.com/rusqlite/rusqlite) against the system `libsqlite3` (same as other static todo builds: `pkg-config sqlite3` or `-lsqlite3`).

## Build

From this directory:

```bash
chmod +x build_static.sh run_tests.sh
./build_static.sh
```

- **Full UI**: runs `npm install` + `npm run build` in [../ui](../ui), embeds `dist/index.html`, configures CMake for static nativeview, links SQLite (`pkg-config sqlite3` or `-lsqlite3`).
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh` embeds [../ui/fallback_index.html](../ui/fallback_index.html) only.

Output binary: `./rust_todo` (copied from `target/release/rust_todo`). Optional DB path: first CLI argument (default `./todo_app.db`).

## Tests

```bash
./run_tests.sh
```

## HTML embed

[tools/embed_html_rust.py](tools/embed_html_rust.py) writes [generated/todo_html_embed.rs](generated/todo_html_embed.rs) (byte array for `nv_load_html_ref`). A small checked-in embed from the fallback HTML keeps `cargo test` working before the first full UI build.

## macOS

`build_static.sh` passes the same static archive + framework order as the C and Nim todo examples (`RUSTFLAGS` with `-Clink-arg=…`). For an optional **`libnativeview.dylib`** build, configure CMake with **`NV_BUILD_SHARED=ON`**, build **`nativeview_shared`**, and use the **`nativeview`** crate with **`link-shared`** instead of **`link-none`** (see [examples/nim/README.md](../../../nim/README.md) and **`docs/Nim.md`** / **`docs/Zig.md`** for the shared pattern).
