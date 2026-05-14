#!/usr/bin/env python3
# Port of c_todo/tests/test_todo_store.c / nim_todo/tests/test_todo_store.nim
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_ROOT))

from todo_store import (  # noqa: E402
    SQLITE_OK,
    store_categories_foreach,
    store_category_delete_by_id,
    store_category_insert,
    store_close,
    store_open,
    store_todo_delete_by_id,
    store_todo_insert,
    store_todos_foreach,
    store_todo_update_full,
)


def _fail(msg: str) -> None:
    sys.stderr.write(f"test_todo_store: {msg}\n")


def main() -> int:
    s = store_open("")
    if s is None:
        _fail('todo_store_open("")')
        return 1

    out_cat: list[int] = []
    if store_category_insert(s, "Work", "#ff00aa", out_cat) != SQLITE_OK:
        _fail("category insert")
        return 1
    cat1 = out_cat[0]
    if cat1 <= 0:
        _fail("category id")
        return 1

    out_dup: list[int] = []
    if store_category_insert(s, "Work", "#000000", out_dup) == SQLITE_OK:
        _fail("duplicate category name")
        return 1

    cat_count = 0

    def _cat_cb(_r) -> None:
        nonlocal cat_count
        cat_count += 1

    if store_categories_foreach(s, _cat_cb) != SQLITE_OK:
        _fail("categories foreach")
        return 1
    if cat_count != 1:
        _fail("category count")
        return 1

    out_tid: list[int] = []
    if (
        store_todo_insert(
            s, "Task one", "", "pending", "high", True, cat1, False, 0, False, 0, out_tid
        )
        != SQLITE_OK
    ):
        _fail("todo insert")
        return 1
    tid = out_tid[0]
    if tid <= 0:
        _fail("todo id")
        return 1

    todo_count = 0

    def _todo_cb(_r) -> None:
        nonlocal todo_count
        todo_count += 1

    if store_todos_foreach(s, _todo_cb) != SQLITE_OK:
        _fail("todos foreach")
        return 1
    if todo_count != 1:
        _fail("todo foreach count")
        return 1

    if (
        store_todo_update_full(
            s, tid, "Task one", "desc", "in_progress", "low", False, 0, False, 0, False, 0, 0
        )
        != SQLITE_OK
    ):
        _fail("todo update")
        return 1

    if (
        store_todo_update_full(
            s, tid, "Task one", "desc", "archived", "low", False, 0, False, 0, False, 0, 0
        )
        != SQLITE_OK
    ):
        _fail("todo update archived")
        return 1

    if store_todo_delete_by_id(s, tid) != SQLITE_OK:
        _fail("todo delete")
        return 1
    todo_count = 0
    if store_todos_foreach(s, _todo_cb) != SQLITE_OK:
        _fail("todos foreach 2")
        return 1
    if todo_count != 0:
        _fail("todo count after delete")
        return 1

    if store_category_delete_by_id(s, cat1) != SQLITE_OK:
        _fail("category delete")
        return 1
    cat_count = 0
    if store_categories_foreach(s, _cat_cb) != SQLITE_OK:
        _fail("cat foreach 2")
        return 1
    if cat_count != 0:
        _fail("categories empty")
        return 1

    bad: list[int] = []
    if store_category_insert(s, "", "#fff", bad) == SQLITE_OK:
        _fail("empty category name")
        return 1
    if (
        store_todo_insert(s, "", "x", "pending", "medium", False, 0, False, 0, False, 0, out_tid)
        == SQLITE_OK
    ):
        _fail("empty title")
        return 1

    store_close(s)
    return 0


if __name__ == "__main__":
    sys.exit(main())
