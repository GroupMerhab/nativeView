//! SQLite persistence for todo_app (port of `nim_todo/todo_store.nim`).
//!
//! SPDX-License-Identifier: Apache-2.0

#![forbid(unsafe_code)]

use std::path::Path;

use rusqlite::ffi::{SQLITE_CONSTRAINT, SQLITE_ERROR, SQLITE_MISUSE, SQLITE_OK, SQLITE_RANGE};
use rusqlite::{params, Connection, Row};

/// Return code aligned with SQLite (`SQLITE_*`).
pub type SqlRc = i32;

const TITLE_MAX: usize = 255;
const DESC_MAX: usize = 4096;
const NAME_MAX: usize = 64;
const COLOR_MAX: usize = 32;

#[derive(Debug, Clone)]
pub struct TodoStoreCategoryRow {
    pub id: i64,
    pub name: String,
    pub color: String,
}

#[derive(Debug, Clone)]
pub struct TodoStoreTodoRow {
    pub id: i64,
    pub title: String,
    pub description: String,
    pub status: String,
    pub priority: String,
    pub category_id_is_null: bool,
    pub category_id: i64,
    pub parent_id_is_null: bool,
    pub parent_id: i64,
    pub due_date_is_null: bool,
    pub due_date: i64,
    pub position: i32,
    pub created_at: i64,
    pub updated_at: i64,
}

pub struct TodoStore {
    conn: Connection,
}

fn map_err(e: rusqlite::Error) -> SqlRc {
    match e {
        rusqlite::Error::SqliteFailure(ie, _) => ie.extended_code,
        _ => SQLITE_ERROR,
    }
}

fn exec_sql(conn: &Connection, sql: &str) -> SqlRc {
    match conn.execute_batch(sql) {
        Ok(()) => SQLITE_OK,
        Err(e) => {
            eprintln!("[todo_store] {e}");
            map_err(e)
        }
    }
}

fn read_user_version(conn: &Connection) -> i32 {
    conn.query_row("PRAGMA user_version;", [], |r| r.get::<_, i32>(0))
        .unwrap_or(-1)
}

const SCHEMA_DDL_V3: &str = r"
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
";

const MIGRATE_V2_TO_V3: &str = r"
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
";

fn ensure_schema(conn: &Connection) -> SqlRc {
    let mut ver = read_user_version(conn);
    if ver < 0 {
        return SQLITE_ERROR;
    }

    if exec_sql(conn, "PRAGMA foreign_keys = ON;") != SQLITE_OK {
        return SQLITE_ERROR;
    }
    let _ = exec_sql(conn, "PRAGMA journal_mode=WAL;");

    if ver == 1 {
        if exec_sql(conn, "DROP TABLE IF EXISTS todos;") != SQLITE_OK {
            return SQLITE_ERROR;
        }
    }

    if ver == 2 {
        return exec_sql(conn, MIGRATE_V2_TO_V3);
    }

    if exec_sql(conn, SCHEMA_DDL_V3) != SQLITE_OK {
        return SQLITE_ERROR;
    }

    ver = read_user_version(conn);
    if ver < 3 && exec_sql(conn, "PRAGMA user_version = 3;") != SQLITE_OK {
        return SQLITE_ERROR;
    }
    SQLITE_OK
}

/// Opens the store (empty `path` uses in-memory SQLite).
pub fn store_open(path: &str) -> Option<TodoStore> {
    let uri = if path.is_empty() { ":memory:" } else { path };
    let conn = Connection::open(Path::new(uri)).ok()?;
    if conn.busy_timeout(std::time::Duration::from_millis(5000)).is_err() {
        return None;
    }
    if ensure_schema(&conn) != SQLITE_OK {
        return None;
    }
    Some(TodoStore { conn })
}

pub fn store_close(s: TodoStore) {
    drop(s);
}

fn validate_name_color(name: &str, color: &str) -> SqlRc {
    if name.is_empty() || color.is_empty() {
        return SQLITE_MISUSE;
    }
    let nl = name.chars().count();
    let cl = color.chars().count();
    if !(1..=NAME_MAX).contains(&nl) {
        return SQLITE_RANGE;
    }
    if !(1..=COLOR_MAX).contains(&cl) {
        return SQLITE_RANGE;
    }
    SQLITE_OK
}

fn validate_todo_texts(title: &str, description: &str, status: &str, priority: &str) -> SqlRc {
    if status.is_empty() || priority.is_empty() {
        return SQLITE_MISUSE;
    }
    let tl = title.chars().count();
    let dl = description.chars().count();
    if !(1..=TITLE_MAX).contains(&tl) {
        return SQLITE_RANGE;
    }
    if dl > DESC_MAX {
        return SQLITE_RANGE;
    }
    if !matches!(
        status,
        "pending" | "in_progress" | "done" | "cancelled" | "archived"
    ) {
        return SQLITE_CONSTRAINT;
    }
    if !matches!(priority, "low" | "medium" | "high" | "urgent") {
        return SQLITE_CONSTRAINT;
    }
    SQLITE_OK
}

pub fn store_categories_foreach<F>(s: &TodoStore, mut f: F) -> SqlRc
where
    F: FnMut(TodoStoreCategoryRow),
{
    let q = "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;";
    let mut stmt = match s.conn.prepare(q) {
        Ok(st) => st,
        Err(e) => return map_err(e),
    };
    let mut rows = match stmt.query([]) {
        Ok(r) => r,
        Err(e) => return map_err(e),
    };
    loop {
        match rows.next() {
            Ok(Some(row)) => {
                let r = TodoStoreCategoryRow {
                    id: row.get(0).unwrap_or(0),
                    name: row.get(1).unwrap_or_default(),
                    color: row.get(2).unwrap_or_default(),
                };
                f(r);
            }
            Ok(None) => break,
            Err(e) => return map_err(e),
        }
    }
    SQLITE_OK
}

pub fn store_category_insert(s: &TodoStore, name: &str, color: &str, out_id: &mut i64) -> SqlRc {
    let v = validate_name_color(name, color);
    if v != SQLITE_OK {
        return v;
    }
    let r = s.conn.execute(
        "INSERT INTO categories(name, color) VALUES(?, ?);",
        params![name, color],
    );
    match r {
        Ok(_) => {
            *out_id = s.conn.last_insert_rowid();
            SQLITE_OK
        }
        Err(e) => map_err(e),
    }
}

pub fn store_category_delete_by_id(s: &TodoStore, id: i64) -> SqlRc {
    match s
        .conn
        .execute("DELETE FROM categories WHERE id = ?1;", params![id])
    {
        Ok(n) if n > 0 => SQLITE_OK,
        Ok(_) => SQLITE_ERROR,
        Err(e) => map_err(e),
    }
}

fn map_todo_row(row: &Row<'_>) -> rusqlite::Result<TodoStoreTodoRow> {
    let category_id: Option<i64> = row.get(5)?;
    let parent_id: Option<i64> = row.get(6)?;
    let due_date: Option<i64> = row.get(7)?;
    Ok(TodoStoreTodoRow {
        id: row.get(0)?,
        title: row.get(1)?,
        description: row.get(2)?,
        status: row.get(3)?,
        priority: row.get(4)?,
        category_id_is_null: category_id.is_none(),
        category_id: category_id.unwrap_or(0),
        parent_id_is_null: parent_id.is_none(),
        parent_id: parent_id.unwrap_or(0),
        due_date_is_null: due_date.is_none(),
        due_date: due_date.unwrap_or(0),
        position: row.get(8)?,
        created_at: row.get(9)?,
        updated_at: row.get(10)?,
    })
}

pub fn store_todos_foreach<F>(s: &TodoStore, mut f: F) -> SqlRc
where
    F: FnMut(TodoStoreTodoRow),
{
    let q = "SELECT id, title, description, status, priority, category_id, parent_id, due_date,\
 position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;";
    let mut stmt = match s.conn.prepare(q) {
        Ok(st) => st,
        Err(e) => return map_err(e),
    };
    let mut rows = match stmt.query([]) {
        Ok(r) => r,
        Err(e) => return map_err(e),
    };
    loop {
        match rows.next() {
            Ok(Some(row)) => match map_todo_row(&row) {
                Ok(r) => f(r),
                Err(e) => return map_err(e),
            },
            Ok(None) => break,
            Err(e) => return map_err(e),
        }
    }
    SQLITE_OK
}

pub fn store_todo_insert(
    s: &TodoStore,
    title: &str,
    description: &str,
    status: &str,
    priority: &str,
    category_id_set: bool,
    category_id: i64,
    parent_id_set: bool,
    parent_id: i64,
    due_date_set: bool,
    due_date: i64,
    out_id: &mut i64,
) -> SqlRc {
    let v = validate_todo_texts(title, description, status, priority);
    if v != SQLITE_OK {
        return v;
    }
    let r = s.conn.execute(
        "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)\
 VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));",
        params![
            title,
            description,
            status,
            priority,
            category_id_set.then_some(category_id).filter(|&id| id > 0),
            parent_id_set.then_some(parent_id).filter(|&id| id > 0),
            due_date_set.then_some(due_date),
        ],
    );
    match r {
        Ok(_) => {
            *out_id = s.conn.last_insert_rowid();
            SQLITE_OK
        }
        Err(e) => map_err(e),
    }
}

pub fn store_todo_update_full(
    s: &TodoStore,
    id: i64,
    title: &str,
    description: &str,
    status: &str,
    priority: &str,
    category_id_set: bool,
    category_id: i64,
    parent_id_set: bool,
    parent_id: i64,
    due_date_set: bool,
    due_date: i64,
    position: i32,
) -> SqlRc {
    let v = validate_todo_texts(title, description, status, priority);
    if v != SQLITE_OK {
        return v;
    }
    let n = s.conn.execute(
        "UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?\
, due_date=?, position=?, updated_at=unixepoch() WHERE id=?;",
        params![
            title,
            description,
            status,
            priority,
            category_id_set.then_some(category_id).filter(|&cid| cid > 0),
            parent_id_set.then_some(parent_id).filter(|&pid| pid > 0),
            due_date_set.then_some(due_date),
            position,
            id,
        ],
    );
    match n {
        Ok(n) if n > 0 => SQLITE_OK,
        Ok(_) => SQLITE_ERROR,
        Err(e) => map_err(e),
    }
}

pub fn store_todo_delete_by_id(s: &TodoStore, id: i64) -> SqlRc {
    match s.conn.execute("DELETE FROM todos WHERE id = ?1;", params![id]) {
        Ok(n) if n > 0 => SQLITE_OK,
        Ok(_) => SQLITE_ERROR,
        Err(e) => map_err(e),
    }
}
