# Low-level Python (ctypes) binding for the nativeview public C API (`include/nv.h`,
# `include/nv_hotkey.h`, plus `nv_version_string` / `nv_get_version_info` / `nv_bench_now`
# from `include/nv_util.h`).
#
# Load the shared library (`nativeview.dll` / `libnativeview.so` / `libnativeview.dylib`)
# built with CMake `-DNV_BUILD_SHARED=ON`, or set `NATIVEVIEW_LIB` to the full path.
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import (
    CDLL,
    CFUNCTYPE,
    POINTER,
    byref,
    c_char,
    c_char_p,
    c_double,
    c_int,
    c_size_t,
    c_uint16,
    c_uint64,
    c_void_p,
    cast,
)
from typing import Sequence

__all__ = [
    "NV_HOTKEY_ERR_ALREADY_EXISTS",
    "NV_HOTKEY_ERR_INVALID",
    "NV_HOTKEY_ERR_NOT_SUPPORTED",
    "NV_HOTKEY_ERR_PLATFORM",
    "NV_HOTKEY_OK",
    "NvVersionInfo",
    "NvWindowCfg",
    "get_library",
    "load_library",
    "nv_app_create",
    "nv_app_destroy",
    "nv_app_quit",
    "nv_app_run",
    "nv_bench_now",
    "nv_eval_js",
    "nv_eval_js_batch",
    "nv_get_version_info",
    "nv_load_html",
    "nv_load_html_ref",
    "nv_load_url",
    "nv_on_message",
    "nv_on_ready",
    "nv_send",
    "nv_version_string",
    "nv_window_center",
    "nv_window_close",
    "nv_window_create",
    "nv_window_destroy",
    "nv_window_focus",
    "nv_window_fullscreen",
    "nv_window_hide",
    "nv_window_is_focused",
    "nv_window_is_fullscreen",
    "nv_window_maximize",
    "nv_window_minimize",
    "nv_window_on_close",
    "nv_window_register_hotkey",
    "nv_window_request_close",
    "nv_window_restore",
    "nv_window_set_always_on_top",
    "nv_window_set_min_size",
    "nv_window_set_opacity",
    "nv_window_set_resizable",
    "nv_window_set_size",
    "nv_window_set_title",
    "nv_window_set_zoom_factor",
    "nv_window_show",
    "nv_window_unregister_hotkey",
]

NV_HOTKEY_OK = 0
NV_HOTKEY_ERR_INVALID = -1
NV_HOTKEY_ERR_ALREADY_EXISTS = -2
NV_HOTKEY_ERR_NOT_SUPPORTED = -3
NV_HOTKEY_ERR_PLATFORM = -4

_nv: CDLL | None = None


def _default_lib_names() -> list[str]:
    if sys.platform == "win32":
        return ["nativeview.dll"]
    if sys.platform == "darwin":
        return ["libnativeview.dylib"]
    return ["libnativeview.so"]


def load_library(path: str | None = None) -> CDLL:
    """Load the nativeview shared library. Idempotent: returns the same handle if already loaded."""
    global _nv
    if _nv is not None and path is None:
        return _nv

    candidates: list[str] = []
    if path:
        candidates.append(path)
    env = os.environ.get("NATIVEVIEW_LIB")
    if env:
        candidates.append(env)
    for name in _default_lib_names():
        candidates.append(name)

    last_err: OSError | None = None
    lib: CDLL | None = None
    for cand in candidates:
        try:
            lib = CDLL(cand)
            break
        except OSError as e:
            last_err = e
            continue

    if lib is None:
        raise OSError(
            "Could not load nativeview shared library. "
            "Build with -DNV_BUILD_SHARED=ON (target nativeview_shared), "
            "then set NATIVEVIEW_LIB or place "
            + " / ".join(_default_lib_names())
            + " on the loader path."
        ) from last_err

    _bind(lib)
    _nv = lib
    return lib


def get_library() -> CDLL:
    """Return the loaded CDLL, loading defaults on first use."""
    if _nv is None:
        return load_library()
    return _nv


class NvWindowCfg(ctypes.Structure):
    _fields_ = [
        ("title", c_char_p),
        ("width", c_int),
        ("height", c_int),
        ("min_width", c_int),
        ("min_height", c_int),
        ("resizable", c_int),
        ("frameless", c_int),
        ("transparent", c_int),
        ("devtools", c_int),
        ("modal", c_int),
    ]


class NvVersionInfo(ctypes.Structure):
    _fields_ = [
        ("major", c_uint16),
        ("minor", c_uint16),
        ("patch", c_uint16),
        ("build_sha", c_char_p),
    ]


_NvMsgCb = CFUNCTYPE(None, c_void_p, c_char_p, c_char_p, c_void_p)
_NvReadyCb = CFUNCTYPE(None, c_void_p, c_void_p)
_NvCloseCb = CFUNCTYPE(None, c_void_p, c_void_p)


def _bind(lib: CDLL) -> None:
    lib.nv_app_create.argtypes = []
    lib.nv_app_create.restype = c_void_p

    lib.nv_app_run.argtypes = [c_void_p]
    lib.nv_app_run.restype = None

    lib.nv_app_quit.argtypes = [c_void_p]
    lib.nv_app_quit.restype = None

    lib.nv_app_destroy.argtypes = [c_void_p]
    lib.nv_app_destroy.restype = None

    lib.nv_window_create.argtypes = [c_void_p, POINTER(NvWindowCfg)]
    lib.nv_window_create.restype = c_void_p

    lib.nv_window_close.argtypes = [c_void_p]
    lib.nv_window_close.restype = None

    lib.nv_window_destroy.argtypes = [c_void_p]
    lib.nv_window_destroy.restype = None

    lib.nv_window_show.argtypes = [c_void_p]
    lib.nv_window_show.restype = None

    lib.nv_window_hide.argtypes = [c_void_p]
    lib.nv_window_hide.restype = None

    lib.nv_window_set_title.argtypes = [c_void_p, c_char_p]
    lib.nv_window_set_title.restype = None

    lib.nv_window_set_size.argtypes = [c_void_p, c_int, c_int]
    lib.nv_window_set_size.restype = None

    lib.nv_window_set_min_size.argtypes = [c_void_p, c_int, c_int]
    lib.nv_window_set_min_size.restype = None

    lib.nv_window_center.argtypes = [c_void_p]
    lib.nv_window_center.restype = None

    lib.nv_window_fullscreen.argtypes = [c_void_p, c_int]
    lib.nv_window_fullscreen.restype = None

    lib.nv_window_is_fullscreen.argtypes = [c_void_p]
    lib.nv_window_is_fullscreen.restype = c_int

    lib.nv_window_minimize.argtypes = [c_void_p]
    lib.nv_window_minimize.restype = None

    lib.nv_window_maximize.argtypes = [c_void_p]
    lib.nv_window_maximize.restype = None

    lib.nv_window_restore.argtypes = [c_void_p]
    lib.nv_window_restore.restype = None

    lib.nv_window_focus.argtypes = [c_void_p]
    lib.nv_window_focus.restype = None

    lib.nv_window_is_focused.argtypes = [c_void_p]
    lib.nv_window_is_focused.restype = c_int

    lib.nv_window_set_resizable.argtypes = [c_void_p, c_int]
    lib.nv_window_set_resizable.restype = None

    lib.nv_window_set_always_on_top.argtypes = [c_void_p, c_int]
    lib.nv_window_set_always_on_top.restype = None

    lib.nv_window_set_opacity.argtypes = [c_void_p, c_int]
    lib.nv_window_set_opacity.restype = None

    lib.nv_window_set_zoom_factor.argtypes = [c_void_p, c_double]
    lib.nv_window_set_zoom_factor.restype = None

    lib.nv_window_request_close.argtypes = [c_void_p]
    lib.nv_window_request_close.restype = None

    lib.nv_window_on_close.argtypes = [c_void_p, _NvCloseCb, c_void_p]
    lib.nv_window_on_close.restype = None

    lib.nv_load_url.argtypes = [c_void_p, c_char_p]
    lib.nv_load_url.restype = None

    lib.nv_load_html.argtypes = [c_void_p, c_char_p, c_char_p]
    lib.nv_load_html.restype = None

    lib.nv_load_html_ref.argtypes = [c_void_p, c_char_p, c_size_t, c_char_p]
    lib.nv_load_html_ref.restype = None

    lib.nv_eval_js.argtypes = [c_void_p, c_char_p]
    lib.nv_eval_js.restype = None

    lib.nv_eval_js_batch.argtypes = [c_void_p, POINTER(c_char_p), c_size_t]
    lib.nv_eval_js_batch.restype = None

    lib.nv_on_message.argtypes = [c_void_p, _NvMsgCb, c_void_p]
    lib.nv_on_message.restype = None

    lib.nv_on_ready.argtypes = [c_void_p, _NvReadyCb, c_void_p]
    lib.nv_on_ready.restype = None

    lib.nv_send.argtypes = [c_void_p, c_char_p, c_char_p]
    lib.nv_send.restype = None

    lib.nv_window_register_hotkey.argtypes = [c_void_p, c_char_p, c_char_p]
    lib.nv_window_register_hotkey.restype = c_int

    lib.nv_window_unregister_hotkey.argtypes = [c_void_p, c_char_p]
    lib.nv_window_unregister_hotkey.restype = None

    lib.nv_version_string.argtypes = []
    lib.nv_version_string.restype = c_char_p

    lib.nv_get_version_info.argtypes = [POINTER(NvVersionInfo)]
    lib.nv_get_version_info.restype = None

    lib.nv_bench_now.argtypes = []
    lib.nv_bench_now.restype = c_uint64


def _enc(s: str | bytes | None) -> bytes | None:
    if s is None:
        return None
    if isinstance(s, bytes):
        return s
    return s.encode("utf-8")


def nv_app_create() -> c_void_p:
    return get_library().nv_app_create()


def nv_app_run(app: c_void_p) -> None:
    get_library().nv_app_run(app)


def nv_app_quit(app: c_void_p) -> None:
    get_library().nv_app_quit(app)


def nv_app_destroy(app: c_void_p) -> None:
    get_library().nv_app_destroy(app)


def nv_window_create(app: c_void_p, config: NvWindowCfg | POINTER(NvWindowCfg)) -> c_void_p:
    ptr = byref(config) if isinstance(config, NvWindowCfg) else config
    return get_library().nv_window_create(app, ptr)


def nv_window_close(window: c_void_p) -> None:
    get_library().nv_window_close(window)


def nv_window_destroy(window: c_void_p) -> None:
    get_library().nv_window_destroy(window)


def nv_window_show(window: c_void_p) -> None:
    get_library().nv_window_show(window)


def nv_window_hide(window: c_void_p) -> None:
    get_library().nv_window_hide(window)


def nv_window_set_title(window: c_void_p, title: str | bytes | None) -> None:
    get_library().nv_window_set_title(window, c_char_p(_enc(title)))


def nv_window_set_size(window: c_void_p, width: int, height: int) -> None:
    get_library().nv_window_set_size(window, width, height)


def nv_window_set_min_size(window: c_void_p, width: int, height: int) -> None:
    get_library().nv_window_set_min_size(window, width, height)


def nv_window_center(window: c_void_p) -> None:
    get_library().nv_window_center(window)


def nv_window_fullscreen(window: c_void_p, enable: int) -> None:
    get_library().nv_window_fullscreen(window, enable)


def nv_window_is_fullscreen(window: c_void_p) -> int:
    return int(get_library().nv_window_is_fullscreen(window))


def nv_window_minimize(window: c_void_p) -> None:
    get_library().nv_window_minimize(window)


def nv_window_maximize(window: c_void_p) -> None:
    get_library().nv_window_maximize(window)


def nv_window_restore(window: c_void_p) -> None:
    get_library().nv_window_restore(window)


def nv_window_focus(window: c_void_p) -> None:
    get_library().nv_window_focus(window)


def nv_window_is_focused(window: c_void_p) -> int:
    return int(get_library().nv_window_is_focused(window))


def nv_window_set_resizable(window: c_void_p, enable: int) -> None:
    get_library().nv_window_set_resizable(window, enable)


def nv_window_set_always_on_top(window: c_void_p, enable: int) -> None:
    get_library().nv_window_set_always_on_top(window, enable)


def nv_window_set_opacity(window: c_void_p, opacity_pct: int) -> None:
    get_library().nv_window_set_opacity(window, opacity_pct)


def nv_window_set_zoom_factor(window: c_void_p, factor: float) -> None:
    get_library().nv_window_set_zoom_factor(window, factor)


def nv_window_request_close(window: c_void_p) -> None:
    get_library().nv_window_request_close(window)


def nv_window_on_close(
    window: c_void_p,
    callback: _NvCloseCb | None,
    userdata: c_void_p | int | None,
) -> None:
    ud = ctypes.cast(userdata, c_void_p) if userdata is not None else c_void_p(None)
    get_library().nv_window_on_close(window, callback, ud)


def nv_load_url(window: c_void_p, url: str | bytes | None) -> None:
    get_library().nv_load_url(window, c_char_p(_enc(url)))


def nv_load_html(window: c_void_p, html: str | bytes | None, base_url: str | bytes | None) -> None:
    get_library().nv_load_html(window, c_char_p(_enc(html)), c_char_p(_enc(base_url)))


def nv_load_html_ref(
    window: c_void_p,
    html: bytes,
    length: int | None = None,
    base_url: str | bytes | None = None,
) -> None:
    n = len(html) if length is None else length
    body = html[:n]
    buf = (c_char * n).from_buffer_copy(body)
    get_library().nv_load_html_ref(window, cast(buf, c_char_p), n, c_char_p(_enc(base_url)))


def nv_eval_js(window: c_void_p, js: str | bytes | None) -> None:
    get_library().nv_eval_js(window, c_char_p(_enc(js)))


def nv_eval_js_batch(window: c_void_p, scripts: Sequence[str | bytes]) -> None:
    """Pass a list/tuple of scripts; bytes are sent as-is, str is UTF-8 encoded."""
    encoded = [s if isinstance(s, bytes) else s.encode("utf-8") for s in scripts]
    arr = (c_char_p * len(encoded))(*encoded)
    get_library().nv_eval_js_batch(window, arr, len(encoded))


def nv_on_message(
    window: c_void_p,
    callback: _NvMsgCb | None,
    userdata: c_void_p | int | None,
) -> None:
    ud = ctypes.cast(userdata, c_void_p) if userdata is not None else c_void_p(None)
    get_library().nv_on_message(window, callback, ud)


def nv_on_ready(
    window: c_void_p,
    callback: _NvReadyCb | None,
    userdata: c_void_p | int | None,
) -> None:
    ud = ctypes.cast(userdata, c_void_p) if userdata is not None else c_void_p(None)
    get_library().nv_on_ready(window, callback, ud)


def nv_send(window: c_void_p, event: str | bytes | None, json_payload: str | bytes | None) -> None:
    get_library().nv_send(window, c_char_p(_enc(event)), c_char_p(_enc(json_payload)))


def nv_window_register_hotkey(window: c_void_p, id_: str | bytes, combo: str | bytes) -> int:
    return int(get_library().nv_window_register_hotkey(window, c_char_p(_enc(id_)), c_char_p(_enc(combo))))


def nv_window_unregister_hotkey(window: c_void_p, id_: str | bytes) -> None:
    get_library().nv_window_unregister_hotkey(window, c_char_p(_enc(id_)))


def nv_version_string() -> str | None:
    p = get_library().nv_version_string()
    if not p:
        return None
    return p.decode("utf-8", errors="replace")


def nv_get_version_info() -> NvVersionInfo:
    info = NvVersionInfo()
    get_library().nv_get_version_info(byref(info))
    return info


def nv_bench_now() -> int:
    return int(get_library().nv_bench_now())


# Re-export callback factory types for user code (must keep references alive).
NvMsgCb = _NvMsgCb
NvReadyCb = _NvReadyCb
NvCloseCb = _NvCloseCb
__all__.extend(["NvMsgCb", "NvReadyCb", "NvCloseCb"])
