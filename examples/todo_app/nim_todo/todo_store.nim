## SQLite persistence for todo_app (port of c_todo/src/todo_store.c).
## SPDX-License-Identifier: Apache-2.0

import db_connector/db_sqlite as dbs
import db_connector/sqlite3 as s3

const
  TitleMax = 255
  DescMax = 4096
  NameMax = 64
  ColorMax = 32

type
  TodoStoreCategoryRow* = object
    id*: int64
    name*: string
    color*: string

  TodoStoreTodoRow* = object
    id*: int64
    title*: string
    description*: string
    status*: string
    priority*: string
    categoryIdIsNull*: bool
    categoryId*: int64
    parentIdIsNull*: bool
    parentId*: int64
    dueDateIsNull*: bool
    dueDate*: int64
    position*: int
    createdAt*: int64
    updatedAt*: int64

  TodoStore* = ref object
    db: DbConn

proc storeClose*(s: TodoStore) =
  if s.isNil:
    return
  dbs.close(s.db)

proc execSql(db: DbConn; sql: string): int =
  var err: cstring = nil
  let rc = s3.exec(db, sql.cstring, nil, nil, err)
  if err != nil:
    stderr.writeLine "[todo_store] ", $err
    s3.free(err)
  rc.int

proc readUserVersion(db: DbConn): int =
  var st: s3.Pstmt
  if s3.prepare_v2(db, cstring"PRAGMA user_version;", (-1).cint, st, nil) != s3.SQLITE_OK:
    return -1
  defer: discard s3.finalize(st)
  let stepRc = s3.step(st)
  if stepRc == s3.SQLITE_ROW:
    return s3.column_int(st, 0).int
  -1

const SchemaDdlV3 = """
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

const MigrateV2ToV3 = """
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

proc ensureSchema(db: DbConn): int =
  var ver = readUserVersion(db)
  if ver < 0:
    return s3.SQLITE_ERROR

  if execSql(db, "PRAGMA foreign_keys = ON;") != s3.SQLITE_OK:
    return s3.SQLITE_ERROR
  discard execSql(db, "PRAGMA journal_mode=WAL;")

  if ver == 1:
    if execSql(db, "DROP TABLE IF EXISTS todos;") != s3.SQLITE_OK:
      return s3.SQLITE_ERROR

  if ver == 2:
    if execSql(db, MigrateV2ToV3) != s3.SQLITE_OK:
      return s3.SQLITE_ERROR
    return s3.SQLITE_OK

  if execSql(db, SchemaDdlV3) != s3.SQLITE_OK:
    return s3.SQLITE_ERROR

  ver = readUserVersion(db)
  if ver < 3 and execSql(db, "PRAGMA user_version = 3;") != s3.SQLITE_OK:
    return s3.SQLITE_ERROR
  s3.SQLITE_OK

proc storeOpen*(path: string): TodoStore =
  let uri = if path.len == 0: ":memory:" else: path
  var db: DbConn
  try:
    db = dbs.open(uri, "", "", "")
  except DbError:
    stderr.writeLine "[todo_store] open failed"
    return nil
  discard s3.busy_timeout(db, 5000)
  if ensureSchema(db) != s3.SQLITE_OK:
    dbs.close(db)
    return nil
  TodoStore(db: db)

proc validateNameColor(name, color: string): int =
  if name.len == 0 or color.len == 0:
    return s3.SQLITE_MISUSE
  let nl = name.len
  let cl = color.len
  if nl < 1 or nl > NameMax:
    return s3.SQLITE_RANGE
  if cl < 1 or cl > ColorMax:
    return s3.SQLITE_RANGE
  s3.SQLITE_OK

proc validateTodoTexts(title, description, status, priority: string): int =
  if status.len == 0 or priority.len == 0:
    return s3.SQLITE_MISUSE
  let tl = title.len
  let dl = description.len
  if tl < 1 or tl > TitleMax:
    return s3.SQLITE_RANGE
  if dl > DescMax:
    return s3.SQLITE_RANGE
  if status != "pending" and status != "in_progress" and status != "done" and
      status != "cancelled" and status != "archived":
    return s3.SQLITE_CONSTRAINT
  if priority != "low" and priority != "medium" and priority != "high" and priority != "urgent":
    return s3.SQLITE_CONSTRAINT
  s3.SQLITE_OK

proc storeCategoriesForeach*(s: TodoStore; fn: proc (r: TodoStoreCategoryRow) {.closure.}): int =
  if s.isNil:
    return s3.SQLITE_MISUSE
  var st: s3.Pstmt
  const q = "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;"
  if s3.prepare_v2(s.db, cstring(q), (-1).cint, st, nil) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  defer: discard s3.finalize(st)
  var rc = s3.step(st)
  while rc == s3.SQLITE_ROW:
    var r: TodoStoreCategoryRow
    r.id = s3.column_int64(st, 0)
    r.name = $s3.column_text(st, 1)
    r.color = $s3.column_text(st, 2)
    fn(r)
    rc = s3.step(st)
  if rc == s3.SQLITE_DONE:
    s3.SQLITE_OK
  else:
    rc.int

proc storeCategoryInsert*(s: TodoStore; name, color: string; outId: var int64): int =
  if s.isNil:
    return s3.SQLITE_MISUSE
  let v = validateNameColor(name, color)
  if v != s3.SQLITE_OK:
    return v
  var st: s3.Pstmt
  const q = "INSERT INTO categories(name, color) VALUES(?, ?);"
  if s3.prepare_v2(s.db, cstring(q), (-1).cint, st, nil) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  defer: discard s3.finalize(st)
  if s3.bind_text(st, 1, name.cstring, name.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_text(st, 2, color.cstring, color.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  let rc = s3.step(st)
  if rc != s3.SQLITE_DONE:
    return if rc == s3.SQLITE_ROW: s3.SQLITE_ERROR else: rc.int
  outId = s3.last_insert_rowid(s.db)
  s3.SQLITE_OK

proc storeCategoryDeleteById*(s: TodoStore; id: int64): int =
  if s.isNil:
    return s3.SQLITE_MISUSE
  var st: s3.Pstmt
  const q = "DELETE FROM categories WHERE id = ?;"
  if s3.prepare_v2(s.db, cstring(q), (-1).cint, st, nil) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  defer: discard s3.finalize(st)
  if s3.bind_int64(st, 1, id) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  let rc = s3.step(st)
  if rc != s3.SQLITE_DONE:
    return if rc == s3.SQLITE_ROW: s3.SQLITE_ERROR else: rc.int
  if s3.changes(s.db) > 0:
    s3.SQLITE_OK
  else:
    s3.SQLITE_ERROR

proc storeTodosForeach*(s: TodoStore; fn: proc (r: TodoStoreTodoRow) {.closure.}): int =
  if s.isNil:
    return s3.SQLITE_MISUSE
  var st: s3.Pstmt
  const q = "SELECT id, title, description, status, priority, category_id, parent_id, due_date," &
    " position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;"
  if s3.prepare_v2(s.db, cstring(q), (-1).cint, st, nil) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  defer: discard s3.finalize(st)
  var rc = s3.step(st)
  while rc == s3.SQLITE_ROW:
    var r: TodoStoreTodoRow
    r.id = s3.column_int64(st, 0)
    r.title = $s3.column_text(st, 1)
    r.description = $s3.column_text(st, 2)
    r.status = $s3.column_text(st, 3)
    r.priority = $s3.column_text(st, 4)
    if s3.column_type(st, 5) == s3.SQLITE_NULL:
      r.categoryIdIsNull = true
      r.categoryId = 0
    else:
      r.categoryIdIsNull = false
      r.categoryId = s3.column_int64(st, 5)
    if s3.column_type(st, 6) == s3.SQLITE_NULL:
      r.parentIdIsNull = true
      r.parentId = 0
    else:
      r.parentIdIsNull = false
      r.parentId = s3.column_int64(st, 6)
    if s3.column_type(st, 7) == s3.SQLITE_NULL:
      r.dueDateIsNull = true
      r.dueDate = 0
    else:
      r.dueDateIsNull = false
      r.dueDate = s3.column_int64(st, 7)
    r.position = s3.column_int(st, 8).int
    r.createdAt = s3.column_int64(st, 9)
    r.updatedAt = s3.column_int64(st, 10)
    fn(r)
    rc = s3.step(st)
  if rc == s3.SQLITE_DONE:
    s3.SQLITE_OK
  else:
    rc.int

proc storeTodoInsert*(s: TodoStore; title, description, status, priority: string;
    categoryIdSet: bool; categoryId: int64;
    parentIdSet: bool; parentId: int64;
    dueDateSet: bool; dueDate: int64;
    outId: var int64): int =
  if s.isNil:
    return s3.SQLITE_MISUSE
  let v = validateTodoTexts(title, description, status, priority)
  if v != s3.SQLITE_OK:
    return v
  var st: s3.Pstmt
  const q = "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)" &
    " VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));"
  if s3.prepare_v2(s.db, cstring(q), (-1).cint, st, nil) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  defer: discard s3.finalize(st)
  if s3.bind_text(st, 1, title.cstring, title.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_text(st, 2, description.cstring, description.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_text(st, 3, status.cstring, status.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_text(st, 4, priority.cstring, priority.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if categoryIdSet and categoryId > 0:
    if s3.bind_int64(st, 5, categoryId) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  else:
    if s3.bind_null(st, 5) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  if parentIdSet and parentId > 0:
    if s3.bind_int64(st, 6, parentId) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  else:
    if s3.bind_null(st, 6) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  if dueDateSet:
    if s3.bind_int64(st, 7, dueDate) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  else:
    if s3.bind_null(st, 7) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  let rc = s3.step(st)
  if rc != s3.SQLITE_DONE:
    return if rc == s3.SQLITE_ROW: s3.SQLITE_ERROR else: rc.int
  outId = s3.last_insert_rowid(s.db)
  s3.SQLITE_OK

proc storeTodoUpdateFull*(s: TodoStore; id: int64; title, description, status, priority: string;
    categoryIdSet: bool; categoryId: int64;
    parentIdSet: bool; parentId: int64;
    dueDateSet: bool; dueDate: int64;
    position: int): int =
  if s.isNil:
    return s3.SQLITE_MISUSE
  let v = validateTodoTexts(title, description, status, priority)
  if v != s3.SQLITE_OK:
    return v
  var st: s3.Pstmt
  const q = "UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?" &
    ", due_date=?, position=?, updated_at=unixepoch() WHERE id=?;"
  if s3.prepare_v2(s.db, cstring(q), (-1).cint, st, nil) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  defer: discard s3.finalize(st)
  if s3.bind_text(st, 1, title.cstring, title.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_text(st, 2, description.cstring, description.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_text(st, 3, status.cstring, status.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_text(st, 4, priority.cstring, priority.len.cint, s3.SQLITE_TRANSIENT) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if categoryIdSet and categoryId > 0:
    if s3.bind_int64(st, 5, categoryId) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  else:
    if s3.bind_null(st, 5) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  if parentIdSet and parentId > 0:
    if s3.bind_int64(st, 6, parentId) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  else:
    if s3.bind_null(st, 6) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  if dueDateSet:
    if s3.bind_int64(st, 7, dueDate) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  else:
    if s3.bind_null(st, 7) != s3.SQLITE_OK:
      return s3.errcode(s.db).int
  if s3.bind_int(st, 8, position.int32) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  if s3.bind_int64(st, 9, id) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  let rc = s3.step(st)
  if rc != s3.SQLITE_DONE:
    return if rc == s3.SQLITE_ROW: s3.SQLITE_ERROR else: rc.int
  if s3.changes(s.db) > 0:
    s3.SQLITE_OK
  else:
    s3.SQLITE_ERROR

proc storeTodoDeleteById*(s: TodoStore; id: int64): int =
  if s.isNil:
    return s3.SQLITE_MISUSE
  var st: s3.Pstmt
  const q = "DELETE FROM todos WHERE id = ?;"
  if s3.prepare_v2(s.db, cstring(q), (-1).cint, st, nil) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  defer: discard s3.finalize(st)
  if s3.bind_int64(st, 1, id) != s3.SQLITE_OK:
    return s3.errcode(s.db).int
  let rc = s3.step(st)
  if rc != s3.SQLITE_DONE:
    return if rc == s3.SQLITE_ROW: s3.SQLITE_ERROR else: rc.int
  if s3.changes(s.db) > 0:
    s3.SQLITE_OK
  else:
    s3.SQLITE_ERROR
