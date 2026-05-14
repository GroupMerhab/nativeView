## Port of c_todo/tests/test_todo_bridge.c
import std/json
import todo_store
import todo_bridge

var gFail = false
var gLastEvent = ""
var gLastJson = ""

proc fail(msg: string) =
  stderr.writeLine "test_todo_bridge: ", msg
  gFail = true

proc capture(ev, j: string) =
  gLastEvent = ev
  gLastJson = j

proc countJsonArrayElems(arrJson: string): int =
  let v = parseJson(arrJson)
  if v.kind != JArray:
    return -1
  v.len

proc main =
  gFail = false
  let s = storeOpen("")
  if s.isNil:
    fail("store")
    quit QuitFailure

  let wireList = """{"e":"todo","s":0,"d":{"action":"categories.list"}}"""
  if bridgeHandleWire(s, wireList, capture) != 0:
    fail("categories.list rc")
  if gLastEvent != "categories.list":
    fail("event categories.list")
  if gLastJson != "[]":
    fail("empty categories json")

  let wireMk = """{"e":"todo","s":0,"d":{"action":"categories.create","payload":{"name":"A","color":"#112233"}}}"""
  if bridgeHandleWire(s, wireMk, capture) != 0:
    fail("categories.create rc")
  if gLastEvent != "categories.list":
    fail("event after create")
  if countJsonArrayElems(gLastJson) != 1:
    fail("one category")

  let wireTodo = """{"e":"todo","s":0,"d":{"action":"todos.create","payload":{"title":"T1","description":"","status":"pending","priority":"medium","categoryId":1,"parentId":0,"dueDate":0}}}"""
  if bridgeHandleWire(s, wireTodo, capture) != 0:
    fail("todos.create rc")
  if gLastEvent != "todos.list":
    fail("event todos.list")
  if countJsonArrayElems(gLastJson) != 1:
    fail("one todo")

  let arr = parseJson(gLastJson)
  if arr.kind != JArray or arr.len < 1:
    fail("parse todos")
  let row = arr[0]
  let tid = row["id"].getBiggestInt
  let delbuf = "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.delete\",\"payload\":{\"id\":" & $tid & "}}}"
  if bridgeHandleWire(s, delbuf, capture) != 0:
    fail("delete rc")
  if countJsonArrayElems(gLastJson) != 0:
    fail("zero todos after delete")

  storeClose(s)
  quit(if gFail: QuitFailure else: QuitSuccess)

when isMainModule:
  main()
