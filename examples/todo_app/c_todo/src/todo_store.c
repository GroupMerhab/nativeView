/*
 * todo_store.c — WAL SQLite schema v3 (categories + todos; status includes archived).
 */

#include "todo_store.h"
#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

#define TITLE_MAX     255
#define DESC_MAX      4096
#define NAME_MAX      64
#define COLOR_MAX     32

struct todo_store {
  sqlite3* db;
};

static int exec_sql(sqlite3* db, const char* sql) {
  char* err = NULL;
  int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[todo_store] %s\n", err ? err : sqlite3_errmsg(db));
    sqlite3_free(err);
  }
  return rc;
}

static int read_user_version(sqlite3* db) {
  sqlite3_stmt* st = NULL;
  int v = -1;
  if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &st, NULL) != SQLITE_OK) return -1;
  if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
  sqlite3_finalize(st);
  return v;
}

static const char* schema_ddl_v3 =
  "CREATE TABLE IF NOT EXISTS categories ("
  "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
  "  name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 64),"
  "  color TEXT NOT NULL DEFAULT '#6366f1' CHECK(length(color) BETWEEN 1 AND 32)"
  ");"
  "CREATE TABLE IF NOT EXISTS todos ("
  "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
  "  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),"
  "  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),"
  "  status TEXT NOT NULL DEFAULT 'pending'"
  "    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),"
  "  priority TEXT NOT NULL DEFAULT 'medium'"
  "    CHECK(priority IN ('low','medium','high','urgent')),"
  "  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,"
  "  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,"
  "  due_date INTEGER,"
  "  position INTEGER NOT NULL DEFAULT 0,"
  "  created_at INTEGER NOT NULL DEFAULT (unixepoch()),"
  "  updated_at INTEGER NOT NULL DEFAULT (unixepoch())"
  ");"
  "CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);"
  "CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);"
  "CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);";

static int migrate_todos_v2_to_v3(sqlite3* db) {
  const char* sql =
    "CREATE TABLE todos_new ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),"
    "  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),"
    "  status TEXT NOT NULL DEFAULT 'pending'"
    "    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),"
    "  priority TEXT NOT NULL DEFAULT 'medium'"
    "    CHECK(priority IN ('low','medium','high','urgent')),"
    "  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,"
    "  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,"
    "  due_date INTEGER,"
    "  position INTEGER NOT NULL DEFAULT 0,"
    "  created_at INTEGER NOT NULL DEFAULT (unixepoch()),"
    "  updated_at INTEGER NOT NULL DEFAULT (unixepoch())"
    ");"
    "INSERT INTO todos_new(id, title, description, status, priority, category_id, parent_id, due_date,"
    " position, created_at, updated_at) "
    "SELECT id, title, description, status, priority, category_id, parent_id, due_date, position, "
    "created_at, updated_at FROM todos;"
    "DROP TABLE todos;"
    "ALTER TABLE todos_new RENAME TO todos;"
    "CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);"
    "CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);"
    "CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);"
    "PRAGMA user_version = 3;";
  return exec_sql(db, sql);
}

static int ensure_schema(sqlite3* db) {
  int ver = read_user_version(db);
  if (ver < 0) return SQLITE_ERROR;

  if (exec_sql(db, "PRAGMA foreign_keys = ON;") != SQLITE_OK) return SQLITE_ERROR;

  /* WAL is ignored for :memory: on some builds; harmless if it fails */
  exec_sql(db, "PRAGMA journal_mode=WAL;");

  if (ver == 1) {
    if (exec_sql(db, "DROP TABLE IF EXISTS todos;") != SQLITE_OK) return SQLITE_ERROR;
  }

  if (ver == 2) {
    if (migrate_todos_v2_to_v3(db) != SQLITE_OK) return SQLITE_ERROR;
    return SQLITE_OK;
  }

  if (exec_sql(db, schema_ddl_v3) != SQLITE_OK) return SQLITE_ERROR;

  ver = read_user_version(db);
  if (ver < 3 && exec_sql(db, "PRAGMA user_version = 3;") != SQLITE_OK) return SQLITE_ERROR;
  return SQLITE_OK;
}

todo_store_t* todo_store_open(const char* path_utf8_or_null_memory) {
  const char* uri = path_utf8_or_null_memory ? path_utf8_or_null_memory : ":memory:";
  todo_store_t* s = (todo_store_t*)sqlite3_malloc(sizeof(todo_store_t));
  if (!s) return NULL;
  s->db = NULL;
  int rc = sqlite3_open_v2(uri, &s->db,
    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
    NULL);
  if (rc != SQLITE_OK || !s->db) {
    fprintf(stderr, "[todo_store] open failed: %s\n", s->db ? sqlite3_errmsg(s->db) : "no db");
    if (s->db) sqlite3_close(s->db);
    sqlite3_free(s);
    return NULL;
  }
  sqlite3_busy_timeout(s->db, 5000);
  if (ensure_schema(s->db) != SQLITE_OK) {
    sqlite3_close(s->db);
    sqlite3_free(s);
    return NULL;
  }
  return s;
}

void todo_store_close(todo_store_t* store) {
  if (!store) return;
  if (store->db) sqlite3_close(store->db);
  sqlite3_free(store);
}

int todo_store_categories_foreach(todo_store_t* store, todo_store_category_cb cb, void* userdata) {
  if (!store || !store->db || !cb) return SQLITE_MISUSE;
  sqlite3_stmt* st = NULL;
  int rc = sqlite3_prepare_v2(store->db,
    "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;", -1, &st, NULL);
  if (rc != SQLITE_OK) return rc;
  while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
    todo_store_category_row_t row;
    row.id = (long long)sqlite3_column_int64(st, 0);
    row.name = (const char*)sqlite3_column_text(st, 1);
    row.color = (const char*)sqlite3_column_text(st, 2);
    cb(userdata, &row);
  }
  sqlite3_finalize(st);
  return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int validate_name_color(const char* name, const char* color) {
  if (!name || !color) return SQLITE_MISUSE;
  size_t nl = strnlen(name, NAME_MAX + 1);
  size_t cl = strnlen(color, COLOR_MAX + 1);
  if (nl < 1 || nl > NAME_MAX) return SQLITE_RANGE;
  if (cl < 1 || cl > COLOR_MAX) return SQLITE_RANGE;
  return SQLITE_OK;
}

int todo_store_category_insert(todo_store_t* store, const char* name, const char* color,
                               long long* out_id) {
  if (!store || !store->db || !out_id) return SQLITE_MISUSE;
  int v = validate_name_color(name, color);
  if (v != SQLITE_OK) return v;
  sqlite3_stmt* st = NULL;
  int rc = sqlite3_prepare_v2(store->db,
    "INSERT INTO categories(name, color) VALUES(?, ?);", -1, &st, NULL);
  if (rc != SQLITE_OK) return rc;
  sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 2, color, -1, SQLITE_STATIC);
  rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE) return rc == SQLITE_ROW ? SQLITE_ERROR : rc;
  *out_id = (long long)sqlite3_last_insert_rowid(store->db);
  return SQLITE_OK;
}

int todo_store_category_delete_by_id(todo_store_t* store, long long id) {
  if (!store || !store->db) return SQLITE_MISUSE;
  sqlite3_stmt* st = NULL;
  int rc = sqlite3_prepare_v2(store->db, "DELETE FROM categories WHERE id = ?;", -1, &st, NULL);
  if (rc != SQLITE_OK) return rc;
  sqlite3_bind_int64(st, 1, (sqlite3_int64)id);
  rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE) return rc == SQLITE_ROW ? SQLITE_ERROR : rc;
  return sqlite3_changes64(store->db) > 0 ? SQLITE_OK : SQLITE_ERROR;
}

int todo_store_todos_foreach(todo_store_t* store, todo_store_todo_cb cb, void* userdata) {
  if (!store || !store->db || !cb) return SQLITE_MISUSE;
  sqlite3_stmt* st = NULL;
  int rc = sqlite3_prepare_v2(store->db,
    "SELECT id, title, description, status, priority, category_id, parent_id, due_date,"
    " position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;",
    -1, &st, NULL);
  if (rc != SQLITE_OK) return rc;
  while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
    todo_store_todo_row_t row;
    row.id = (long long)sqlite3_column_int64(st, 0);
    row.title = (const char*)sqlite3_column_text(st, 1);
    row.description = (const char*)sqlite3_column_text(st, 2);
    row.status = (const char*)sqlite3_column_text(st, 3);
    row.priority = (const char*)sqlite3_column_text(st, 4);
    if (sqlite3_column_type(st, 5) == SQLITE_NULL) {
      row.category_id_is_null = 1;
      row.category_id = 0;
    } else {
      row.category_id_is_null = 0;
      row.category_id = (long long)sqlite3_column_int64(st, 5);
    }
    if (sqlite3_column_type(st, 6) == SQLITE_NULL) {
      row.parent_id_is_null = 1;
      row.parent_id = 0;
    } else {
      row.parent_id_is_null = 0;
      row.parent_id = (long long)sqlite3_column_int64(st, 6);
    }
    if (sqlite3_column_type(st, 7) == SQLITE_NULL) {
      row.due_date_is_null = 1;
      row.due_date = 0;
    } else {
      row.due_date_is_null = 0;
      row.due_date = (long long)sqlite3_column_int64(st, 7);
    }
    row.position = sqlite3_column_int(st, 8);
    row.created_at = (long long)sqlite3_column_int64(st, 9);
    row.updated_at = (long long)sqlite3_column_int64(st, 10);
    cb(userdata, &row);
  }
  sqlite3_finalize(st);
  return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int validate_todo_texts(const char* title, const char* description, const char* status,
                               const char* priority) {
  if (!title || !description || !status || !priority) return SQLITE_MISUSE;
  size_t tl = strnlen(title, TITLE_MAX + 1);
  size_t dl = strnlen(description, DESC_MAX + 1);
  if (tl < 1 || tl > TITLE_MAX) return SQLITE_RANGE;
  if (dl > DESC_MAX) return SQLITE_RANGE;
  if (strcmp(status, "pending") != 0 && strcmp(status, "in_progress") != 0 &&
      strcmp(status, "done") != 0 && strcmp(status, "cancelled") != 0 &&
      strcmp(status, "archived") != 0)
    return SQLITE_CONSTRAINT;
  if (strcmp(priority, "low") != 0 && strcmp(priority, "medium") != 0 &&
      strcmp(priority, "high") != 0 && strcmp(priority, "urgent") != 0)
    return SQLITE_CONSTRAINT;
  return SQLITE_OK;
}

int todo_store_todo_insert(todo_store_t* store, const char* title, const char* description,
                           const char* status, const char* priority, int category_id_set,
                           long long category_id, int parent_id_set, long long parent_id,
                           int due_date_set, long long due_date, long long* out_id) {
  if (!store || !store->db || !out_id) return SQLITE_MISUSE;
  int v = validate_todo_texts(title, description, status, priority);
  if (v != SQLITE_OK) return v;

  sqlite3_stmt* st = NULL;
  int rc = sqlite3_prepare_v2(store->db,
    "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)"
    " VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));",
    -1, &st, NULL);
  if (rc != SQLITE_OK) return rc;
  sqlite3_bind_text(st, 1, title, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 2, description, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 3, status, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 4, priority, -1, SQLITE_STATIC);
  if (category_id_set && category_id > 0) sqlite3_bind_int64(st, 5, (sqlite3_int64)category_id);
  else sqlite3_bind_null(st, 5);
  if (parent_id_set && parent_id > 0) sqlite3_bind_int64(st, 6, (sqlite3_int64)parent_id);
  else sqlite3_bind_null(st, 6);
  if (due_date_set) sqlite3_bind_int64(st, 7, (sqlite3_int64)due_date);
  else sqlite3_bind_null(st, 7);
  rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE) return rc == SQLITE_ROW ? SQLITE_ERROR : rc;
  *out_id = (long long)sqlite3_last_insert_rowid(store->db);
  return SQLITE_OK;
}

int todo_store_todo_update_full(todo_store_t* store, long long id, const char* title,
                                 const char* description, const char* status, const char* priority,
                                 int category_id_set, long long category_id, int parent_id_set,
                                 long long parent_id, int due_date_set, long long due_date,
                                 int position) {
  if (!store || !store->db) return SQLITE_MISUSE;
  int v = validate_todo_texts(title, description, status, priority);
  if (v != SQLITE_OK) return v;

  sqlite3_stmt* st = NULL;
  int rc = sqlite3_prepare_v2(store->db,
    "UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?,"
    " due_date=?, position=?, updated_at=unixepoch() WHERE id=?;",
    -1, &st, NULL);
  if (rc != SQLITE_OK) return rc;
  sqlite3_bind_text(st, 1, title, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 2, description, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 3, status, -1, SQLITE_STATIC);
  sqlite3_bind_text(st, 4, priority, -1, SQLITE_STATIC);
  if (category_id_set && category_id > 0) sqlite3_bind_int64(st, 5, (sqlite3_int64)category_id);
  else sqlite3_bind_null(st, 5);
  if (parent_id_set && parent_id > 0) sqlite3_bind_int64(st, 6, (sqlite3_int64)parent_id);
  else sqlite3_bind_null(st, 6);
  if (due_date_set) sqlite3_bind_int64(st, 7, (sqlite3_int64)due_date);
  else sqlite3_bind_null(st, 7);
  sqlite3_bind_int(st, 8, position);
  sqlite3_bind_int64(st, 9, (sqlite3_int64)id);
  rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE) return rc == SQLITE_ROW ? SQLITE_ERROR : rc;
  return sqlite3_changes64(store->db) > 0 ? SQLITE_OK : SQLITE_ERROR;
}

int todo_store_todo_delete_by_id(todo_store_t* store, long long id) {
  if (!store || !store->db) return SQLITE_MISUSE;
  sqlite3_stmt* st = NULL;
  int rc = sqlite3_prepare_v2(store->db, "DELETE FROM todos WHERE id = ?;", -1, &st, NULL);
  if (rc != SQLITE_OK) return rc;
  sqlite3_bind_int64(st, 1, (sqlite3_int64)id);
  rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE) return rc == SQLITE_ROW ? SQLITE_ERROR : rc;
  return sqlite3_changes64(store->db) > 0 ? SQLITE_OK : SQLITE_ERROR;
}
