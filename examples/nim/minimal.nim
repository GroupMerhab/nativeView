## Minimal nativeview sample: `nv_on_ready` → `nv_load_html`, `nv_eval_js_batch`, teardown.
## Build with `examples/nim/build_static.sh` (Linux) or follow `docs/Nim.md`.
##
## SPDX-License-Identifier: Apache-2.0

import nativeview

const Html = cstring"""<!doctype html>
<html><head><meta charset="utf-8"><title>Nim + nativeview</title></head>
<body><p id="t">loading…</p>
<script>window.__nv.on('ping', function(d){ document.getElementById('t').textContent = d.msg; });</script>
</body></html>"""

var gWindowTitle: cstring = cstring"nativeview (Nim minimal)"

proc onReady(win: NvWindow; userdata: pointer) {.cdecl.} =
  discard userdata
  nv_load_html(win, Html, cstring"about:blank")
  var scripts: array[2, cstring] = [cstring"document.title='nim'", cstring"void 0"]
  nv_eval_js_batch(win, cast[ptr UncheckedArray[cstring]](addr scripts[0]), csize_t(scripts.len))
  nv_send(win, cstring"ping", cstring"""{"msg":"hello from Nim"}""")

proc onMessage(win: NvWindow; event, json: cstring; userdata: pointer) {.cdecl.} =
  discard win; discard event; discard json; discard userdata

proc onClose(win: NvWindow; userdata: pointer) {.cdecl.} =
  discard win
  nv_app_quit(cast[NvApp](userdata))

proc main =
  let ver = nv_version_string()
  if ver != nil:
    echo $ver

  var info: NvVersionInfo
  nv_get_version_info(addr info)
  echo "nativeview ", info.major, ".", info.minor, ".", info.patch

  let app = nv_app_create()
  if cast[pointer](app) == nil:
    echo "nv_app_create failed"
    quit QuitFailure

  var cfg = NvWindowCfg(
    title: gWindowTitle,
    width: 800,
    height: 600,
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
    echo "nv_window_create failed"
    nv_app_destroy(app)
    quit QuitFailure

  nv_on_ready(win, onReady, nil)
  nv_on_message(win, onMessage, nil)
  nv_window_on_close(win, onClose, cast[pointer](app))

  nv_window_show(win)
  nv_app_run(app)
  nv_app_destroy(app)

when isMainModule:
  main()
