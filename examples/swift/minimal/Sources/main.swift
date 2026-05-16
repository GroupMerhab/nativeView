// SPDX-License-Identifier: Apache-2.0
import Nativeview

private var gWindowTitle: ContiguousArray<CChar> = ContiguousArray(
  "nativeview (Swift minimal)\0".utf8CString)

private let htmlBytes: ContiguousArray<CChar> = ContiguousArray(
  """
  <!doctype html>
  <html><head><meta charset="utf-8"><title>Swift + nativeview</title></head>
  <body><p id="t">loading…</p>
  <script>window.__nv.on('ping', function(d){ document.getElementById('t').textContent = d.msg; });</script>
  </body></html>
  \0
  """.utf8CString)

private func onReady(_ win: OpaquePointer?, _ userdata: UnsafeMutableRawPointer?) {
  _ = userdata
  guard let win else { return }
  htmlBytes.withUnsafeBufferPointer { htmlBuf in
    let about = "about:blank\0".utf8CString
    about.withUnsafeBufferPointer { aboutBuf in
      nv_load_html(win, htmlBuf.baseAddress, aboutBuf.baseAddress)
    }
  }

  var s0 = "document.title='swift'\0".utf8CString
  var s1 = "void 0\0".utf8CString
  s0.withUnsafeMutableBufferPointer { b0 in
    s1.withUnsafeMutableBufferPointer { b1 in
      guard let p0 = b0.baseAddress, let p1 = b1.baseAddress else { return }
      var ptrs: [UnsafePointer<CChar>?] = [
        UnsafePointer(p0),
        UnsafePointer(p1),
      ]
      ptrs.withUnsafeMutableBufferPointer { scripts in
        nv_eval_js_batch(win, scripts.baseAddress!, 2)
      }
    }
  }

  let ev = "ping\0".utf8CString
  let json = #"{"msg":"hello from Swift"}"#.utf8CString
  ev.withUnsafeBufferPointer { evBuf in
    json.withUnsafeBufferPointer { jsonBuf in
      nv_send(win, evBuf.baseAddress, jsonBuf.baseAddress)
    }
  }
}

private func onMessage(
  _ win: OpaquePointer?,
  _ event: UnsafePointer<CChar>?,
  _ json: UnsafePointer<CChar>?,
  _ userdata: UnsafeMutableRawPointer?
) {
  _ = win
  _ = event
  _ = json
  _ = userdata
}

private func onClose(_ win: OpaquePointer?, _ userdata: UnsafeMutableRawPointer?) {
  _ = win
  guard let userdata else { return }
  let app = userdata.assumingMemoryBound(to: OpaquePointer?.self).pointee
  nv_app_quit(app)
}

@main
enum Entry {
  static func main() {
    if let ver = nv_version_string() {
      print(String(cString: ver))
    }

    var info = NvVersionInfo()
    nv_get_version_info(&info)
    print("nativeview \(info.major).\(info.minor).\(info.patch)")

    guard let app = nv_app_create() else {
      fatalError("nv_app_create failed")
    }
    defer { nv_app_destroy(app) }

    var cfg = nv_window_cfg_t()
    gWindowTitle.withUnsafeMutableBufferPointer { titleBuf in
      cfg.title = titleBuf.baseAddress.map { UnsafePointer($0) }
      cfg.width = 800
      cfg.height = 600
      cfg.min_width = 0
      cfg.min_height = 0
      cfg.resizable = 1
      cfg.frameless = 0
      cfg.transparent = 0
      cfg.devtools = 1
      cfg.modal = 0
    }

    guard let win = nv_window_create(app, &cfg) else {
      fatalError("nv_window_create failed")
    }
    defer { nv_window_destroy(win) }

    var appStorage: OpaquePointer? = app
    nv_on_ready(win, onReady, nil)
    nv_on_message(win, onMessage, nil)
    withUnsafeMutablePointer(to: &appStorage) { appPtr in
      nv_window_on_close(win, onClose, UnsafeMutableRawPointer(appPtr))
    }

    nv_window_show(win)
    nv_app_run(app)
  }
}
