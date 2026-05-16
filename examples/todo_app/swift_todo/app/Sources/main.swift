// SPDX-License-Identifier: Apache-2.0
// Todo app: Vue UI + SQLite + nativeview (Swift; port of c_todo/todo_app.c).

#if canImport(Darwin)
  import Darwin
#else
  import Glibc
#endif
import Nativeview
import TodoBackend
import TodoHtmlEmbed

private final class TodoAppContext {
  let store: TodoStore
  init(store: TodoStore) {
    self.store = store
  }
}

private final class CloseUserData {
  var app: OpaquePointer?
}

private var gTodoCtx: TodoAppContext?

private func sendJson(_ win: OpaquePointer?, event: String, json: String) {
  event.withCString { ePtr in
    json.withCString { jPtr in
      nv_send(win, ePtr, jPtr)
    }
  }
}

private func onReady(_ win: OpaquePointer?, _ userdata: UnsafeMutableRawPointer?) {
  _ = win
  _ = userdata
  fputs("[todo_app] webview ready\n", stderr)
}

private func onMessage(
  _ win: OpaquePointer?,
  _ event: UnsafePointer<CChar>?,
  _ json: UnsafePointer<CChar>?,
  _ userdata: UnsafeMutableRawPointer?
) {
  _ = userdata
  guard let win else { return }
  guard let ctx = gTodoCtx else { return }
  guard let event, String(cString: event) == "todo" else { return }
  guard let json else { return }
  let body = String(cString: json)
  let emit: TodoBridgeEmit = { ev, j in
    sendJson(win, event: ev, json: j)
  }
  _ = bridgeHandleWire(ctx.store, rawIpcJson: body, emit: emit)
}

private func onClose(_ win: OpaquePointer?, _ userdata: UnsafeMutableRawPointer?) {
  _ = win
  guard let userdata else { return }
  let box = Unmanaged<CloseUserData>.fromOpaque(userdata).takeUnretainedValue()
  nv_app_quit(box.app)
}

@main
enum TodoEntry {
  static func main() {
    let args = CommandLine.arguments.dropFirst()
    let dbPath: String?
    if let first = args.first, !first.isEmpty {
      dbPath = first
    } else {
      dbPath = "todo_app.db"
    }

    guard let store = TodoStore.open(path: dbPath) else {
      fputs("[todo_app] todo_store_open failed\n", stderr)
      exit(1)
    }
    defer { store.close() }

    guard let app = nv_app_create() else {
      fputs("[todo_app] nv_app_create failed\n", stderr)
      exit(1)
    }
    defer { nv_app_destroy(app) }

    var windowTitle = ContiguousArray("Todo App\0".utf8CString)
    var cfg = nv_window_cfg_t()
    windowTitle.withUnsafeMutableBufferPointer { titleBuf in
      cfg.title = titleBuf.baseAddress.map { UnsafePointer($0) }
      cfg.width = 960
      cfg.height = 720
      cfg.min_width = 0
      cfg.min_height = 0
      cfg.resizable = 1
      cfg.frameless = 0
      cfg.transparent = 0
      cfg.devtools = 1
      cfg.modal = 0
    }

    guard let win = nv_window_create(app, &cfg) else {
      fputs("[todo_app] nv_window_create failed\n", stderr)
      exit(1)
    }
    defer { nv_window_destroy(win) }

    gTodoCtx = TodoAppContext(store: store)
    defer { gTodoCtx = nil }

    let closeBox = CloseUserData()
    closeBox.app = app
    let closeRaw = Unmanaged.passUnretained(closeBox).toOpaque()

    nv_on_ready(win, onReady, nil)
    nv_on_message(win, onMessage, nil)
    nv_window_on_close(win, onClose, closeRaw)

    // Load before show (same as nim_todo / c_todo). Doing nv_load_html_ref inside onReady
    // re-fires ready and loops forever.
    TodoHtmlEmbed.bytes.withUnsafeBufferPointer { buf in
      let htmlPtr = UnsafeRawPointer(buf.baseAddress!).assumingMemoryBound(to: CChar.self)
      "about:blank\0".utf8CString.withUnsafeBufferPointer { aboutBuf in
        nv_load_html_ref(win, htmlPtr, buf.count, aboutBuf.baseAddress)
      }
    }

    nv_window_show(win)
    nv_app_run(app)
  }
}
