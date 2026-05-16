// SPDX-License-Identifier: Apache-2.0
// Port of c_todo/tests/test_todo_store.c

import SQLite3
import XCTest

@testable import TodoBackend

final class TodoStoreTests: XCTestCase {
  func testStoreCrud() throws {
    guard let s = TodoStore.open(path: nil) else {
      XCTFail("todo_store_open")
      return
    }
    defer { s.close() }

    var cat1: Int64 = 0
    XCTAssertEqual(s.categoryInsert(name: "Work", color: "#ff00aa", outId: &cat1), SQLITE_OK)
    XCTAssertGreaterThan(cat1, 0)

    var dup: Int64 = 0
    XCTAssertNotEqual(s.categoryInsert(name: "Work", color: "#000000", outId: &dup), SQLITE_OK)

    var catCount = 0
    XCTAssertEqual(
      s.categoriesForeach { _ in catCount += 1 },
      SQLITE_OK
    )
    XCTAssertEqual(catCount, 1)

    var tid: Int64 = 0
    XCTAssertEqual(
      s.todoInsert(
        title: "Task one",
        description: "",
        status: "pending",
        priority: "high",
        categoryIdSet: true,
        categoryId: cat1,
        parentIdSet: false,
        parentId: 0,
        dueDateSet: false,
        dueDate: 0,
        outId: &tid
      ),
      SQLITE_OK
    )
    XCTAssertGreaterThan(tid, 0)

    var todoCount = 0
    XCTAssertEqual(s.todosForeach { _ in todoCount += 1 }, SQLITE_OK)
    XCTAssertEqual(todoCount, 1)

    XCTAssertEqual(
      s.todoUpdateFull(
        id: tid,
        title: "Task one",
        description: "desc",
        status: "in_progress",
        priority: "low",
        categoryIdSet: false,
        categoryId: 0,
        parentIdSet: false,
        parentId: 0,
        dueDateSet: false,
        dueDate: 0,
        position: 0
      ),
      SQLITE_OK
    )

    XCTAssertEqual(
      s.todoUpdateFull(
        id: tid,
        title: "Task one",
        description: "desc",
        status: "archived",
        priority: "low",
        categoryIdSet: false,
        categoryId: 0,
        parentIdSet: false,
        parentId: 0,
        dueDateSet: false,
        dueDate: 0,
        position: 0
      ),
      SQLITE_OK
    )

    XCTAssertEqual(s.todoDeleteById(tid), SQLITE_OK)
    todoCount = 0
    XCTAssertEqual(s.todosForeach { _ in todoCount += 1 }, SQLITE_OK)
    XCTAssertEqual(todoCount, 0)

    XCTAssertEqual(s.categoryDeleteById(cat1), SQLITE_OK)
    catCount = 0
    XCTAssertEqual(s.categoriesForeach { _ in catCount += 1 }, SQLITE_OK)
    XCTAssertEqual(catCount, 0)

    var bad: Int64 = 0
    XCTAssertNotEqual(s.categoryInsert(name: "", color: "#fff", outId: &bad), SQLITE_OK)
    XCTAssertNotEqual(
      s.todoInsert(
        title: "",
        description: "x",
        status: "pending",
        priority: "medium",
        categoryIdSet: false,
        categoryId: 0,
        parentIdSet: false,
        parentId: 0,
        dueDateSet: false,
        dueDate: 0,
        outId: &tid
      ),
      SQLITE_OK
    )
  }
}
