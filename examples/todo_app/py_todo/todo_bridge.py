# IPC dispatch + JSON (port of c_todo/src/todo_bridge.c / nim_todo/todo_bridge.nim).
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import json
from collections.abc import Callable
from typing import Any

from todo_store import (
    SQLITE_MISUSE,
    SQLITE_OK,
    TodoStore,
    TodoStoreCategoryRow,
    TodoStoreTodoRow,
    store_categories_foreach,
    store_category_delete_by_id,
    store_category_insert,
    store_todo_delete_by_id,
    store_todo_insert,
    store_todos_foreach,
    store_todo_update_full,
)


def _emit_err(emit: Callable[[str, str], None], code: int, msg: str) -> None:
    emit("error", json.dumps({"code": code, "message": msg}))


def _j_str(pl: dict[str, Any] | None, k: str) -> str:
    if not pl or k not in pl:
        return ""
    v = pl[k]
    if isinstance(v, str):
        return v
    return ""


def _j_str_or(pl: dict[str, Any] | None, k: str, default: str) -> str:
    s = _j_str(pl, k)
    return s if s else default


def _j_int64(pl: dict[str, Any] | None, k: str) -> int:
    if not pl or k not in pl:
        return 0
    v = pl[k]
    if isinstance(v, bool):
        return int(v)
    if isinstance(v, int):
        return int(v)
    return 0


def _category_row_json(r: TodoStoreCategoryRow) -> dict[str, Any]:
    return {"id": r.id, "name": r.name, "color": r.color}


def _todo_row_json(r: TodoStoreTodoRow) -> dict[str, Any]:
    o: dict[str, Any] = {
        "id": r.id,
        "title": r.title,
        "description": r.description,
        "status": r.status,
        "priority": r.priority,
        "position": r.position,
        "createdAt": r.created_at,
        "updatedAt": r.updated_at,
    }
    if r.category_id_is_null:
        o["categoryId"] = None
    else:
        o["categoryId"] = r.category_id
    if r.parent_id_is_null:
        o["parentId"] = None
    else:
        o["parentId"] = r.parent_id
    if r.due_date_is_null:
        o["dueDate"] = None
    else:
        o["dueDate"] = r.due_date
    return o


def _emit_categories_list(s: TodoStore, emit: Callable[[str, str], None]) -> int:
    rows: list[TodoStoreCategoryRow] = []

    def _cb(r: TodoStoreCategoryRow) -> None:
        rows.append(r)

    rc = store_categories_foreach(s, _cb)
    if rc != SQLITE_OK:
        _emit_err(emit, rc, "categories.foreach failed")
        return -1
    emit("categories.list", json.dumps([_category_row_json(r) for r in rows]))
    return 0


def _emit_todos_list(s: TodoStore, emit: Callable[[str, str], None]) -> int:
    rows: list[TodoStoreTodoRow] = []

    def _cb(r: TodoStoreTodoRow) -> None:
        rows.append(r)

    rc = store_todos_foreach(s, _cb)
    if rc != SQLITE_OK:
        _emit_err(emit, rc, "todos.foreach failed")
        return -1
    emit("todos.list", json.dumps([_todo_row_json(r) for r in rows]))
    return 0


def bridge_handle_wire(
    s: TodoStore | None,
    raw_ipc_json: str,
    emit: Callable[[str, str], None],
) -> int:
    if s is None or not raw_ipc_json:
        return -1
    try:
        root = json.loads(raw_ipc_json)
    except json.JSONDecodeError:
        _emit_err(emit, -1, "invalid wire json")
        return -1
    if not isinstance(root, dict):
        _emit_err(emit, -1, "invalid wire json")
        return -1
    d = root.get("d")
    if not isinstance(d, dict):
        _emit_err(emit, -1, "missing wire.d object")
        return -1
    action = d.get("action", "")
    if not isinstance(action, str) or not action:
        _emit_err(emit, -1, "missing action")
        return -1
    pl = d.get("payload")
    pld: dict[str, Any] | None = pl if isinstance(pl, dict) else None

    if action == "categories.list":
        return _emit_categories_list(s, emit)

    if action == "todos.list":
        return _emit_todos_list(s, emit)

    if action == "categories.create":
        if pld is None:
            _emit_err(emit, SQLITE_MISUSE, "missing payload")
            return -1
        name = _j_str(pld, "name")
        color = _j_str_or(pld, "color", "#6366f1")
        new_id: list[int] = []
        rc = store_category_insert(s, name, color, new_id)
        if rc != SQLITE_OK:
            _emit_err(emit, rc, "category insert failed")
            return -1
        return _emit_categories_list(s, emit)

    if action == "categories.delete":
        if pld is None:
            _emit_err(emit, SQLITE_MISUSE, "missing payload")
            return -1
        row_id = _j_int64(pld, "id")
        rc = store_category_delete_by_id(s, row_id)
        if rc != SQLITE_OK:
            _emit_err(emit, rc, "category delete failed")
            return -1
        return _emit_categories_list(s, emit)

    if action == "todos.create":
        if pld is None:
            _emit_err(emit, SQLITE_MISUSE, "missing payload")
            return -1
        title = _j_str(pld, "title")
        desc = _j_str_or(pld, "description", "")
        status = _j_str_or(pld, "status", "pending")
        pri = _j_str_or(pld, "priority", "medium")
        cat = _j_int64(pld, "categoryId")
        par = _j_int64(pld, "parentId")
        due = _j_int64(pld, "dueDate")
        cat_set = cat > 0
        par_set = par > 0
        due_set = due != 0
        new_id: list[int] = []
        rc = store_todo_insert(
            s, title, desc, status, pri, cat_set, cat, par_set, par, due_set, due, new_id
        )
        if rc != SQLITE_OK:
            _emit_err(emit, rc, "todo insert failed")
            return -1
        return _emit_todos_list(s, emit)

    if action == "todos.update":
        if pld is None:
            _emit_err(emit, SQLITE_MISUSE, "missing payload")
            return -1
        row_id = _j_int64(pld, "id")
        title = _j_str(pld, "title")
        desc = _j_str_or(pld, "description", "")
        status = _j_str_or(pld, "status", "pending")
        pri = _j_str_or(pld, "priority", "medium")
        cat = _j_int64(pld, "categoryId")
        par = _j_int64(pld, "parentId")
        due = _j_int64(pld, "dueDate")
        pos = int(_j_int64(pld, "position"))
        cat_set = cat > 0
        par_set = par > 0
        due_set = due != 0
        rc = store_todo_update_full(
            s, row_id, title, desc, status, pri, cat_set, cat, par_set, par, due_set, due, pos
        )
        if rc != SQLITE_OK:
            _emit_err(emit, rc, "todo update failed")
            return -1
        return _emit_todos_list(s, emit)

    if action == "todos.delete":
        if pld is None:
            _emit_err(emit, SQLITE_MISUSE, "missing payload")
            return -1
        row_id = _j_int64(pld, "id")
        rc = store_todo_delete_by_id(s, row_id)
        if rc != SQLITE_OK:
            _emit_err(emit, rc, "todo delete failed")
            return -1
        return _emit_todos_list(s, emit)

    _emit_err(emit, -1, "unknown action")
    return -1
