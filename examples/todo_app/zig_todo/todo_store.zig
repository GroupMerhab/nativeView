//! SQLite persistence for todo_app (port of c_todo/src/todo_store.c).
//! SPDX-License-Identifier: Apache-2.0

const std = @import("std");

/// Opaque handles for `extern "c"` sqlite3 API (no @cImport — compatible with Zig 0.17+).
const Sql = struct {
    pub const Db = opaque {};
    pub const Stmt = opaque {};
};

pub const SQLITE_OK: c_int = 0;
pub const SQLITE_ROW: c_int = 100;
pub const SQLITE_DONE: c_int = 101;
pub const SQLITE_NULL: c_int = 5;
pub const SQLITE_MISUSE: c_int = 21;
pub const SQLITE_ERROR: c_int = 1;
pub const SQLITE_RANGE: c_int = 25;
pub const SQLITE_CONSTRAINT: c_int = 19;
pub const SQLITE_OPEN_READWRITE: c_int = 0x00000002;
pub const SQLITE_OPEN_CREATE: c_int = 0x00000004;
pub const SQLITE_OPEN_FULLMUTEX: c_int = 0x00010000;

/// `SQLITE_TRANSIENT` — SQLite copies bound string bytes (sentinel, not a real pointer).
const sqlite_transient: ?*anyopaque = @ptrFromInt(@as(usize, @bitCast(@as(isize, -1))));

pub extern "c" fn sqlite3_exec(
    db: ?*Sql.Db,
    sql: [*:0]const u8,
    callback: ?*anyopaque,
    first_arg: ?*anyopaque,
    errmsg: *?[*:0]u8,
) c_int;
pub extern "c" fn sqlite3_free(ptr: ?*anyopaque) void;
pub extern "c" fn sqlite3_prepare_v2(
    db: ?*Sql.Db,
    sql: [*:0]const u8,
    n_byte: c_int,
    pp_stmt: *?*Sql.Stmt,
    pz_tail: ?*anyopaque,
) c_int;
pub extern "c" fn sqlite3_finalize(stmt: ?*Sql.Stmt) c_int;
pub extern "c" fn sqlite3_step(stmt: ?*Sql.Stmt) c_int;
pub extern "c" fn sqlite3_column_int(stmt: ?*Sql.Stmt, i_col: c_int) c_int;
pub extern "c" fn sqlite3_column_int64(stmt: ?*Sql.Stmt, i_col: c_int) i64;
pub extern "c" fn sqlite3_column_text(stmt: ?*Sql.Stmt, i_col: c_int) ?[*]const u8;
pub extern "c" fn sqlite3_column_bytes(stmt: ?*Sql.Stmt, i_col: c_int) c_int;
pub extern "c" fn sqlite3_column_type(stmt: ?*Sql.Stmt, i_col: c_int) c_int;
pub extern "c" fn sqlite3_bind_text(
    stmt: ?*Sql.Stmt,
    idx: c_int,
    data: [*]const u8,
    n: c_int,
    destructor: ?*anyopaque,
) c_int;
pub extern "c" fn sqlite3_bind_int64(stmt: ?*Sql.Stmt, idx: c_int, v: i64) c_int;
pub extern "c" fn sqlite3_bind_null(stmt: ?*Sql.Stmt, idx: c_int) c_int;
pub extern "c" fn sqlite3_bind_int(stmt: ?*Sql.Stmt, idx: c_int, v: c_int) c_int;
pub extern "c" fn sqlite3_last_insert_rowid(db: ?*Sql.Db) i64;
pub extern "c" fn sqlite3_changes(db: ?*Sql.Db) c_int;
pub extern "c" fn sqlite3_busy_timeout(db: ?*Sql.Db, ms: c_int) c_int;
pub extern "c" fn sqlite3_errcode(db: ?*Sql.Db) c_int;
pub extern "c" fn sqlite3_open_v2(
    filename: ?[*:0]const u8,
    pp_db: *?*Sql.Db,
    flags: c_int,
    z_vfs: ?[*:0]const u8,
) c_int;
pub extern "c" fn sqlite3_close(db: ?*Sql.Db) c_int;

const title_max: usize = 255;
const desc_max: usize = 4096;
const name_max: usize = 64;
const color_max: usize = 32;

pub const CategoryRow = struct {
    id: i64,
    name: []const u8,
    color: []const u8,
};

pub const TodoRow = struct {
    id: i64,
    title: []const u8,
    description: []const u8,
    status: []const u8,
    priority: []const u8,
    category_id_is_null: bool,
    category_id: i64,
    parent_id_is_null: bool,
    parent_id: i64,
    due_date_is_null: bool,
    due_date: i64,
    position: c_int,
    created_at: i64,
    updated_at: i64,
};

pub const CategoryCb = *const fn (?*anyopaque, CategoryRow) void;
pub const TodoCb = *const fn (?*anyopaque, TodoRow) void;

pub const TodoStore = struct {
    db: *Sql.Db,

    fn execSql(self: *TodoStore, sql: [:0]const u8) c_int {
        var err: ?[*:0]u8 = null;
        const rc = sqlite3_exec(self.db, sql.ptr, null, null, &err);
        if (rc != SQLITE_OK) {
            if (err) |e| {
                std.debug.print("[todo_store] {s}\n", .{std.mem.span(e)});
                sqlite3_free(e);
            }
        }
        return rc;
    }

    fn readUserVersion(self: *TodoStore) c_int {
        var st: ?*Sql.Stmt = null;
        if (sqlite3_prepare_v2(self.db, "PRAGMA user_version;", -1, &st, null) != SQLITE_OK)
            return -1;
        defer _ = sqlite3_finalize(st);
        const step_rc = sqlite3_step(st.?);
        if (step_rc == SQLITE_ROW) {
            return sqlite3_column_int(st.?, 0);
        }
        return -1;
    }

    fn ensureSchema(self: *TodoStore) c_int {
        var ver = self.readUserVersion();
        if (ver < 0) return SQLITE_ERROR;

        if (self.execSql("PRAGMA foreign_keys = ON;") != SQLITE_OK)
            return SQLITE_ERROR;
        _ = self.execSql("PRAGMA journal_mode=WAL;");

        if (ver == 1) {
            if (self.execSql("DROP TABLE IF EXISTS todos;") != SQLITE_OK)
                return SQLITE_ERROR;
        }

        if (ver == 2) {
            const migrate =
                \\CREATE TABLE todos_new (
                \\  id INTEGER PRIMARY KEY AUTOINCREMENT,
                \\  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),
                \\  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),
                \\  status TEXT NOT NULL DEFAULT 'pending'
                \\    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),
                \\  priority TEXT NOT NULL DEFAULT 'medium'
                \\    CHECK(priority IN ('low','medium','high','urgent')),
                \\  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
                \\  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,
                \\  due_date INTEGER,
                \\  position INTEGER NOT NULL DEFAULT 0,
                \\  created_at INTEGER NOT NULL DEFAULT (unixepoch()),
                \\  updated_at INTEGER NOT NULL DEFAULT (unixepoch())
                \\);
                \\INSERT INTO todos_new(id, title, description, status, priority, category_id, parent_id, due_date,
                \\ position, created_at, updated_at)
                \\SELECT id, title, description, status, priority, category_id, parent_id, due_date, position,
                \\created_at, updated_at FROM todos;
                \\DROP TABLE todos;
                \\ALTER TABLE todos_new RENAME TO todos;
                \\CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);
                \\CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);
                \\CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);
                \\PRAGMA user_version = 3;
            ;
            if (self.execSql(migrate) != SQLITE_OK)
                return SQLITE_ERROR;
            return SQLITE_OK;
        }

        const schema_ddl_v3 =
            \\CREATE TABLE IF NOT EXISTS categories (
            \\  id INTEGER PRIMARY KEY AUTOINCREMENT,
            \\  name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 64),
            \\  color TEXT NOT NULL DEFAULT '#6366f1' CHECK(length(color) BETWEEN 1 AND 32)
            \\);
            \\CREATE TABLE IF NOT EXISTS todos (
            \\  id INTEGER PRIMARY KEY AUTOINCREMENT,
            \\  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),
            \\  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),
            \\  status TEXT NOT NULL DEFAULT 'pending'
            \\    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),
            \\  priority TEXT NOT NULL DEFAULT 'medium'
            \\    CHECK(priority IN ('low','medium','high','urgent')),
            \\  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
            \\  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,
            \\  due_date INTEGER,
            \\  position INTEGER NOT NULL DEFAULT 0,
            \\  created_at INTEGER NOT NULL DEFAULT (unixepoch()),
            \\  updated_at INTEGER NOT NULL DEFAULT (unixepoch())
            \\);
            \\CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);
            \\CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);
            \\CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);
        ;
        if (self.execSql(schema_ddl_v3) != SQLITE_OK)
            return SQLITE_ERROR;

        ver = self.readUserVersion();
        if (ver < 3 and self.execSql("PRAGMA user_version = 3;") != SQLITE_OK)
            return SQLITE_ERROR;
        return SQLITE_OK;
    }

    fn colText(st: *Sql.Stmt, i: c_int) []const u8 {
        const p = sqlite3_column_text(st, i);
        const n = sqlite3_column_bytes(st, i);
        if (p == null or n <= 0) return "";
        return @as([*c]const u8, @ptrCast(p.?))[0..@intCast(n)];
    }

    fn validateNameColor(name: []const u8, color: []const u8) c_int {
        if (name.len == 0 or color.len == 0) return SQLITE_MISUSE;
        if (name.len < 1 or name.len > name_max) return SQLITE_RANGE;
        if (color.len < 1 or color.len > color_max) return SQLITE_RANGE;
        return SQLITE_OK;
    }

    fn validateTodoTexts(title: []const u8, description: []const u8, status: []const u8, priority: []const u8) c_int {
        if (status.len == 0 or priority.len == 0) return SQLITE_MISUSE;
        if (title.len < 1 or title.len > title_max) return SQLITE_RANGE;
        if (description.len > desc_max) return SQLITE_RANGE;
        if (!std.mem.eql(u8, status, "pending") and
            !std.mem.eql(u8, status, "in_progress") and
            !std.mem.eql(u8, status, "done") and
            !std.mem.eql(u8, status, "cancelled") and
            !std.mem.eql(u8, status, "archived")) return SQLITE_CONSTRAINT;
        if (!std.mem.eql(u8, priority, "low") and
            !std.mem.eql(u8, priority, "medium") and
            !std.mem.eql(u8, priority, "high") and
            !std.mem.eql(u8, priority, "urgent")) return SQLITE_CONSTRAINT;
        return SQLITE_OK;
    }
};

pub fn storeOpen(path: [:0]const u8) ?*TodoStore {
    const uri: [:0]const u8 = if (path.len == 0) ":memory:" else path;
    const s = std.heap.c_allocator.create(TodoStore) catch return null;
    errdefer std.heap.c_allocator.destroy(s);

    var db: ?*Sql.Db = null;
    const rc = sqlite3_open_v2(
        uri.ptr,
        &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        null,
    );
    if (rc != SQLITE_OK or db == null) {
        std.debug.print("[todo_store] open failed\n", .{});
        if (db) |d| _ = sqlite3_close(d);
        std.heap.c_allocator.destroy(s);
        return null;
    }
    s.* = .{ .db = db.? };
    _ = sqlite3_busy_timeout(s.db, 5000);
    if (s.ensureSchema() != SQLITE_OK) {
        _ = sqlite3_close(s.db);
        std.heap.c_allocator.destroy(s);
        return null;
    }
    return s;
}

pub fn storeClose(store: ?*TodoStore) void {
    const s = store orelse return;
    _ = sqlite3_close(s.db);
    std.heap.c_allocator.destroy(s);
}

pub fn storeCategoriesForeach(store: *TodoStore, userdata: ?*anyopaque, cb: CategoryCb) c_int {
    var st: ?*Sql.Stmt = null;
    const q = "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;";
    if (sqlite3_prepare_v2(store.db, q, -1, &st, null) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    defer _ = sqlite3_finalize(st);

    var rc = sqlite3_step(st.?);
    while (rc == SQLITE_ROW) {
        const row = CategoryRow{
            .id = sqlite3_column_int64(st.?, 0),
            .name = TodoStore.colText(st.?, 1),
            .color = TodoStore.colText(st.?, 2),
        };
        cb(userdata, row);
        rc = sqlite3_step(st.?);
    }
    return if (rc == SQLITE_DONE) SQLITE_OK else rc;
}

pub fn storeCategoryInsert(store: *TodoStore, name: []const u8, color: []const u8, out_id: *i64) c_int {
    const vnc = TodoStore.validateNameColor(name, color);
    if (vnc != SQLITE_OK) return vnc;
    var st: ?*Sql.Stmt = null;
    const q = "INSERT INTO categories(name, color) VALUES(?, ?);";
    if (sqlite3_prepare_v2(store.db, q, -1, &st, null) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    defer _ = sqlite3_finalize(st);
    if (sqlite3_bind_text(st.?, 1, name.ptr, @intCast(name.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (sqlite3_bind_text(st.?, 2, color.ptr, @intCast(color.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    const step_rc = sqlite3_step(st.?);
    if (step_rc != SQLITE_DONE)
        return if (step_rc == SQLITE_ROW) SQLITE_ERROR else step_rc;
    out_id.* = sqlite3_last_insert_rowid(store.db);
    return SQLITE_OK;
}

pub fn storeCategoryDeleteById(store: *TodoStore, id: i64) c_int {
    var st: ?*Sql.Stmt = null;
    const q = "DELETE FROM categories WHERE id = ?;";
    if (sqlite3_prepare_v2(store.db, q, -1, &st, null) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    defer _ = sqlite3_finalize(st);
    if (sqlite3_bind_int64(st.?, 1, id) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    const step_rc = sqlite3_step(st.?);
    if (step_rc != SQLITE_DONE)
        return if (step_rc == SQLITE_ROW) SQLITE_ERROR else step_rc;
    return if (sqlite3_changes(store.db) > 0) SQLITE_OK else SQLITE_ERROR;
}

pub fn storeTodosForeach(store: *TodoStore, userdata: ?*anyopaque, cb: TodoCb) c_int {
    var st: ?*Sql.Stmt = null;
    const q =
        \\SELECT id, title, description, status, priority, category_id, parent_id, due_date,
        \\ position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;
    ;
    if (sqlite3_prepare_v2(store.db, q, -1, &st, null) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    defer _ = sqlite3_finalize(st);

    var rc = sqlite3_step(st.?);
    while (rc == SQLITE_ROW) {
        const row = TodoRow{
            .id = sqlite3_column_int64(st.?, 0),
            .title = TodoStore.colText(st.?, 1),
            .description = TodoStore.colText(st.?, 2),
            .status = TodoStore.colText(st.?, 3),
            .priority = TodoStore.colText(st.?, 4),
            .category_id_is_null = sqlite3_column_type(st.?, 5) == SQLITE_NULL,
            .category_id = if (sqlite3_column_type(st.?, 5) == SQLITE_NULL) 0 else sqlite3_column_int64(st.?, 5),
            .parent_id_is_null = sqlite3_column_type(st.?, 6) == SQLITE_NULL,
            .parent_id = if (sqlite3_column_type(st.?, 6) == SQLITE_NULL) 0 else sqlite3_column_int64(st.?, 6),
            .due_date_is_null = sqlite3_column_type(st.?, 7) == SQLITE_NULL,
            .due_date = if (sqlite3_column_type(st.?, 7) == SQLITE_NULL) 0 else sqlite3_column_int64(st.?, 7),
            .position = sqlite3_column_int(st.?, 8),
            .created_at = sqlite3_column_int64(st.?, 9),
            .updated_at = sqlite3_column_int64(st.?, 10),
        };
        cb(userdata, row);
        rc = sqlite3_step(st.?);
    }
    return if (rc == SQLITE_DONE) SQLITE_OK else rc;
}

pub fn storeTodoInsert(
    store: *TodoStore,
    title: []const u8,
    description: []const u8,
    status: []const u8,
    priority: []const u8,
    category_id_set: bool,
    category_id: i64,
    parent_id_set: bool,
    parent_id: i64,
    due_date_set: bool,
    due_date: i64,
    out_id: *i64,
) c_int {
    const v = TodoStore.validateTodoTexts(title, description, status, priority);
    if (v != SQLITE_OK) return v;
    var st: ?*Sql.Stmt = null;
    const q =
        \\INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)
        \\ VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));
    ;
    if (sqlite3_prepare_v2(store.db, q, -1, &st, null) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    defer _ = sqlite3_finalize(st);
    if (sqlite3_bind_text(st.?, 1, title.ptr, @intCast(title.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (sqlite3_bind_text(st.?, 2, description.ptr, @intCast(description.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (sqlite3_bind_text(st.?, 3, status.ptr, @intCast(status.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (sqlite3_bind_text(st.?, 4, priority.ptr, @intCast(priority.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (category_id_set and category_id > 0) {
        if (sqlite3_bind_int64(st.?, 5, category_id) != SQLITE_OK) return sqlite3_errcode(store.db);
    } else {
        if (sqlite3_bind_null(st.?, 5) != SQLITE_OK) return sqlite3_errcode(store.db);
    }
    if (parent_id_set and parent_id > 0) {
        if (sqlite3_bind_int64(st.?, 6, parent_id) != SQLITE_OK) return sqlite3_errcode(store.db);
    } else {
        if (sqlite3_bind_null(st.?, 6) != SQLITE_OK) return sqlite3_errcode(store.db);
    }
    if (due_date_set) {
        if (sqlite3_bind_int64(st.?, 7, due_date) != SQLITE_OK) return sqlite3_errcode(store.db);
    } else {
        if (sqlite3_bind_null(st.?, 7) != SQLITE_OK) return sqlite3_errcode(store.db);
    }
    const step_rc = sqlite3_step(st.?);
    if (step_rc != SQLITE_DONE)
        return if (step_rc == SQLITE_ROW) SQLITE_ERROR else step_rc;
    out_id.* = sqlite3_last_insert_rowid(store.db);
    return SQLITE_OK;
}

pub fn storeTodoUpdateFull(
    store: *TodoStore,
    id: i64,
    title: []const u8,
    description: []const u8,
    status: []const u8,
    priority: []const u8,
    category_id_set: bool,
    category_id: i64,
    parent_id_set: bool,
    parent_id: i64,
    due_date_set: bool,
    due_date: i64,
    position: c_int,
) c_int {
    const v = TodoStore.validateTodoTexts(title, description, status, priority);
    if (v != SQLITE_OK) return v;
    var st: ?*Sql.Stmt = null;
    const q =
        \\UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?,
        \\ due_date=?, position=?, updated_at=unixepoch() WHERE id=?;
    ;
    if (sqlite3_prepare_v2(store.db, q, -1, &st, null) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    defer _ = sqlite3_finalize(st);
    if (sqlite3_bind_text(st.?, 1, title.ptr, @intCast(title.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (sqlite3_bind_text(st.?, 2, description.ptr, @intCast(description.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (sqlite3_bind_text(st.?, 3, status.ptr, @intCast(status.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (sqlite3_bind_text(st.?, 4, priority.ptr, @intCast(priority.len), sqlite_transient) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    if (category_id_set and category_id > 0) {
        if (sqlite3_bind_int64(st.?, 5, category_id) != SQLITE_OK) return sqlite3_errcode(store.db);
    } else {
        if (sqlite3_bind_null(st.?, 5) != SQLITE_OK) return sqlite3_errcode(store.db);
    }
    if (parent_id_set and parent_id > 0) {
        if (sqlite3_bind_int64(st.?, 6, parent_id) != SQLITE_OK) return sqlite3_errcode(store.db);
    } else {
        if (sqlite3_bind_null(st.?, 6) != SQLITE_OK) return sqlite3_errcode(store.db);
    }
    if (due_date_set) {
        if (sqlite3_bind_int64(st.?, 7, due_date) != SQLITE_OK) return sqlite3_errcode(store.db);
    } else {
        if (sqlite3_bind_null(st.?, 7) != SQLITE_OK) return sqlite3_errcode(store.db);
    }
    if (sqlite3_bind_int(st.?, 8, position) != SQLITE_OK) return sqlite3_errcode(store.db);
    if (sqlite3_bind_int64(st.?, 9, id) != SQLITE_OK) return sqlite3_errcode(store.db);
    const step_rc = sqlite3_step(st.?);
    if (step_rc != SQLITE_DONE)
        return if (step_rc == SQLITE_ROW) SQLITE_ERROR else step_rc;
    return if (sqlite3_changes(store.db) > 0) SQLITE_OK else SQLITE_ERROR;
}

pub fn storeTodoDeleteById(store: *TodoStore, id: i64) c_int {
    var st: ?*Sql.Stmt = null;
    const q = "DELETE FROM todos WHERE id = ?;";
    if (sqlite3_prepare_v2(store.db, q, -1, &st, null) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    defer _ = sqlite3_finalize(st);
    if (sqlite3_bind_int64(st.?, 1, id) != SQLITE_OK)
        return sqlite3_errcode(store.db);
    const step_rc = sqlite3_step(st.?);
    if (step_rc != SQLITE_DONE)
        return if (step_rc == SQLITE_ROW) SQLITE_ERROR else step_rc;
    return if (sqlite3_changes(store.db) > 0) SQLITE_OK else SQLITE_ERROR;
}
