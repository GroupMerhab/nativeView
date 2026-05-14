# Todo app (Python + nativeview)

Python port of [../c_todo](../c_todo) and [../nim_todo](../nim_todo): same SQLite schema and Vue `__nv` bridge. The window uses [../../../bindings/python/nativeview](../../../bindings/python/nativeview) (ctypes) against a **shared** `libnativeview` — see [../../../docs/Python.md](../../../docs/Python.md).

## Build

From this directory:

```bash
./build_shared.sh
```

- **Full UI**: runs `npm install` + `npm run build` in [../ui](../ui), embeds `dist/index.html`, configures CMake with `NV_BUILD_SHARED=ON`, builds target **`nativeview_shared`**.
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./build_shared.sh` embeds [../ui/fallback_index.html](../ui/fallback_index.html) only.

The script prints `NATIVEVIEW_LIB` and `PYTHONPATH`. Run:

```bash
export NATIVEVIEW_LIB=…   # as printed
export PYTHONPATH=…/bindings/python
python3 todo_app.py
```

Optional DB path: first CLI argument (default `./todo_app.db`).

## Tests

```bash
./run_tests.sh
```

## HTML embed

[tools/embed_html_py.py](tools/embed_html_py.py) writes [generated/todo_html_embed.py](generated/todo_html_embed.py) (`NV_TODO_HTML` for `nv_load_html_ref`). The C example’s [../c_todo/tools/embed_html.py](../c_todo/tools/embed_html.py) is **not** modified.

## macOS

Prefer the shared library path for GUI apps (see `docs/Python.md`).
