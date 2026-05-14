#!/usr/bin/env python3
# Todo app: Vue UI + SQLite + nativeview (Python port of c_todo / nim_todo).
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path

_THIS_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _THIS_DIR.parents[2]
_BINDINGS = _REPO_ROOT / "bindings" / "python"
if _BINDINGS.is_dir():
    sys.path.insert(0, str(_BINDINGS))
sys.path.insert(0, str(_THIS_DIR))

import nativeview as nv  # noqa: E402

from todo_bridge import bridge_handle_wire  # noqa: E402
from todo_store import store_close, store_open  # noqa: E402

try:
    from generated.todo_html_embed import NV_TODO_HTML  # noqa: E402
except ImportError:
    sys.stderr.write(
        "[todo_app] missing generated/todo_html_embed.py — run ./build_shared.sh\n"
    )
    raise

# Stable through nv_window_create (nv.h).
_g_title = b"Todo App"

# Keep callbacks alive while registered.
_on_ready: nv.NvReadyCb | None = None
_on_message: nv.NvMsgCb | None = None
_on_close: nv.NvCloseCb | None = None

# Message handler reads this (nv_on_message only accepts void* userdata; py_object does not cast).
_g_ctx: _AppCtx | None = None


class _AppCtx:
    __slots__ = ("store",)

    def __init__(self) -> None:
        self.store = None  # set after store_open


def _send_json(win: ctypes.c_void_p, ev: str, j: str) -> None:
    nv.nv_send(win, ev.encode("utf-8"), j.encode("utf-8"))


def _nativeview_lib_filenames() -> tuple[str, ...]:
    if sys.platform == "win32":
        return ("nativeview.dll",)
    if sys.platform == "darwin":
        return ("libnativeview.dylib",)
    return ("libnativeview.so",)


def _ensure_nativeview_loaded() -> None:
    """Load shared nativeview: NATIVEVIEW_LIB, else repo build dirs from ./build_shared.sh."""
    if not os.environ.get("NATIVEVIEW_LIB"):
        for sub in ("build-python-todo-shared", "build-python-shared", "build"):
            base = _REPO_ROOT / sub
            for name in _nativeview_lib_filenames():
                cand = base / name
                if cand.is_file():
                    nv.load_library(str(cand))
                    return
    try:
        nv.load_library()
    except OSError as e:
        sys.stderr.write(
            "[todo_app] Could not load nativeview shared library "
            f"({e}).\n"
            "From this directory run ./build_shared.sh, or set NATIVEVIEW_LIB to the "
            "full path of libnativeview.so (see py_todo/README.md).\n"
        )
        raise SystemExit(1) from e


def _c_char_param_to_bytes(p: object) -> bytes:
    if p is None:
        return b""
    if isinstance(p, (bytes, bytearray)):
        return bytes(p)
    if isinstance(p, memoryview):
        return p.tobytes()
    val = getattr(p, "value", None)
    if val is None:
        return b""
    if isinstance(val, bytes):
        return val
    if isinstance(val, str):
        return val.encode("utf-8")
    return b""


def main() -> None:
    global _on_ready, _on_message, _on_close, _g_ctx

    _ensure_nativeview_loaded()

    db_path = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] else "todo_app.db"
    store = store_open(db_path)
    if store is None:
        sys.stderr.write(f"[todo_app] todo_store_open failed for {db_path}\n")
        sys.exit(1)

    app = nv.nv_app_create()
    if not app:
        sys.stderr.write("[todo_app] nv_app_create failed\n")
        store_close(store)
        sys.exit(1)

    cfg = nv.NvWindowCfg(
        title=ctypes.c_char_p(_g_title),
        width=960,
        height=720,
        min_width=0,
        min_height=0,
        resizable=1,
        frameless=0,
        transparent=0,
        devtools=1,
        modal=0,
    )

    win = nv.nv_window_create(app, cfg)
    if not win:
        sys.stderr.write("[todo_app] nv_window_create failed\n")
        store_close(store)
        nv.nv_app_destroy(app)
        sys.exit(1)

    _g_ctx = _AppCtx()
    _g_ctx.store = store

    def ready_cb(_w: ctypes.c_void_p, _userdata: ctypes.c_void_p) -> None:
        sys.stderr.write("[todo_app] webview ready\n")

    def message_cb(
        w: ctypes.c_void_p,
        event: ctypes.c_char_p,
        json_body: ctypes.c_char_p,
        _userdata: ctypes.c_void_p,
    ) -> None:
        c = _g_ctx
        if c is None or c.store is None:
            return
        ev = _c_char_param_to_bytes(event)
        if ev != b"todo":
            return
        raw = _c_char_param_to_bytes(json_body)
        try:
            body = raw.decode("utf-8")
        except UnicodeDecodeError:
            return

        def emit_proc(ev_name: str, j: str) -> None:
            _send_json(w, ev_name, j)

        bridge_handle_wire(c.store, body, emit_proc)

    def close_cb(_w: ctypes.c_void_p, userdata: ctypes.c_void_p) -> None:
        nv.nv_app_quit(userdata)

    _on_ready = nv.NvReadyCb(ready_cb)
    _on_message = nv.NvMsgCb(message_cb)
    _on_close = nv.NvCloseCb(close_cb)

    nv.nv_on_ready(win, _on_ready, None)
    nv.nv_on_message(win, _on_message, None)
    nv.nv_window_on_close(win, _on_close, app)

    nv.nv_load_html_ref(win, NV_TODO_HTML, None, b"about:blank")
    nv.nv_window_show(win)

    nv.nv_app_run(app)

    store_close(store)
    _g_ctx = None
    nv.nv_app_destroy(app)


if __name__ == "__main__":
    main()
