// SPDX-License-Identifier: Apache-2.0
// SQLite persistence (port of c_todo/src/todo_store.c).

import Foundation
import SQLite3

private let SQLITE_TRANSIENT = unsafeBitCast(-1, to: sqlite3_destructor_type.self)

private let titleMax = 255
private let descMax = 4096
private let nameMax = 64
private let colorMax = 32

public struct TodoStoreCategoryRow: Sendable {
  public var id: Int64
  public var name: String
  public var color: String
}

public struct TodoStoreTodoRow: Sendable {
  public var id: Int64
  public var title: String
  public var description: String
  public var status: String
  public var priority: String
  public var categoryIdIsNull: Bool
  public var categoryId: Int64
  public var parentIdIsNull: Bool
  public var parentId: Int64
  public var dueDateIsNull: Bool
  public var dueDate: Int64
  public var position: Int
  public var createdAt: Int64
  public var updatedAt: Int64
}

public final class TodoStore: @unchecked Sendable {
  private var db: OpaquePointer?

  private static let schemaDdlV3 = """
CREATE TABLE IF NOT EXISTS categories (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 64),
  color TEXT NOT NULL DEFAULT '#6366f1' CHECK(length(color) BETWEEN 1 AND 32)
);
CREATE TABLE IF NOT EXISTS todos (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),
  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),
  status TEXT NOT NULL DEFAULT 'pending'
    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),
  priority TEXT NOT NULL DEFAULT 'medium'
    CHECK(priority IN ('low','medium','high','urgent')),
  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,
  due_date INTEGER,
  position INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL DEFAULT (unixepoch()),
  updated_at INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);
CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);
CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);
"""

  private static let migrateV2ToV3 = """
CREATE TABLE todos_new (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),
  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),
  status TEXT NOT NULL DEFAULT 'pending'
    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),
  priority TEXT NOT NULL DEFAULT 'medium'
    CHECK(priority IN ('low','medium','high','urgent')),
  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,
  due_date INTEGER,
  position INTEGER NOT NULL DEFAULT 0,
  created_at INTEGER NOT NULL DEFAULT (unixepoch()),
  updated_at INTEGER NOT NULL DEFAULT (unixepoch())
);
INSERT INTO todos_new(id, title, description, status, priority, category_id, parent_id, due_date,
 position, created_at, updated_at)
SELECT id, title, description, status, priority, category_id, parent_id, due_date, position,
created_at, updated_at FROM todos;
DROP TABLE todos;
ALTER TABLE todos_new RENAME TO todos;
CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);
CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);
CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);
PRAGMA user_version = 3;
"""

  /// `nil` or empty path opens an in-memory database (for tests).
  public static func open(path: String?) -> TodoStore? {
    let uri: String
    if let path, !path.isEmpty { uri = path }
    else { uri = ":memory:" }
    var handle: OpaquePointer?
    let flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX
    guard sqlite3_open_v2(uri, &handle, flags, nil) == SQLITE_OK, let db = handle else {
      if let h = handle { sqlite3_close(h) }
      fputs("[todo_store] open failed\n", stderr)
      return nil
    }
    sqlite3_busy_timeout(db, 5000)
    let s = TodoStore(db: db)
    guard s.ensureSchema() == SQLITE_OK else {
      sqlite3_close(db)
      return nil
    }
    return s
  }

  private init(db: OpaquePointer) {
    self.db = db
  }

  deinit {
    close()
  }

  public func close() {
    if let db {
      sqlite3_close(db)
      self.db = nil
    }
  }

  private func execSql(_ sql: String) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    var err: UnsafeMutablePointer<CChar>?
    let rc = sqlite3_exec(db, sql, nil, nil, &err)
    if rc != SQLITE_OK, let e = err {
      fputs("[todo_store] \(String(cString: e))\n", stderr)
      sqlite3_free(err)
    }
    return rc
  }

  private func readUserVersion() -> Int {
    guard let db else { return -1 }
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &st, nil) == SQLITE_OK,
      let stmt = st
    else { return -1 }
    defer { sqlite3_finalize(stmt) }
    guard sqlite3_step(stmt) == SQLITE_ROW else { return -1 }
    return Int(sqlite3_column_int(stmt, 0))
  }

  private func ensureSchema() -> Int32 {
    guard db != nil else { return SQLITE_MISUSE }
    var ver = readUserVersion()
    if ver < 0 { return SQLITE_ERROR }
    guard execSql("PRAGMA foreign_keys = ON;") == SQLITE_OK else { return SQLITE_ERROR }
    _ = execSql("PRAGMA journal_mode=WAL;")
    if ver == 1 {
      guard execSql("DROP TABLE IF EXISTS todos;") == SQLITE_OK else { return SQLITE_ERROR }
    }
    if ver == 2 {
      guard execSql(Self.migrateV2ToV3) == SQLITE_OK else { return SQLITE_ERROR }
      return SQLITE_OK
    }
    guard execSql(Self.schemaDdlV3) == SQLITE_OK else { return SQLITE_ERROR }
    ver = readUserVersion()
    if ver < 3 {
      guard execSql("PRAGMA user_version = 3;") == SQLITE_OK else { return SQLITE_ERROR }
    }
    return SQLITE_OK
  }

  private func validateNameColor(_ name: String, _ color: String) -> Int32 {
    guard !name.isEmpty, !color.isEmpty else { return SQLITE_MISUSE }
    let nl = name.count
    let cl = color.count
    if nl < 1 || nl > nameMax { return SQLITE_RANGE }
    if cl < 1 || cl > colorMax { return SQLITE_RANGE }
    return SQLITE_OK
  }

  private func validateTodoTexts(_ title: String, _ description: String, _ status: String, _ priority: String)
    -> Int32
  {
    guard !status.isEmpty, !priority.isEmpty else { return SQLITE_MISUSE }
    let tl = title.count
    let dl = description.count
    if tl < 1 || tl > titleMax { return SQLITE_RANGE }
    if dl > descMax { return SQLITE_RANGE }
    let okStatus = ["pending", "in_progress", "done", "cancelled", "archived"]
    let okPri = ["low", "medium", "high", "urgent"]
    guard okStatus.contains(status), okPri.contains(priority) else { return SQLITE_CONSTRAINT }
    return SQLITE_OK
  }

  public func categoriesForeach(_ body: (TodoStoreCategoryRow) -> Void) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    let q = "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;"
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, q, -1, &st, nil) == SQLITE_OK, let stmt = st else {
      return sqlite3_errcode(db)
    }
    defer { sqlite3_finalize(stmt) }
    var rc = sqlite3_step(stmt)
    while rc == SQLITE_ROW {
      let r = TodoStoreCategoryRow(
        id: sqlite3_column_int64(stmt, 0),
        name: String(cString: sqlite3_column_text(stmt, 1)),
        color: String(cString: sqlite3_column_text(stmt, 2))
      )
      body(r)
      rc = sqlite3_step(stmt)
    }
    return rc == SQLITE_DONE ? SQLITE_OK : rc
  }

  public func categoryInsert(name: String, color: String, outId: inout Int64) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    let v = validateNameColor(name, color)
    guard v == SQLITE_OK else { return v }
    let q = "INSERT INTO categories(name, color) VALUES(?, ?);"
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, q, -1, &st, nil) == SQLITE_OK, let stmt = st else {
      return sqlite3_errcode(db)
    }
    defer { sqlite3_finalize(stmt) }
    var rc: Int32 = SQLITE_OK
    name.withCString { p in
      rc = sqlite3_bind_text(stmt, 1, p, -1, SQLITE_TRANSIENT)
    }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    color.withCString { p in
      rc = sqlite3_bind_text(stmt, 2, p, -1, SQLITE_TRANSIENT)
    }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    let stepRc = sqlite3_step(stmt)
    guard stepRc == SQLITE_DONE else {
      return stepRc == SQLITE_ROW ? SQLITE_ERROR : stepRc
    }
    outId = sqlite3_last_insert_rowid(db)
    return SQLITE_OK
  }

  public func categoryDeleteById(_ id: Int64) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    let q = "DELETE FROM categories WHERE id = ?;"
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, q, -1, &st, nil) == SQLITE_OK, let stmt = st else {
      return sqlite3_errcode(db)
    }
    defer { sqlite3_finalize(stmt) }
    guard sqlite3_bind_int64(stmt, 1, id) == SQLITE_OK else { return sqlite3_errcode(db) }
    let stepRc = sqlite3_step(stmt)
    guard stepRc == SQLITE_DONE else {
      return stepRc == SQLITE_ROW ? SQLITE_ERROR : stepRc
    }
    return sqlite3_changes(db) > 0 ? SQLITE_OK : SQLITE_ERROR
  }

  public func todosForeach(_ body: (TodoStoreTodoRow) -> Void) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    let q =
      "SELECT id, title, description, status, priority, category_id, parent_id, due_date,"
      + " position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;"
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, q, -1, &st, nil) == SQLITE_OK, let stmt = st else {
      return sqlite3_errcode(db)
    }
    defer { sqlite3_finalize(stmt) }
    var rc = sqlite3_step(stmt)
    while rc == SQLITE_ROW {
      let catNull = sqlite3_column_type(stmt, 5) == SQLITE_NULL
      let parNull = sqlite3_column_type(stmt, 6) == SQLITE_NULL
      let dueNull = sqlite3_column_type(stmt, 7) == SQLITE_NULL
      let r = TodoStoreTodoRow(
        id: sqlite3_column_int64(stmt, 0),
        title: String(cString: sqlite3_column_text(stmt, 1)),
        description: String(cString: sqlite3_column_text(stmt, 2)),
        status: String(cString: sqlite3_column_text(stmt, 3)),
        priority: String(cString: sqlite3_column_text(stmt, 4)),
        categoryIdIsNull: catNull,
        categoryId: catNull ? 0 : sqlite3_column_int64(stmt, 5),
        parentIdIsNull: parNull,
        parentId: parNull ? 0 : sqlite3_column_int64(stmt, 6),
        dueDateIsNull: dueNull,
        dueDate: dueNull ? 0 : sqlite3_column_int64(stmt, 7),
        position: Int(sqlite3_column_int(stmt, 8)),
        createdAt: sqlite3_column_int64(stmt, 9),
        updatedAt: sqlite3_column_int64(stmt, 10)
      )
      body(r)
      rc = sqlite3_step(stmt)
    }
    return rc == SQLITE_DONE ? SQLITE_OK : rc
  }

  public func todoInsert(
    title: String,
    description: String,
    status: String,
    priority: String,
    categoryIdSet: Bool,
    categoryId: Int64,
    parentIdSet: Bool,
    parentId: Int64,
    dueDateSet: Bool,
    dueDate: Int64,
    outId: inout Int64
  ) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    let v = validateTodoTexts(title, description, status, priority)
    guard v == SQLITE_OK else { return v }
    let q =
      "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)"
      + " VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));"
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, q, -1, &st, nil) == SQLITE_OK, let stmt = st else {
      return sqlite3_errcode(db)
    }
    defer { sqlite3_finalize(stmt) }
    var rc: Int32 = SQLITE_OK
    title.withCString { p in rc = sqlite3_bind_text(stmt, 1, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    description.withCString { p in rc = sqlite3_bind_text(stmt, 2, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    status.withCString { p in rc = sqlite3_bind_text(stmt, 3, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    priority.withCString { p in rc = sqlite3_bind_text(stmt, 4, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    if categoryIdSet && categoryId > 0 {
      guard sqlite3_bind_int64(stmt, 5, categoryId) == SQLITE_OK else { return sqlite3_errcode(db) }
    } else {
      guard sqlite3_bind_null(stmt, 5) == SQLITE_OK else { return sqlite3_errcode(db) }
    }
    if parentIdSet && parentId > 0 {
      guard sqlite3_bind_int64(stmt, 6, parentId) == SQLITE_OK else { return sqlite3_errcode(db) }
    } else {
      guard sqlite3_bind_null(stmt, 6) == SQLITE_OK else { return sqlite3_errcode(db) }
    }
    if dueDateSet {
      guard sqlite3_bind_int64(stmt, 7, dueDate) == SQLITE_OK else { return sqlite3_errcode(db) }
    } else {
      guard sqlite3_bind_null(stmt, 7) == SQLITE_OK else { return sqlite3_errcode(db) }
    }
    let stepRc = sqlite3_step(stmt)
    guard stepRc == SQLITE_DONE else {
      return stepRc == SQLITE_ROW ? SQLITE_ERROR : stepRc
    }
    outId = sqlite3_last_insert_rowid(db)
    return SQLITE_OK
  }

  public func todoUpdateFull(
    id: Int64,
    title: String,
    description: String,
    status: String,
    priority: String,
    categoryIdSet: Bool,
    categoryId: Int64,
    parentIdSet: Bool,
    parentId: Int64,
    dueDateSet: Bool,
    dueDate: Int64,
    position: Int
  ) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    let v = validateTodoTexts(title, description, status, priority)
    guard v == SQLITE_OK else { return v }
    let q =
      "UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?"
      + ", due_date=?, position=?, updated_at=unixepoch() WHERE id=?;"
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, q, -1, &st, nil) == SQLITE_OK, let stmt = st else {
      return sqlite3_errcode(db)
    }
    defer { sqlite3_finalize(stmt) }
    var rc: Int32 = SQLITE_OK
    title.withCString { p in rc = sqlite3_bind_text(stmt, 1, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    description.withCString { p in rc = sqlite3_bind_text(stmt, 2, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    status.withCString { p in rc = sqlite3_bind_text(stmt, 3, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    priority.withCString { p in rc = sqlite3_bind_text(stmt, 4, p, -1, SQLITE_TRANSIENT) }
    guard rc == SQLITE_OK else { return sqlite3_errcode(db) }
    if categoryIdSet && categoryId > 0 {
      guard sqlite3_bind_int64(stmt, 5, categoryId) == SQLITE_OK else { return sqlite3_errcode(db) }
    } else {
      guard sqlite3_bind_null(stmt, 5) == SQLITE_OK else { return sqlite3_errcode(db) }
    }
    if parentIdSet && parentId > 0 {
      guard sqlite3_bind_int64(stmt, 6, parentId) == SQLITE_OK else { return sqlite3_errcode(db) }
    } else {
      guard sqlite3_bind_null(stmt, 6) == SQLITE_OK else { return sqlite3_errcode(db) }
    }
    if dueDateSet {
      guard sqlite3_bind_int64(stmt, 7, dueDate) == SQLITE_OK else { return sqlite3_errcode(db) }
    } else {
      guard sqlite3_bind_null(stmt, 7) == SQLITE_OK else { return sqlite3_errcode(db) }
    }
    guard sqlite3_bind_int(stmt, 8, Int32(position)) == SQLITE_OK else { return sqlite3_errcode(db) }
    guard sqlite3_bind_int64(stmt, 9, id) == SQLITE_OK else { return sqlite3_errcode(db) }
    let stepRc = sqlite3_step(stmt)
    guard stepRc == SQLITE_DONE else {
      return stepRc == SQLITE_ROW ? SQLITE_ERROR : stepRc
    }
    return sqlite3_changes(db) > 0 ? SQLITE_OK : SQLITE_ERROR
  }

  public func todoDeleteById(_ id: Int64) -> Int32 {
    guard let db else { return SQLITE_MISUSE }
    let q = "DELETE FROM todos WHERE id = ?;"
    var st: OpaquePointer?
    guard sqlite3_prepare_v2(db, q, -1, &st, nil) == SQLITE_OK, let stmt = st else {
      return sqlite3_errcode(db)
    }
    defer { sqlite3_finalize(stmt) }
    guard sqlite3_bind_int64(stmt, 1, id) == SQLITE_OK else { return sqlite3_errcode(db) }
    let stepRc = sqlite3_step(stmt)
    guard stepRc == SQLITE_DONE else {
      return stepRc == SQLITE_ROW ? SQLITE_ERROR : stepRc
    }
    return sqlite3_changes(db) > 0 ? SQLITE_OK : SQLITE_ERROR
  }
}
