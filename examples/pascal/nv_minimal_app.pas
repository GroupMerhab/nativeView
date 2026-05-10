{
  nv_minimal_app.pas — Free Pascal + nativeview (same ideas as examples/hello/main.c).

  - Inline HTML UI with a counter (Increment / Reset → native, counter_update → JS)
  - nv_on_ready / nv_on_message / nv_window_on_close + nv_app_quit
  - UTF-8 strings kept alive while pointers are passed (see nv.h / docs/Pascal.md)

  SPDX-License-Identifier: Apache-2.0
}

unit nv_minimal_app;

{$mode objfpc}{$H+}

interface

{ cdecl so the shared-library export matches plain C dlsym / static link names. }
procedure RunNativeView; cdecl;

implementation

uses
  SysUtils,
  nativeview;

var
  gTitle: UTF8String;
  gBootHtml: UTF8String;
  gCounter: Integer;
  gSendEvent: UTF8String;
  gSendPayload: UTF8String;

procedure OnReady(win: PNVWindow; userdata: Pointer); cdecl;
begin
  if (win <> nil) or (userdata <> nil) then
    ;
  Writeln('[nv_minimal] window ready');
end;

procedure OnMessage(win: PNVWindow; event_, json: PAnsiChar; userdata: Pointer); cdecl;
var
  ev: AnsiString;
  js: AnsiString;
begin
  if (win <> nil) or (userdata <> nil) then
    ;
  if (win = nil) or (event_ = nil) or (json = nil) then
    Exit;
  ev := AnsiString(event_);
  if ev <> 'counter' then
    Exit;
  js := AnsiString(json);
  if Pos('increment', js) > 0 then
    Inc(gCounter)
  else if Pos('reset', js) > 0 then
    gCounter := 0;

  gSendEvent := 'counter_update';
  gSendPayload := Format('{"value":%d}', [gCounter]);
  nv_send(win, PAnsiChar(gSendEvent), PAnsiChar(gSendPayload));
  Writeln('[nv_minimal] on_message -> ', gSendPayload);
end;

procedure OnClose(win: PNVWindow; userdata: Pointer); cdecl;
var
  app: PNVApp;
begin
  if win <> nil then
    ;
  app := PNVApp(userdata);
  Writeln('[nv_minimal] window close -> app quit');
  if app <> nil then
    nv_app_quit(app);
end;

procedure RunNativeView; cdecl;
var
  app: PNVApp;
  win: PNVWindow;
  cfg: TNVWindowCfg;
begin
  gCounter := 0;

  app := nv_app_create;
  if app = nil then
    Halt(1);

  gTitle := 'nativeview · Free Pascal';
  gBootHtml :=
    '<!doctype html><html><head><meta charset="utf-8"><title>Pascal + nativeview</title>' +
    '<style>' +
    'body{font-family:system-ui,sans-serif;display:flex;flex-direction:column;' +
    'align-items:center;justify-content:center;height:100vh;margin:0;background:#111;color:#eee}' +
    'button{margin:0.5rem;padding:0.5rem 1rem;font-size:1rem;cursor:pointer}' +
    '#value{font-size:3rem;margin-bottom:1rem}' +
    '.hint{font-size:0.85rem;opacity:0.75;margin-top:1.5rem;max-width:28rem;text-align:center}' +
    '</style></head><body>' +
    '<div id="value">0</div>' +
    '<div>' +
    '<button type="button" onclick="window.__nv.send(''counter'',JSON.stringify({action:''increment''}))">Increment</button>' +
    '<button type="button" onclick="window.__nv.send(''counter'',JSON.stringify({action:''reset''}))">Reset</button>' +
    '</div>' +
    '<p class="hint">Buttons call into Pascal via <code>window.__nv.send</code>; ' +
    'the native side answers with <code>counter_update</code>.</p>' +
    '<script>' +
    'var display=document.getElementById("value");' +
    'window.__nv.on("counter_update",function(d){display.textContent=d.value;});' +
    '</script>' +
    '</body></html>';

  FillChar(cfg, SizeOf(cfg), 0);
  cfg.title := PAnsiChar(gTitle);
  cfg.width := 800;
  cfg.height := 600;
  cfg.min_width := 400;
  cfg.min_height := 300;
  cfg.resizable := 1;
  cfg.frameless := 0;
  cfg.transparent := 0;
  cfg.devtools := 1;
  cfg.modal := 0;

  win := nv_window_create(app, @cfg);
  if win = nil then
  begin
    nv_app_destroy(app);
    Halt(1);
  end;

  nv_on_ready(win, @OnReady, nil);
  nv_on_message(win, @OnMessage, nil);
  nv_window_on_close(win, @OnClose, app);
  nv_load_html(win, PAnsiChar(gBootHtml), PAnsiChar('about:blank'));
  nv_window_show(win);

  Writeln('[nv_minimal] app run (close the window to exit)');
  nv_app_run(app);
  nv_app_destroy(app);
end;

end.
