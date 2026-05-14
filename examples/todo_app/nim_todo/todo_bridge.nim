## IPC dispatch + JSON (port of c_todo/src/todo_bridge.c; std/json instead of nv_json).
## SPDX-License-Identifier: Apache-2.0

import std/json
import db_connector/sqlite3 as s3
import ./todo_store

proc emitErr(emit: proc (ev: string; json: string) {.closure.}; code: int; msg: string) =
  let o = %* {"code": code, "message": msg}
  emit("error", $o)

proc jStr(pl: JsonNode; k: string): string =
  if pl.isNil or not pl.hasKey(k):
    return ""
  case pl[k].kind
  of JString:
    pl[k].getStr
  else:
    ""

proc jStrOr(pl: JsonNode; k, default: string): string =
  let v = jStr(pl, k)
  if v.len > 0:
    v
  else:
    default

proc jInt64(pl: JsonNode; k: string): int64 =
  if pl.isNil or not pl.hasKey(k):
    return 0
  case pl[k].kind
  of JInt:
    pl[k].getInt.int64
  else:
    0

proc categoryRowJson(r: TodoStoreCategoryRow): JsonNode =
  result = newJObject()
  result["id"] = %r.id
  result["name"] = %r.name
  result["color"] = %r.color

proc todoRowJson(r: TodoStoreTodoRow): JsonNode =
  result = newJObject()
  result["id"] = %r.id
  result["title"] = %r.title
  result["description"] = %r.description
  result["status"] = %r.status
  result["priority"] = %r.priority
  if r.categoryIdIsNull:
    result["categoryId"] = newJNull()
  else:
    result["categoryId"] = %r.categoryId
  if r.parentIdIsNull:
    result["parentId"] = newJNull()
  else:
    result["parentId"] = %r.parentId
  if r.dueDateIsNull:
    result["dueDate"] = newJNull()
  else:
    result["dueDate"] = %r.dueDate
  result["position"] = %r.position
  result["createdAt"] = %r.createdAt
  result["updatedAt"] = %r.updatedAt

proc emitCategoriesList(s: TodoStore; emit: proc (ev: string; json: string) {.closure.}): int =
  var rows: seq[TodoStoreCategoryRow] = @[]
  let rc = storeCategoriesForeach(s) do (r: TodoStoreCategoryRow):
    rows.add(r)
  if rc != s3.SQLITE_OK:
    emitErr(emit, rc, "categories.foreach failed")
    return -1
  let arr = newJArray()
  for r in rows:
    arr.add(categoryRowJson(r))
  emit("categories.list", $arr)
  0

proc emitTodosList(s: TodoStore; emit: proc (ev: string; json: string) {.closure.}): int =
  var rows: seq[TodoStoreTodoRow] = @[]
  let rc = storeTodosForeach(s) do (r: TodoStoreTodoRow):
    rows.add(r)
  if rc != s3.SQLITE_OK:
    emitErr(emit, rc, "todos.foreach failed")
    return -1
  let arr = newJArray()
  for r in rows:
    arr.add(todoRowJson(r))
  emit("todos.list", $arr)
  0

proc bridgeHandleWire*(s: TodoStore; rawIpcJson: string;
    emit: proc (ev: string; json: string) {.closure.}): int =
  if s.isNil or rawIpcJson.len == 0:
    return -1
  var root: JsonNode
  try:
    root = parseJson(rawIpcJson)
  except JsonParsingError:
    emitErr(emit, -1, "invalid wire json")
    return -1
  if root.kind != JObject:
    emitErr(emit, -1, "invalid wire json")
    return -1
  if not root.hasKey("d") or root["d"].kind != JObject:
    emitErr(emit, -1, "missing wire.d object")
    return -1
  let d = root["d"]
  let action = if d.hasKey("action") and d["action"].kind == JString: d["action"].getStr else: ""
  if action.len == 0:
    emitErr(emit, -1, "missing action")
    return -1
  let pl = if d.hasKey("payload") and d["payload"].kind == JObject: d["payload"] else: JsonNode(nil)

  if action == "categories.list":
    return emitCategoriesList(s, emit)

  if action == "todos.list":
    return emitTodosList(s, emit)

  if action == "categories.create":
    if pl.isNil:
      emitErr(emit, int(s3.SQLITE_MISUSE), "missing payload")
      return -1
    let name = jStr(pl, "name")
    let color = jStrOr(pl, "color", "#6366f1")
    var newId: int64 = 0
    let rc = storeCategoryInsert(s, name, color, newId)
    if rc != s3.SQLITE_OK:
      emitErr(emit, rc, "category insert failed")
      return -1
    discard newId
    return emitCategoriesList(s, emit)

  if action == "categories.delete":
    if pl.isNil:
      emitErr(emit, int(s3.SQLITE_MISUSE), "missing payload")
      return -1
    let id = jInt64(pl, "id")
    let rc = storeCategoryDeleteById(s, id)
    if rc != s3.SQLITE_OK:
      emitErr(emit, rc, "category delete failed")
      return -1
    return emitCategoriesList(s, emit)

  if action == "todos.create":
    if pl.isNil:
      emitErr(emit, int(s3.SQLITE_MISUSE), "missing payload")
      return -1
    let title = jStr(pl, "title")
    let desc = jStrOr(pl, "description", "")
    let status = jStrOr(pl, "status", "pending")
    let pri = jStrOr(pl, "priority", "medium")
    let cat = jInt64(pl, "categoryId")
    let par = jInt64(pl, "parentId")
    let due = jInt64(pl, "dueDate")
    let catSet = cat > 0
    let parSet = par > 0
    let dueSet = due != 0
    var newId: int64 = 0
    let rc = storeTodoInsert(s, title, desc, status, pri, catSet, cat, parSet, par, dueSet, due, newId)
    if rc != s3.SQLITE_OK:
      emitErr(emit, rc, "todo insert failed")
      return -1
    discard newId
    return emitTodosList(s, emit)

  if action == "todos.update":
    if pl.isNil:
      emitErr(emit, int(s3.SQLITE_MISUSE), "missing payload")
      return -1
    let id = jInt64(pl, "id")
    let title = jStr(pl, "title")
    let desc = jStrOr(pl, "description", "")
    let status = jStrOr(pl, "status", "pending")
    let pri = jStrOr(pl, "priority", "medium")
    let cat = jInt64(pl, "categoryId")
    let par = jInt64(pl, "parentId")
    let due = jInt64(pl, "dueDate")
    let pos = jInt64(pl, "position").int
    let catSet = cat > 0
    let parSet = par > 0
    let dueSet = due != 0
    let rc = storeTodoUpdateFull(s, id, title, desc, status, pri, catSet, cat, parSet, par, dueSet, due, pos)
    if rc != s3.SQLITE_OK:
      emitErr(emit, rc, "todo update failed")
      return -1
    return emitTodosList(s, emit)

  if action == "todos.delete":
    if pl.isNil:
      emitErr(emit, int(s3.SQLITE_MISUSE), "missing payload")
      return -1
    let id = jInt64(pl, "id")
    let rc = storeTodoDeleteById(s, id)
    if rc != s3.SQLITE_OK:
      emitErr(emit, rc, "todo delete failed")
      return -1
    return emitTodosList(s, emit)

  emitErr(emit, -1, "unknown action")
  -1
