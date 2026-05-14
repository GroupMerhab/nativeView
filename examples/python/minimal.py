#!/usr/bin/env python3
# Minimal nativeview sample: nv_on_ready -> nv_load_html, nv_eval_js_batch, teardown.
# Build the shared library and run with PYTHONPATH and NATIVEVIEW_LIB; see README.md
# and examples/python/build_shared.sh.
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import ctypes
import sys
from pathlib import Path

# Repo layout: examples/python/minimal.py -> ../../bindings/python
_REPO_BINDINGS = Path(__file__).resolve().parents[2] / "bindings" / "python"
if _REPO_BINDINGS.is_dir():
    sys.path.insert(0, str(_REPO_BINDINGS))

import nativeview as nv

_HTML = """<!doctype html>
<html><head><meta charset="utf-8"><title>Python + nativeview</title></head>
<body><p id="t">loading\u2026</p>
<script>window.__nv.on('ping', function(d){ document.getElementById('t').textContent = d.msg; });</script>
</body></html>""".encode(
    "utf-8"
)

# Must outlive nv_window_create (nv.h): stable bytes for cfg.title
_g_title = b"nativeview (Python minimal)"

# Keep callbacks alive while registered
_on_ready = None
_on_message = None
_on_close = None


def main() -> None:
    global _on_ready, _on_message, _on_close

    ver = nv.nv_version_string()
    if ver:
        print(ver)

    info = nv.nv_get_version_info()
    print(f"nativeview {info.major}.{info.minor}.{info.patch}")

    app = nv.nv_app_create()
    if not app:
        print("nv_app_create failed", file=sys.stderr)
        sys.exit(1)

    cfg = nv.NvWindowCfg(
        title=ctypes.c_char_p(_g_title),
        width=800,
        height=600,
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
        print("nv_window_create failed", file=sys.stderr)
        nv.nv_app_destroy(app)
        sys.exit(1)

    def ready_cb(w: ctypes.c_void_p, _userdata: ctypes.c_void_p) -> None:
        nv.nv_load_html(w, _HTML, b"about:blank")
        nv.nv_eval_js_batch(w, [b"document.title='python'", b"void 0"])
        nv.nv_send(w, b"ping", b'{"msg":"hello from Python"}')

    def message_cb(_w, _event, _json, _userdata) -> None:
        pass

    def close_cb(_w, userdata) -> None:
        nv.nv_app_quit(userdata)

    _on_ready = nv.NvReadyCb(ready_cb)
    _on_message = nv.NvMsgCb(message_cb)
    _on_close = nv.NvCloseCb(close_cb)

    nv.nv_on_ready(win, _on_ready, None)
    nv.nv_on_message(win, _on_message, None)
    nv.nv_window_on_close(win, _on_close, app)

    nv.nv_window_show(win)
    nv.nv_app_run(app)
    nv.nv_app_destroy(app)


if __name__ == "__main__":
    main()
