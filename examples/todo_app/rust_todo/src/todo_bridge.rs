//! IPC dispatch + JSON (port of `nim_todo/todo_bridge.nim`).
//!
//! SPDX-License-Identifier: Apache-2.0

#![forbid(unsafe_code)]

use rusqlite::ffi::{SQLITE_MISUSE, SQLITE_OK};
use serde_json::{json, Map, Value};

use crate::todo_store::{
    self, TodoStore, TodoStoreCategoryRow, TodoStoreTodoRow,
};

fn emit_err<E: FnMut(String, String)>(emit: &mut E, code: i32, msg: &str) {
    let o = json!({ "code": code, "message": msg });
    emit("error".to_string(), o.to_string());
}

fn j_str(pl: &Map<String, Value>, k: &str) -> String {
    pl.get(k)
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string()
}

fn j_str_or(pl: &Map<String, Value>, k: &str, default: &str) -> String {
    let v = j_str(pl, k);
    if !v.is_empty() {
        v
    } else {
        default.to_string()
    }
}

fn j_i64(pl: &Map<String, Value>, k: &str) -> i64 {
    pl.get(k)
        .map(|v| {
            if let Some(i) = v.as_i64() {
                return i;
            }
            if let Some(u) = v.as_u64() {
                return u as i64;
            }
            if let Some(f) = v.as_f64() {
                return f as i64;
            }
            0
        })
        .unwrap_or(0)
}

fn category_row_json(r: &TodoStoreCategoryRow) -> Value {
    json!({
        "id": r.id,
        "name": r.name,
        "color": r.color,
    })
}

fn todo_row_json(r: &TodoStoreTodoRow) -> Value {
    json!({
        "id": r.id,
        "title": r.title,
        "description": r.description,
        "status": r.status,
        "priority": r.priority,
        "categoryId": if r.category_id_is_null { Value::Null } else { json!(r.category_id) },
        "parentId": if r.parent_id_is_null { Value::Null } else { json!(r.parent_id) },
        "dueDate": if r.due_date_is_null { Value::Null } else { json!(r.due_date) },
        "position": r.position,
        "createdAt": r.created_at,
        "updatedAt": r.updated_at,
    })
}

fn emit_categories_list<E: FnMut(String, String)>(s: &TodoStore, emit: &mut E) -> i32 {
    let mut rows: Vec<TodoStoreCategoryRow> = Vec::new();
    let rc = todo_store::store_categories_foreach(s, |r| rows.push(r));
    if rc != SQLITE_OK {
        emit_err(emit, rc, "categories.foreach failed");
        return -1;
    }
    let arr: Vec<Value> = rows.iter().map(category_row_json).collect();
    emit("categories.list".to_string(), serde_json::to_string(&arr).unwrap_or_else(|_| "[]".into()));
    0
}

fn emit_todos_list<E: FnMut(String, String)>(s: &TodoStore, emit: &mut E) -> i32 {
    let mut rows: Vec<TodoStoreTodoRow> = Vec::new();
    let rc = todo_store::store_todos_foreach(s, |r| rows.push(r));
    if rc != SQLITE_OK {
        emit_err(emit, rc, "todos.foreach failed");
        return -1;
    }
    let arr: Vec<Value> = rows.iter().map(todo_row_json).collect();
    emit("todos.list".to_string(), serde_json::to_string(&arr).unwrap_or_else(|_| "[]".into()));
    0
}

/// Handles one `__nv` wire JSON envelope. Returns `0` on success, `-1` on failure.
pub fn bridge_handle_wire<E>(s: &TodoStore, raw_ipc_json: &str, emit: &mut E) -> i32
where
    E: FnMut(String, String),
{
    if raw_ipc_json.is_empty() {
        return -1;
    }
    let root: Value = match serde_json::from_str(raw_ipc_json) {
        Ok(v) => v,
        Err(_) => {
            emit_err(emit, -1, "invalid wire json");
            return -1;
        }
    };
    let Some(d) = root.get("d").and_then(|v| v.as_object()) else {
        emit_err(emit, -1, "missing wire.d object");
        return -1;
    };
    let action = d
        .get("action")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    if action.is_empty() {
        emit_err(emit, -1, "missing action");
        return -1;
    }
    let pl = d.get("payload").and_then(|v| v.as_object());

    match action.as_str() {
        "categories.list" => emit_categories_list(s, emit),
        "todos.list" => emit_todos_list(s, emit),
        "categories.create" => {
            let Some(pl) = pl else {
                emit_err(emit, SQLITE_MISUSE, "missing payload");
                return -1;
            };
            let name = j_str(pl, "name");
            let color = j_str_or(pl, "color", "#6366f1");
            let mut new_id = 0i64;
            let rc = todo_store::store_category_insert(s, &name, &color, &mut new_id);
            if rc != SQLITE_OK {
                emit_err(emit, rc, "category insert failed");
                return -1;
            }
            emit_categories_list(s, emit)
        }
        "categories.delete" => {
            let Some(pl) = pl else {
                emit_err(emit, SQLITE_MISUSE, "missing payload");
                return -1;
            };
            let id = j_i64(pl, "id");
            let rc = todo_store::store_category_delete_by_id(s, id);
            if rc != SQLITE_OK {
                emit_err(emit, rc, "category delete failed");
                return -1;
            }
            emit_categories_list(s, emit)
        }
        "todos.create" => {
            let Some(pl) = pl else {
                emit_err(emit, SQLITE_MISUSE, "missing payload");
                return -1;
            };
            let title = j_str(pl, "title");
            let desc = j_str_or(pl, "description", "");
            let status = j_str_or(pl, "status", "pending");
            let pri = j_str_or(pl, "priority", "medium");
            let cat = j_i64(pl, "categoryId");
            let par = j_i64(pl, "parentId");
            let due = j_i64(pl, "dueDate");
            let cat_set = cat > 0;
            let par_set = par > 0;
            let due_set = due != 0;
            let mut new_id = 0i64;
            let rc = todo_store::store_todo_insert(
                s, &title, &desc, &status, &pri, cat_set, cat, par_set, par, due_set, due, &mut new_id,
            );
            if rc != SQLITE_OK {
                emit_err(emit, rc, "todo insert failed");
                return -1;
            }
            emit_todos_list(s, emit)
        }
        "todos.update" => {
            let Some(pl) = pl else {
                emit_err(emit, SQLITE_MISUSE, "missing payload");
                return -1;
            };
            let id = j_i64(pl, "id");
            let title = j_str(pl, "title");
            let desc = j_str_or(pl, "description", "");
            let status = j_str_or(pl, "status", "pending");
            let pri = j_str_or(pl, "priority", "medium");
            let cat = j_i64(pl, "categoryId");
            let par = j_i64(pl, "parentId");
            let due = j_i64(pl, "dueDate");
            let pos = j_i64(pl, "position") as i32;
            let cat_set = cat > 0;
            let par_set = par > 0;
            let due_set = due != 0;
            let rc = todo_store::store_todo_update_full(
                s, id, &title, &desc, &status, &pri, cat_set, cat, par_set, par, due_set, due, pos,
            );
            if rc != SQLITE_OK {
                emit_err(emit, rc, "todo update failed");
                return -1;
            }
            emit_todos_list(s, emit)
        }
        "todos.delete" => {
            let Some(pl) = pl else {
                emit_err(emit, SQLITE_MISUSE, "missing payload");
                return -1;
            };
            let id = j_i64(pl, "id");
            let rc = todo_store::store_todo_delete_by_id(s, id);
            if rc != SQLITE_OK {
                emit_err(emit, rc, "todo delete failed");
                return -1;
            }
            emit_todos_list(s, emit)
        }
        _ => {
            emit_err(emit, -1, "unknown action");
            -1
        }
    }
}
