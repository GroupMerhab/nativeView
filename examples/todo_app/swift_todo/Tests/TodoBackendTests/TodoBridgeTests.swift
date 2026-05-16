// SPDX-License-Identifier: Apache-2.0
// Port of c_todo/tests/test_todo_bridge.c

import XCTest

@testable import TodoBackend

final class TodoBridgeTests: XCTestCase {
  private var lastEvent = ""
  private var lastJson = ""

  private func captureEmit(event: String, json: String) {
    lastEvent = event
    lastJson = json
  }

  private func countJsonArrayElems(_ arrJson: String) -> Int {
    guard let data = arrJson.data(using: .utf8),
      let v = try? JSONSerialization.jsonObject(with: data) as? [Any]
    else { return -1 }
    return v.count
  }

  func testBridgeWire() throws {
    guard let s = TodoStore.open(path: nil) else {
      XCTFail("store")
      return
    }
    defer { s.close() }

    lastEvent = ""
    lastJson = ""
    let wireList = #"{"e":"todo","s":0,"d":{"action":"categories.list"}}"#
    XCTAssertEqual(bridgeHandleWire(s, rawIpcJson: wireList, emit: captureEmit), 0)
    XCTAssertEqual(lastEvent, "categories.list")
    XCTAssertEqual(lastJson, "[]")

    let wireMk =
      ##"{"e":"todo","s":0,"d":{"action":"categories.create","payload":{"name":"A","color":"#112233"}}}"##
    XCTAssertEqual(bridgeHandleWire(s, rawIpcJson: wireMk, emit: captureEmit), 0)
    XCTAssertEqual(lastEvent, "categories.list")
    XCTAssertEqual(countJsonArrayElems(lastJson), 1)

    let wireTodo =
      #"{"e":"todo","s":0,"d":{"action":"todos.create","payload":{"title":"T1","description":"","status":"pending","priority":"medium","categoryId":1,"parentId":0,"dueDate":0}}}"#
    XCTAssertEqual(bridgeHandleWire(s, rawIpcJson: wireTodo, emit: captureEmit), 0)
    XCTAssertEqual(lastEvent, "todos.list")
    XCTAssertEqual(countJsonArrayElems(lastJson), 1)

    guard let data = lastJson.data(using: .utf8),
      let arr = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]],
      let first = arr.first,
      let tid = first["id"] as? Int64 ?? (first["id"] as? Int).map(Int64.init)
    else {
      XCTFail("parse todos")
      return
    }

    let delbuf =
      "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.delete\",\"payload\":{\"id\":\(tid)}}}"
    XCTAssertEqual(bridgeHandleWire(s, rawIpcJson: delbuf, emit: captureEmit), 0)
    XCTAssertEqual(countJsonArrayElems(lastJson), 0)
  }
}
