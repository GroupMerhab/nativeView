#!/usr/bin/env python3
# Port of c_todo/tests/test_todo_bridge.c / nim_todo/tests/test_todo_bridge.nim
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import json
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_ROOT))

from todo_bridge import bridge_handle_wire  # noqa: E402
from todo_store import store_close, store_open  # noqa: E402


def _fail(msg: str) -> None:
    sys.stderr.write(f"test_todo_bridge: {msg}\n")


def _capture_factory():
    last = {"ev": "", "j": ""}

    def cap(ev: str, j: str) -> None:
        last["ev"] = ev
        last["j"] = j

    return last, cap


def _count_json_array_elems(arr_json: str) -> int:
    v = json.loads(arr_json)
    if not isinstance(v, list):
        return -1
    return len(v)


def main() -> int:
    s = store_open("")
    if s is None:
        _fail("store")
        return 1

    last, cap = _capture_factory()

    wire_list = '{"e":"todo","s":0,"d":{"action":"categories.list"}}'
    if bridge_handle_wire(s, wire_list, cap) != 0:
        _fail("categories.list rc")
        return 1
    if last["ev"] != "categories.list":
        _fail("event categories.list")
        return 1
    if last["j"] != "[]":
        _fail("empty categories json")
        return 1

    wire_mk = '{"e":"todo","s":0,"d":{"action":"categories.create","payload":{"name":"A","color":"#112233"}}}'
    if bridge_handle_wire(s, wire_mk, cap) != 0:
        _fail("categories.create rc")
        return 1
    if last["ev"] != "categories.list":
        _fail("event after create")
        return 1
    if _count_json_array_elems(last["j"]) != 1:
        _fail("one category")
        return 1

    wire_todo = '{"e":"todo","s":0,"d":{"action":"todos.create","payload":{"title":"T1","description":"","status":"pending","priority":"medium","categoryId":1,"parentId":0,"dueDate":0}}}'
    if bridge_handle_wire(s, wire_todo, cap) != 0:
        _fail("todos.create rc")
        return 1
    if last["ev"] != "todos.list":
        _fail("event todos.list")
        return 1
    if _count_json_array_elems(last["j"]) != 1:
        _fail("one todo")
        return 1

    arr = json.loads(last["j"])
    if not isinstance(arr, list) or len(arr) < 1:
        _fail("parse todos")
        return 1
    row = arr[0]
    tid = int(row["id"])
    delbuf = (
        '{"e":"todo","s":0,"d":{"action":"todos.delete","payload":{"id":'
        + str(tid)
        + "}}}"
    )
    if bridge_handle_wire(s, delbuf, cap) != 0:
        _fail("delete rc")
        return 1
    if _count_json_array_elems(last["j"]) != 0:
        _fail("zero todos after delete")
        return 1

    store_close(s)
    return 0


if __name__ == "__main__":
    sys.exit(main())
