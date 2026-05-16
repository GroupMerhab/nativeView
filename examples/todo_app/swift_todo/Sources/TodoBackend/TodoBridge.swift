// SPDX-License-Identifier: Apache-2.0
// IPC dispatch + JSON (port of c_todo/src/todo_bridge.c; Foundation JSON).

import Foundation
import SQLite3

public typealias TodoBridgeEmit = (_ event: String, _ json: String) -> Void

private func emitWireError(_ emit: TodoBridgeEmit, _ code: Int, _ message: String) {
  let o: [String: Any] = ["code": code, "message": message]
  guard let data = try? JSONSerialization.data(withJSONObject: o),
    let s = String(data: data, encoding: .utf8)
  else { return }
  emit("error", s)
}

private func jStr(_ pl: [String: Any]?, _ key: String) -> String {
  guard let pl, let v = pl[key] else { return "" }
  if v is NSNull { return "" }
  if let s = v as? String { return s }
  if let n = v as? NSNumber { return n.stringValue }
  return ""
}

private func jStrOr(_ pl: [String: Any]?, _ key: String, default def: String) -> String {
  let t = jStr(pl, key)
  return t.isEmpty ? def : t
}

private func jInt64(_ pl: [String: Any]?, _ key: String) -> Int64 {
  guard let pl, let v = pl[key], !(v is NSNull) else { return 0 }
  if let i = v as? Int64 { return i }
  if let i = v as? Int { return Int64(i) }
  if let n = v as? NSNumber { return n.int64Value }
  return 0
}

private func jsonString(data: Any) -> String? {
  guard JSONSerialization.isValidJSONObject(data),
    let d = try? JSONSerialization.data(withJSONObject: data)
  else { return nil }
  return String(data: d, encoding: .utf8)
}

private func categoryRowJson(_ r: TodoStoreCategoryRow) -> [String: Any] {
  ["id": r.id, "name": r.name, "color": r.color]
}

private func todoRowJson(_ r: TodoStoreTodoRow) -> [String: Any] {
  var o: [String: Any] = [
    "id": r.id,
    "title": r.title,
    "description": r.description,
    "status": r.status,
    "priority": r.priority,
    "position": r.position,
    "createdAt": r.createdAt,
    "updatedAt": r.updatedAt,
  ]
  if r.categoryIdIsNull { o["categoryId"] = NSNull() }
  else { o["categoryId"] = r.categoryId }
  if r.parentIdIsNull { o["parentId"] = NSNull() }
  else { o["parentId"] = r.parentId }
  if r.dueDateIsNull { o["dueDate"] = NSNull() }
  else { o["dueDate"] = r.dueDate }
  return o
}

private func emitCategoriesList(_ s: TodoStore, _ emit: TodoBridgeEmit) -> Int {
  var rows: [TodoStoreCategoryRow] = []
  let rc = s.categoriesForeach { rows.append($0) }
  guard rc == SQLITE_OK else {
    emitWireError(emit, Int(rc), "categories.foreach failed")
    return -1
  }
  let arr = rows.map { categoryRowJson($0) }
  guard let json = jsonString(data: arr) else { return -1 }
  emit("categories.list", json)
  return 0
}

private func emitTodosList(_ s: TodoStore, _ emit: TodoBridgeEmit) -> Int {
  var rows: [TodoStoreTodoRow] = []
  let rc = s.todosForeach { rows.append($0) }
  guard rc == SQLITE_OK else {
    emitWireError(emit, Int(rc), "todos.foreach failed")
    return -1
  }
  let arr = rows.map { todoRowJson($0) }
  guard let json = jsonString(data: arr) else { return -1 }
  emit("todos.list", json)
  return 0
}

/// Handle one `todo` wire JSON message; emits follow-up events via `emit`.
public func bridgeHandleWire(_ store: TodoStore, rawIpcJson: String, emit: TodoBridgeEmit) -> Int {
  guard !rawIpcJson.isEmpty else { return -1 }
  guard let rootData = rawIpcJson.data(using: .utf8),
    let rootObj = try? JSONSerialization.jsonObject(with: rootData) as? [String: Any]
  else {
    emitWireError(emit, -1, "invalid wire json")
    return -1
  }
  guard let d = rootObj["d"] as? [String: Any] else {
    emitWireError(emit, -1, "missing wire.d object")
    return -1
  }
  let action = jStr(d, "action")
  if action.isEmpty {
    emitWireError(emit, -1, "missing action")
    return -1
  }
  let pl = d["payload"] as? [String: Any]

  if action == "categories.list" {
    return emitCategoriesList(store, emit)
  }
  if action == "todos.list" {
    return emitTodosList(store, emit)
  }

  if action == "categories.create" {
    guard let pl else {
      emitWireError(emit, Int(SQLITE_MISUSE), "missing payload")
      return -1
    }
    let name = jStr(pl, "name")
    let color = jStrOr(pl, "color", default: "#6366f1")
    var newId: Int64 = 0
    let rc = store.categoryInsert(name: name, color: color, outId: &newId)
    guard rc == SQLITE_OK else {
      emitWireError(emit, Int(rc), "category insert failed")
      return -1
    }
    return emitCategoriesList(store, emit)
  }

  if action == "categories.delete" {
    guard let pl else {
      emitWireError(emit, Int(SQLITE_MISUSE), "missing payload")
      return -1
    }
    let id = jInt64(pl, "id")
    let rc = store.categoryDeleteById(id)
    guard rc == SQLITE_OK else {
      emitWireError(emit, Int(rc), "category delete failed")
      return -1
    }
    return emitCategoriesList(store, emit)
  }

  if action == "todos.create" {
    guard let pl else {
      emitWireError(emit, Int(SQLITE_MISUSE), "missing payload")
      return -1
    }
    let title = jStr(pl, "title")
    let desc = jStrOr(pl, "description", default: "")
    let status = jStrOr(pl, "status", default: "pending")
    let pri = jStrOr(pl, "priority", default: "medium")
    let cat = jInt64(pl, "categoryId")
    let par = jInt64(pl, "parentId")
    let due = jInt64(pl, "dueDate")
    let catSet = cat > 0
    let parSet = par > 0
    let dueSet = due != 0
    var newId: Int64 = 0
    let rc = store.todoInsert(
      title: title,
      description: desc,
      status: status,
      priority: pri,
      categoryIdSet: catSet,
      categoryId: cat,
      parentIdSet: parSet,
      parentId: par,
      dueDateSet: dueSet,
      dueDate: due,
      outId: &newId
    )
    guard rc == SQLITE_OK else {
      emitWireError(emit, Int(rc), "todo insert failed")
      return -1
    }
    return emitTodosList(store, emit)
  }

  if action == "todos.update" {
    guard let pl else {
      emitWireError(emit, Int(SQLITE_MISUSE), "missing payload")
      return -1
    }
    let id = jInt64(pl, "id")
    let title = jStr(pl, "title")
    let desc = jStrOr(pl, "description", default: "")
    let status = jStrOr(pl, "status", default: "pending")
    let pri = jStrOr(pl, "priority", default: "medium")
    let cat = jInt64(pl, "categoryId")
    let par = jInt64(pl, "parentId")
    let due = jInt64(pl, "dueDate")
    let pos = Int(jInt64(pl, "position"))
    let catSet = cat > 0
    let parSet = par > 0
    let dueSet = due != 0
    let rc = store.todoUpdateFull(
      id: id,
      title: title,
      description: desc,
      status: status,
      priority: pri,
      categoryIdSet: catSet,
      categoryId: cat,
      parentIdSet: parSet,
      parentId: par,
      dueDateSet: dueSet,
      dueDate: due,
      position: pos
    )
    guard rc == SQLITE_OK else {
      emitWireError(emit, Int(rc), "todo update failed")
      return -1
    }
    return emitTodosList(store, emit)
  }

  if action == "todos.delete" {
    guard let pl else {
      emitWireError(emit, Int(SQLITE_MISUSE), "missing payload")
      return -1
    }
    let id = jInt64(pl, "id")
    let rc = store.todoDeleteById(id)
    guard rc == SQLITE_OK else {
      emitWireError(emit, Int(rc), "todo delete failed")
      return -1
    }
    return emitTodosList(store, emit)
  }

  emitWireError(emit, -1, "unknown action")
  return -1
}
