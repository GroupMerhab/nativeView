{
  nativeview.pas — Free Pascal import unit for the public C API (include/nv.h,
  include/nv_hotkey.h).

  User guide (linking DLL vs static, Lazarus, cdecl callbacks, thin-main pattern):
    docs/Pascal.md
  Sample project:
    examples/pascal/

  Shared DLL/SO: define NATIVEVIEW_SHARED when compiling this unit; C headers use NV_SHARED.
  Static: omit NATIVEVIEW_SHARED; link the same archive chain as CMake target `nativeview`.

  SPDX-License-Identifier: Apache-2.0
}

unit nativeview;

{$mode objfpc}{$H+}

interface

uses
  ctypes;

const
  NV_HOTKEY_OK = 0;
  NV_HOTKEY_ERR_INVALID = -1;
  NV_HOTKEY_ERR_ALREADY_EXISTS = -2;
  NV_HOTKEY_ERR_NOT_SUPPORTED = -3;
  NV_HOTKEY_ERR_PLATFORM = -4;

type
  PNVApp = Pointer;
  PNVWindow = Pointer;

  PNVMsgCB = procedure(win: PNVWindow; event_: PAnsiChar; json: PAnsiChar;
    userdata: Pointer); cdecl;
  PNVReadyCB = procedure(win: PNVWindow; userdata: Pointer); cdecl;
  PNVCloseCB = procedure(win: PNVWindow; userdata: Pointer); cdecl;

{$push}{$packrecords c}
  TNVWindowCfg = record
    title: PAnsiChar;
    width: cint;
    height: cint;
    min_width: cint;
    min_height: cint;
    resizable: cint;
    frameless: cint;
    transparent: cint;
    devtools: cint;
    modal: cint;
  end;
{$pop}

  PNVWindowCfg = ^TNVWindowCfg;

function nv_app_create: PNVApp; cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_app_create';
  {$ELSE}
  external name 'nv_app_create';
  {$ENDIF}

procedure nv_app_run(app: PNVApp); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_app_run';
  {$ELSE}
  external name 'nv_app_run';
  {$ENDIF}

procedure nv_app_quit(app: PNVApp); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_app_quit';
  {$ELSE}
  external name 'nv_app_quit';
  {$ENDIF}

procedure nv_app_destroy(app: PNVApp); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_app_destroy';
  {$ELSE}
  external name 'nv_app_destroy';
  {$ENDIF}

function nv_window_create(app: PNVApp; const cfg: PNVWindowCfg): PNVWindow; cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_create';
  {$ELSE}
  external name 'nv_window_create';
  {$ENDIF}

procedure nv_window_close(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_close';
  {$ELSE}
  external name 'nv_window_close';
  {$ENDIF}

procedure nv_window_destroy(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_destroy';
  {$ELSE}
  external name 'nv_window_destroy';
  {$ENDIF}

procedure nv_window_show(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_show';
  {$ELSE}
  external name 'nv_window_show';
  {$ENDIF}

procedure nv_window_hide(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_hide';
  {$ELSE}
  external name 'nv_window_hide';
  {$ENDIF}

procedure nv_window_set_title(win: PNVWindow; title: PAnsiChar); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_set_title';
  {$ELSE}
  external name 'nv_window_set_title';
  {$ENDIF}

procedure nv_window_set_size(win: PNVWindow; width: cint; height: cint); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_set_size';
  {$ELSE}
  external name 'nv_window_set_size';
  {$ENDIF}

procedure nv_window_set_min_size(win: PNVWindow; width: cint; height: cint); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_set_min_size';
  {$ELSE}
  external name 'nv_window_set_min_size';
  {$ENDIF}

procedure nv_window_center(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_center';
  {$ELSE}
  external name 'nv_window_center';
  {$ENDIF}

procedure nv_window_fullscreen(win: PNVWindow; enable: cint); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_fullscreen';
  {$ELSE}
  external name 'nv_window_fullscreen';
  {$ENDIF}

function nv_window_is_fullscreen(win: PNVWindow): cint; cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_is_fullscreen';
  {$ELSE}
  external name 'nv_window_is_fullscreen';
  {$ENDIF}

procedure nv_window_minimize(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_minimize';
  {$ELSE}
  external name 'nv_window_minimize';
  {$ENDIF}

procedure nv_window_maximize(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_maximize';
  {$ELSE}
  external name 'nv_window_maximize';
  {$ENDIF}

procedure nv_window_restore(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_restore';
  {$ELSE}
  external name 'nv_window_restore';
  {$ENDIF}

procedure nv_window_focus(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_focus';
  {$ELSE}
  external name 'nv_window_focus';
  {$ENDIF}

function nv_window_is_focused(win: PNVWindow): cint; cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_is_focused';
  {$ELSE}
  external name 'nv_window_is_focused';
  {$ENDIF}

procedure nv_window_set_resizable(win: PNVWindow; enable: cint); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_set_resizable';
  {$ELSE}
  external name 'nv_window_set_resizable';
  {$ENDIF}

procedure nv_window_set_always_on_top(win: PNVWindow; enable: cint); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_set_always_on_top';
  {$ELSE}
  external name 'nv_window_set_always_on_top';
  {$ENDIF}

procedure nv_window_set_opacity(win: PNVWindow; opacity_pct: cint); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_set_opacity';
  {$ELSE}
  external name 'nv_window_set_opacity';
  {$ENDIF}

procedure nv_window_set_zoom_factor(win: PNVWindow; factor: cdouble); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_set_zoom_factor';
  {$ELSE}
  external name 'nv_window_set_zoom_factor';
  {$ENDIF}

procedure nv_window_request_close(win: PNVWindow); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_request_close';
  {$ELSE}
  external name 'nv_window_request_close';
  {$ENDIF}

procedure nv_window_on_close(win: PNVWindow; cb: PNVCloseCB; userdata: Pointer); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_on_close';
  {$ELSE}
  external name 'nv_window_on_close';
  {$ENDIF}

procedure nv_load_url(win: PNVWindow; url: PAnsiChar); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_load_url';
  {$ELSE}
  external name 'nv_load_url';
  {$ENDIF}

procedure nv_load_html(win: PNVWindow; html: PAnsiChar; base_url: PAnsiChar); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_load_html';
  {$ELSE}
  external name 'nv_load_html';
  {$ENDIF}

procedure nv_load_html_ref(win: PNVWindow; html: PAnsiChar; len: csize_t;
  base_url: PAnsiChar); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_load_html_ref';
  {$ELSE}
  external name 'nv_load_html_ref';
  {$ENDIF}

procedure nv_eval_js(win: PNVWindow; js: PAnsiChar); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_eval_js';
  {$ELSE}
  external name 'nv_eval_js';
  {$ENDIF}

procedure nv_eval_js_batch(win: PNVWindow; scripts: Pointer {char **}; count: csize_t); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_eval_js_batch';
  {$ELSE}
  external name 'nv_eval_js_batch';
  {$ENDIF}

procedure nv_on_message(win: PNVWindow; cb: PNVMsgCB; userdata: Pointer); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_on_message';
  {$ELSE}
  external name 'nv_on_message';
  {$ENDIF}

procedure nv_on_ready(win: PNVWindow; cb: PNVReadyCB; userdata: Pointer); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_on_ready';
  {$ELSE}
  external name 'nv_on_ready';
  {$ENDIF}

procedure nv_send(win: PNVWindow; event_: PAnsiChar; json: PAnsiChar); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_send';
  {$ELSE}
  external name 'nv_send';
  {$ENDIF}

function nv_window_register_hotkey(win: PNVWindow; id: PAnsiChar; combo: PAnsiChar): cint; cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_register_hotkey';
  {$ELSE}
  external name 'nv_window_register_hotkey';
  {$ENDIF}

procedure nv_window_unregister_hotkey(win: PNVWindow; id: PAnsiChar); cdecl;
  {$IFDEF NATIVEVIEW_SHARED}
  external 'nativeview' name 'nv_window_unregister_hotkey';
  {$ELSE}
  external name 'nv_window_unregister_hotkey';
  {$ENDIF}

implementation

end.
