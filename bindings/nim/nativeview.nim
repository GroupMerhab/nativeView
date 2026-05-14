## Nim import module for the nativeview public C API (`include/nv.h`, `include/nv_hotkey.h`,
## plus `nv_version_string` / `nv_get_version_info` / `nv_bench_now` from `include/nv_util.h`).
##
## Linking: see `docs/Nim.md`. Default in this repo is **static** (no `dynlib`); define
## `-d:nativeviewShared` when linking `libnativeview` / `nativeview.dll` / `.dylib`.
##
## SPDX-License-Identifier: Apache-2.0

when defined(nativeviewShared):
  when defined(windows):
    const NvDynlib* = "nativeview.dll"
  elif defined(macosx):
    const NvDynlib* = "libnativeview.dylib"
  else:
    const NvDynlib* = "libnativeview.so"
  {.push dynlib: NvDynlib.}

type
  NvApp* = distinct pointer
  NvWindow* = distinct pointer

  NvMsgCb* = proc (window: NvWindow; event, json: cstring; userdata: pointer) {.cdecl.}
  NvReadyCb* = proc (window: NvWindow; userdata: pointer) {.cdecl.}
  NvCloseCb* = proc (window: NvWindow; userdata: pointer) {.cdecl.}

  NvWindowCfg* {.bycopy.} = object
    title*: cstring
    width*: cint
    height*: cint
    min_width*: cint
    min_height*: cint
    resizable*: cint
    frameless*: cint
    transparent*: cint
    devtools*: cint
    modal*: cint

  NvVersionInfo* {.bycopy.} = object
    major*: uint16
    minor*: uint16
    patch*: uint16
    build_sha*: cstring

const
  NV_HOTKEY_OK* = 0
  NV_HOTKEY_ERR_INVALID* = -1
  NV_HOTKEY_ERR_ALREADY_EXISTS* = -2
  NV_HOTKEY_ERR_NOT_SUPPORTED* = -3
  NV_HOTKEY_ERR_PLATFORM* = -4

proc nv_app_create*(): NvApp {.cdecl, importc.}
proc nv_app_run*(app: NvApp) {.cdecl, importc.}
proc nv_app_quit*(app: NvApp) {.cdecl, importc.}
proc nv_app_destroy*(app: NvApp) {.cdecl, importc.}

proc nv_window_create*(app: NvApp; config: ptr NvWindowCfg): NvWindow {.cdecl, importc.}
proc nv_window_close*(window: NvWindow) {.cdecl, importc.}
proc nv_window_destroy*(window: NvWindow) {.cdecl, importc.}
proc nv_window_show*(window: NvWindow) {.cdecl, importc.}
proc nv_window_hide*(window: NvWindow) {.cdecl, importc.}
proc nv_window_set_title*(window: NvWindow; title: cstring) {.cdecl, importc.}
proc nv_window_set_size*(window: NvWindow; width, height: cint) {.cdecl, importc.}
proc nv_window_set_min_size*(window: NvWindow; width, height: cint) {.cdecl, importc.}
proc nv_window_center*(window: NvWindow) {.cdecl, importc.}
proc nv_window_fullscreen*(window: NvWindow; enable: cint) {.cdecl, importc.}
proc nv_window_is_fullscreen*(window: NvWindow): cint {.cdecl, importc.}
proc nv_window_minimize*(window: NvWindow) {.cdecl, importc.}
proc nv_window_maximize*(window: NvWindow) {.cdecl, importc.}
proc nv_window_restore*(window: NvWindow) {.cdecl, importc.}
proc nv_window_focus*(window: NvWindow) {.cdecl, importc.}
proc nv_window_is_focused*(window: NvWindow): cint {.cdecl, importc.}
proc nv_window_set_resizable*(window: NvWindow; enable: cint) {.cdecl, importc.}
proc nv_window_set_always_on_top*(window: NvWindow; enable: cint) {.cdecl, importc.}
proc nv_window_set_opacity*(window: NvWindow; opacity_pct: cint) {.cdecl, importc.}
proc nv_window_set_zoom_factor*(window: NvWindow; factor: cdouble) {.cdecl, importc.}
proc nv_window_request_close*(window: NvWindow) {.cdecl, importc.}
proc nv_window_on_close*(window: NvWindow; callback: NvCloseCb; userdata: pointer) {.cdecl, importc.}

proc nv_load_url*(window: NvWindow; url: cstring) {.cdecl, importc.}
proc nv_load_html*(window: NvWindow; html, base_url: cstring) {.cdecl, importc.}
proc nv_load_html_ref*(window: NvWindow; html: cstring; length: csize_t; base_url: cstring) {.cdecl, importc.}
proc nv_eval_js*(window: NvWindow; js: cstring) {.cdecl, importc.}
proc nv_eval_js_batch*(window: NvWindow; scripts: ptr UncheckedArray[cstring]; count: csize_t) {.cdecl, importc.}

proc nv_on_message*(window: NvWindow; callback: NvMsgCb; userdata: pointer) {.cdecl, importc.}
proc nv_on_ready*(window: NvWindow; callback: NvReadyCb; userdata: pointer) {.cdecl, importc.}
proc nv_send*(window: NvWindow; event, json: cstring) {.cdecl, importc.}

proc nv_window_register_hotkey*(window: NvWindow; id, combo: cstring): cint {.cdecl, importc.}
proc nv_window_unregister_hotkey*(window: NvWindow; id: cstring) {.cdecl, importc.}

proc nv_version_string*(): cstring {.cdecl, importc.}
proc nv_get_version_info*(out_info: ptr NvVersionInfo) {.cdecl, importc.}
proc nv_bench_now*(): uint64 {.cdecl, importc.}

when defined(nativeviewShared):
  {.pop.}
