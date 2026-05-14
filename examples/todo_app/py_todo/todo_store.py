# SQLite persistence for todo_app (port of c_todo/src/todo_store.c / nim_todo/todo_store.nim).
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import sqlite3
import sys
from dataclasses import dataclass
from typing import Callable

# Match sqlite3 C API return codes used by the C/Nim ports.
SQLITE_OK = 0
SQLITE_ERROR = 1
SQLITE_MISUSE = 21
SQLITE_RANGE = 25
SQLITE_CONSTRAINT = 19

TITLE_MAX = 255
DESC_MAX = 4096
NAME_MAX = 64
COLOR_MAX = 32


@dataclass
class TodoStoreCategoryRow:
    id: int
    name: str
    color: str


@dataclass
class TodoStoreTodoRow:
    id: int
    title: str
    description: str
    status: str
    priority: str
    category_id_is_null: bool
    category_id: int
    parent_id_is_null: bool
    parent_id: int
    due_date_is_null: bool
    due_date: int
    position: int
    created_at: int
    updated_at: int


class TodoStore:
    __slots__ = ("_conn",)

    def __init__(self, conn: sqlite3.Connection) -> None:
        self._conn = conn

    @property
    def conn(self) -> sqlite3.Connection:
        return self._conn


def store_close(s: TodoStore | None) -> None:
    if s is None:
        return
    s._conn.close()


def _exec_sql(conn: sqlite3.Connection, sql: str) -> int:
    try:
        conn.execute(sql)
    except sqlite3.Error as e:
        sys.stderr.write(f"[todo_store] {e}\n")
        return SQLITE_ERROR
    return SQLITE_OK


def _read_user_version(conn: sqlite3.Connection) -> int:
    try:
        cur = conn.execute("PRAGMA user_version;")
        row = cur.fetchone()
        if row is not None:
            return int(row[0])
    except sqlite3.Error:
        pass
    return -1


SCHEMA_DDL_V3 = """
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

MIGRATE_V2_TO_V3 = """
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


def _ensure_schema(conn: sqlite3.Connection) -> int:
    ver = _read_user_version(conn)
    if ver < 0:
        return SQLITE_ERROR

    if _exec_sql(conn, "PRAGMA foreign_keys = ON;") != SQLITE_OK:
        return SQLITE_ERROR
    _exec_sql(conn, "PRAGMA journal_mode=WAL;")

    if ver == 1:
        if _exec_sql(conn, "DROP TABLE IF EXISTS todos;") != SQLITE_OK:
            return SQLITE_ERROR

    if ver == 2:
        try:
            conn.executescript(MIGRATE_V2_TO_V3)
        except sqlite3.Error as e:
            sys.stderr.write(f"[todo_store] {e}\n")
            return SQLITE_ERROR
        return SQLITE_OK

    try:
        conn.executescript(SCHEMA_DDL_V3)
    except sqlite3.Error as e:
        sys.stderr.write(f"[todo_store] {e}\n")
        return SQLITE_ERROR

    ver = _read_user_version(conn)
    if ver < 3 and _exec_sql(conn, "PRAGMA user_version = 3;") != SQLITE_OK:
        return SQLITE_ERROR
    return SQLITE_OK


def store_open(path: str) -> TodoStore | None:
    uri = ":memory:" if not path else path
    try:
        conn = sqlite3.connect(uri, isolation_level=None)
    except sqlite3.Error:
        sys.stderr.write("[todo_store] open failed\n")
        return None
    conn.execute("PRAGMA busy_timeout = 5000;")
    if _ensure_schema(conn) != SQLITE_OK:
        conn.close()
        return None
    return TodoStore(conn)


def _validate_name_color(name: str, color: str) -> int:
    if not name or not color:
        return SQLITE_MISUSE
    nl, cl = len(name), len(color)
    if nl < 1 or nl > NAME_MAX or cl < 1 or cl > COLOR_MAX:
        return SQLITE_RANGE
    return SQLITE_OK


def _validate_todo_texts(title: str, description: str, status: str, priority: str) -> int:
    if not status or not priority:
        return SQLITE_MISUSE
    tl, dl = len(title), len(description)
    if tl < 1 or tl > TITLE_MAX or dl > DESC_MAX:
        return SQLITE_RANGE
    if status not in (
        "pending",
        "in_progress",
        "done",
        "cancelled",
        "archived",
    ):
        return SQLITE_CONSTRAINT
    if priority not in ("low", "medium", "high", "urgent"):
        return SQLITE_CONSTRAINT
    return SQLITE_OK


def store_categories_foreach(
    s: TodoStore | None, fn: Callable[[TodoStoreCategoryRow], None]
) -> int:
    if s is None:
        return SQLITE_MISUSE
    conn = s.conn
    q = "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;"
    try:
        cur = conn.execute(q)
        for row in cur:
            fn(
                TodoStoreCategoryRow(
                    id=int(row[0]),
                    name=str(row[1] or ""),
                    color=str(row[2] or ""),
                )
            )
    except sqlite3.Error:
        return SQLITE_ERROR
    return SQLITE_OK


def store_category_insert(s: TodoStore | None, name: str, color: str, out_id: list[int]) -> int:
    """out_id must be a one-element list; on success out_id[0] is the new row id."""
    if s is None:
        return SQLITE_MISUSE
    v = _validate_name_color(name, color)
    if v != SQLITE_OK:
        return v
    conn = s.conn
    q = "INSERT INTO categories(name, color) VALUES(?, ?);"
    try:
        cur = conn.execute(q, (name, color))
        out_id.clear()
        out_id.append(int(cur.lastrowid))
    except sqlite3.IntegrityError:
        return SQLITE_CONSTRAINT
    except sqlite3.Error:
        return SQLITE_ERROR
    return SQLITE_OK


def store_category_delete_by_id(s: TodoStore | None, row_id: int) -> int:
    if s is None:
        return SQLITE_MISUSE
    conn = s.conn
    q = "DELETE FROM categories WHERE id = ?;"
    try:
        cur = conn.execute(q, (row_id,))
        if cur.rowcount and cur.rowcount > 0:
            return SQLITE_OK
    except sqlite3.Error:
        pass
    return SQLITE_ERROR


def store_todos_foreach(
    s: TodoStore | None, fn: Callable[[TodoStoreTodoRow], None]
) -> int:
    if s is None:
        return SQLITE_MISUSE
    conn = s.conn
    q = (
        "SELECT id, title, description, status, priority, category_id, parent_id, due_date,"
        " position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;"
    )
    try:
        cur = conn.execute(q)
        for row in cur:
            cat_raw, par_raw, due_raw = row[5], row[6], row[7]
            fn(
                TodoStoreTodoRow(
                    id=int(row[0]),
                    title=str(row[1] or ""),
                    description=str(row[2] or ""),
                    status=str(row[3] or ""),
                    priority=str(row[4] or ""),
                    category_id_is_null=cat_raw is None,
                    category_id=int(cat_raw or 0),
                    parent_id_is_null=par_raw is None,
                    parent_id=int(par_raw or 0),
                    due_date_is_null=due_raw is None,
                    due_date=int(due_raw or 0),
                    position=int(row[8] or 0),
                    created_at=int(row[9] or 0),
                    updated_at=int(row[10] or 0),
                )
            )
    except sqlite3.Error:
        return SQLITE_ERROR
    return SQLITE_OK


def store_todo_insert(
    s: TodoStore | None,
    title: str,
    description: str,
    status: str,
    priority: str,
    category_id_set: bool,
    category_id: int,
    parent_id_set: bool,
    parent_id: int,
    due_date_set: bool,
    due_date: int,
    out_id: list[int],
) -> int:
    if s is None:
        return SQLITE_MISUSE
    v = _validate_todo_texts(title, description, status, priority)
    if v != SQLITE_OK:
        return v
    conn = s.conn
    q = (
        "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)"
        " VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));"
    )
    cat = category_id if category_id_set and category_id > 0 else None
    par = parent_id if parent_id_set and parent_id > 0 else None
    due = due_date if due_date_set else None
    try:
        cur = conn.execute(q, (title, description, status, priority, cat, par, due))
        out_id.clear()
        out_id.append(int(cur.lastrowid))
    except sqlite3.Error:
        return SQLITE_ERROR
    return SQLITE_OK


def store_todo_update_full(
    s: TodoStore | None,
    row_id: int,
    title: str,
    description: str,
    status: str,
    priority: str,
    category_id_set: bool,
    category_id: int,
    parent_id_set: bool,
    parent_id: int,
    due_date_set: bool,
    due_date: int,
    position: int,
) -> int:
    if s is None:
        return SQLITE_MISUSE
    v = _validate_todo_texts(title, description, status, priority)
    if v != SQLITE_OK:
        return v
    conn = s.conn
    q = (
        "UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?"
        ", due_date=?, position=?, updated_at=unixepoch() WHERE id=?;"
    )
    cat = category_id if category_id_set and category_id > 0 else None
    par = parent_id if parent_id_set and parent_id > 0 else None
    due = due_date if due_date_set else None
    try:
        cur = conn.execute(
            q,
            (title, description, status, priority, cat, par, due, position, row_id),
        )
        if cur.rowcount and cur.rowcount > 0:
            return SQLITE_OK
    except sqlite3.Error:
        pass
    return SQLITE_ERROR


def store_todo_delete_by_id(s: TodoStore | None, row_id: int) -> int:
    if s is None:
        return SQLITE_MISUSE
    conn = s.conn
    q = "DELETE FROM todos WHERE id = ?;"
    try:
        cur = conn.execute(q, (row_id,))
        if cur.rowcount and cur.rowcount > 0:
            return SQLITE_OK
    except sqlite3.Error:
        pass
    return SQLITE_ERROR
