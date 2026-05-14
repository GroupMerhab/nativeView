## Port of c_todo/tests/test_todo_store.c
import db_connector/sqlite3 as s3
import todo_store

var gFail = false

proc fail(msg: string) =
  stderr.writeLine "test_todo_store: ", msg
  gFail = true

proc main =
  gFail = false
  let s = storeOpen("")
  if s.isNil:
    fail("todo_store_open(\"\")")
    quit QuitFailure

  var cat1: int64 = 0
  if storeCategoryInsert(s, "Work", "#ff00aa", cat1) != s3.SQLITE_OK:
    fail("category insert")
  if cat1 <= 0:
    fail("category id")

  var dup: int64 = 0
  if storeCategoryInsert(s, "Work", "#000000", dup) == s3.SQLITE_OK:
    fail("duplicate category name")

  var catCount = 0
  let rc1 = storeCategoriesForeach(s) do (r: TodoStoreCategoryRow):
    inc catCount
  if rc1 != s3.SQLITE_OK:
    fail("categories foreach")
  if catCount != 1:
    fail("category count")

  var tid: int64 = 0
  if storeTodoInsert(s, "Task one", "", "pending", "high", true, cat1, false, 0, false, 0, tid) !=
      s3.SQLITE_OK:
    fail("todo insert")
  if tid <= 0:
    fail("todo id")

  var todoCount = 0
  let rc2 = storeTodosForeach(s) do (r: TodoStoreTodoRow):
    inc todoCount
  if rc2 != s3.SQLITE_OK:
    fail("todos foreach")
  if todoCount != 1:
    fail("todo foreach count")

  if storeTodoUpdateFull(s, tid, "Task one", "desc", "in_progress", "low", false, 0, false, 0, false, 0, 0) !=
      s3.SQLITE_OK:
    fail("todo update")

  if storeTodoUpdateFull(s, tid, "Task one", "desc", "archived", "low", false, 0, false, 0, false, 0, 0) !=
      s3.SQLITE_OK:
    fail("todo update archived")

  if storeTodoDeleteById(s, tid) != s3.SQLITE_OK:
    fail("todo delete")
  todoCount = 0
  let rc3 = storeTodosForeach(s) do (r: TodoStoreTodoRow):
    inc todoCount
  if rc3 != s3.SQLITE_OK:
    fail("todos foreach 2")
  if todoCount != 0:
    fail("todo count after delete")

  if storeCategoryDeleteById(s, cat1) != s3.SQLITE_OK:
    fail("category delete")
  catCount = 0
  let rc4 = storeCategoriesForeach(s) do (r: TodoStoreCategoryRow):
    inc catCount
  if rc4 != s3.SQLITE_OK:
    fail("cat foreach 2")
  if catCount != 0:
    fail("categories empty")

  var bad: int64 = 0
  if storeCategoryInsert(s, "", "#fff", bad) == s3.SQLITE_OK:
    fail("empty category name")
  if storeTodoInsert(s, "", "x", "pending", "medium", false, 0, false, 0, false, 0, tid) == s3.SQLITE_OK:
    fail("empty title")

  storeClose(s)
  quit(if gFail: QuitFailure else: QuitSuccess)

when isMainModule:
  main()
