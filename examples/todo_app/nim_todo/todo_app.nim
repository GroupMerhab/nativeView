## Todo app: Vue UI + SQLite + nativeview (Nim port of c_todo/src/todo_app.c).
## SPDX-License-Identifier: Apache-2.0

import std/os
import nativeview
import ./todo_store
import ./todo_bridge
import ./generated/todo_html_embed

proc allocC(s: string): cstring =
  result = cast[cstring](allocShared0(s.len + 1))
  if s.len > 0:
    copyMem(cast[pointer](result), unsafeAddr s[0], s.len)

proc freeC(p: cstring) =
  if p != nil:
    deallocShared(cast[pointer](p))

proc sendJson(win: NvWindow; ev, j: string) =
  let e = allocC(ev)
  let b = allocC(j)
  nv_send(win, e, b)
  freeC(e)
  freeC(b)

type AppCtx = object
  app: NvApp
  win: NvWindow
  store: TodoStore

var gCtx: AppCtx

proc onReady(win: NvWindow; userdata: pointer) {.cdecl.} =
  discard win
  discard userdata
  stderr.writeLine "[todo_app] webview ready\n"

proc onMessage(win: NvWindow; event, json: cstring; userdata: pointer) {.cdecl.} =
  let ctx = cast[ptr AppCtx](userdata)
  if ctx == nil or ctx.store.isNil or json == nil:
    return
  if event == nil or $event != "todo":
    return
  let body = $json
  proc emitProc(ev: string; j: string) {.closure.} =
    sendJson(win, ev, j)
  discard bridgeHandleWire(ctx.store, body, emitProc)

proc onClose(win: NvWindow; userdata: pointer) {.cdecl.} =
  discard win
  nv_app_quit(cast[NvApp](userdata))

proc main =
  let dbPath = if paramCount() >= 1 and paramStr(1).len > 0: paramStr(1) else: "todo_app.db"
  let store = storeOpen(dbPath)
  if store.isNil:
    stderr.writeLine "[todo_app] todo_store_open failed for ", dbPath
    quit QuitFailure

  let app = nv_app_create()
  if cast[pointer](app) == nil:
    stderr.writeLine "[todo_app] nv_app_create failed"
    storeClose(store)
    quit QuitFailure

  var cfg = NvWindowCfg(
    title: cstring"Todo App",
    width: 960,
    height: 720,
    min_width: 0,
    min_height: 0,
    resizable: 1,
    frameless: 0,
    transparent: 0,
    devtools: 1,
    modal: 0,
  )

  let win = nv_window_create(app, addr cfg)
  if cast[pointer](win) == nil:
    stderr.writeLine "[todo_app] nv_window_create failed"
    storeClose(store)
    nv_app_destroy(app)
    quit QuitFailure

  gCtx = AppCtx(app: app, win: win, store: store)

  nv_on_ready(win, onReady, nil)
  nv_on_message(win, onMessage, addr gCtx)
  nv_window_on_close(win, onClose, cast[pointer](app))

  nv_load_html_ref(win, cast[cstring](unsafeAddr nvTodoHtmlData[0]),
    csize_t(nvTodoHtmlLen), cstring"about:blank")
  nv_window_show(win)

  nv_app_run(app)

  storeClose(store)
  nv_app_destroy(app)

when isMainModule:
  main()
