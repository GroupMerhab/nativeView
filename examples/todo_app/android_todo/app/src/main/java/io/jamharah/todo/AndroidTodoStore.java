/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

import android.content.Context;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteStatement;
import java.util.function.Consumer;

/**
 * SQLite persistence for todo_app on Android ({@link SQLiteOpenHelper}), same schema as {@code
 * java_todo} / {@code nim_todo}.
 */
public final class AndroidTodoStore implements AutoCloseable {

  private static final int DB_VERSION = 3;
  private static final int TITLE_MAX = 255;
  private static final int DESC_MAX = 4096;
  private static final int NAME_MAX = 64;
  private static final int COLOR_MAX = 32;

  private static final String SCHEMA_DDL_V3 =
      "CREATE TABLE IF NOT EXISTS categories (\n"
          + "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
          + "  name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 64),\n"
          + "  color TEXT NOT NULL DEFAULT '#6366f1' CHECK(length(color) BETWEEN 1 AND 32)\n"
          + ");\n"
          + "CREATE TABLE IF NOT EXISTS todos (\n"
          + "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
          + "  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),\n"
          + "  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),\n"
          + "  status TEXT NOT NULL DEFAULT 'pending'\n"
          + "    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),\n"
          + "  priority TEXT NOT NULL DEFAULT 'medium'\n"
          + "    CHECK(priority IN ('low','medium','high','urgent')),\n"
          + "  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,\n"
          + "  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,\n"
          + "  due_date INTEGER,\n"
          + "  position INTEGER NOT NULL DEFAULT 0,\n"
          + "  created_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER)),\n"
          + "  updated_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))\n"
          + ");\n"
          + "CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);\n"
          + "CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);\n"
          + "CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);\n";

  private static final String MIGRATE_V2_TO_V3 =
      "CREATE TABLE todos_new (\n"
          + "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
          + "  title TEXT NOT NULL CHECK(length(title) BETWEEN 1 AND 255),\n"
          + "  description TEXT NOT NULL DEFAULT '' CHECK(length(description) <= 4096),\n"
          + "  status TEXT NOT NULL DEFAULT 'pending'\n"
          + "    CHECK(status IN ('pending','in_progress','done','cancelled','archived')),\n"
          + "  priority TEXT NOT NULL DEFAULT 'medium'\n"
          + "    CHECK(priority IN ('low','medium','high','urgent')),\n"
          + "  category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,\n"
          + "  parent_id INTEGER REFERENCES todos(id) ON DELETE CASCADE,\n"
          + "  due_date INTEGER,\n"
          + "  position INTEGER NOT NULL DEFAULT 0,\n"
          + "  created_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER)),\n"
          + "  updated_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))\n"
          + ");\n"
          + "INSERT INTO todos_new(id, title, description, status, priority, category_id, parent_id, due_date,\n"
          + " position, created_at, updated_at)\n"
          + "SELECT id, title, description, status, priority, category_id, parent_id, due_date, position,\n"
          + "created_at, updated_at FROM todos;\n"
          + "DROP TABLE todos;\n"
          + "ALTER TABLE todos_new RENAME TO todos;\n"
          + "CREATE INDEX IF NOT EXISTS idx_todos_status ON todos(status);\n"
          + "CREATE INDEX IF NOT EXISTS idx_todos_priority ON todos(priority);\n"
          + "CREATE INDEX IF NOT EXISTS idx_todos_category ON todos(category_id);\n"
          + "PRAGMA user_version = 3;\n";

  private final TodoOpenHelper helper;

  private AndroidTodoStore(TodoOpenHelper helper) {
    this.helper = helper;
  }

  /** Opens the store in the app databases directory ({@code todo_app.db}). */
  public static AndroidTodoStore storeOpen(Context context) {
    if (context == null) {
      return null;
    }
    Context app = context.getApplicationContext();
    try {
      TodoOpenHelper h = new TodoOpenHelper(app);
      SQLiteDatabase db = h.getWritableDatabase();
      if (db == null) {
        return null;
      }
      if (ensureSchema(db) != SqliteRc.SQLITE_OK) {
        h.close();
        return null;
      }
      return new AndroidTodoStore(h);
    } catch (SQLException e) {
      System.err.println("[android_todo_store] open failed: " + e.getMessage());
      return null;
    }
  }

  public static void storeClose(AndroidTodoStore s) {
    if (s != null) {
      s.close();
    }
  }

  @Override
  public void close() {
    helper.close();
  }

  private SQLiteDatabase db() {
    return helper.getWritableDatabase();
  }

  private static int execSql(SQLiteDatabase db, String sql) {
    for (String part : sql.split(";")) {
      String p = part.trim();
      if (p.isEmpty()) {
        continue;
      }
      try {
        db.execSQL(p);
      } catch (SQLException e) {
        System.err.println("[android_todo_store] " + e.getMessage());
        return SqliteRc.SQLITE_ERROR;
      }
    }
    return SqliteRc.SQLITE_OK;
  }

  private static int readUserVersion(SQLiteDatabase db) {
    try (Cursor c = db.rawQuery("PRAGMA user_version;", null)) {
      if (c.moveToFirst()) {
        return c.getInt(0);
      }
    } catch (SQLException ignored) {
      // fall through
    }
    return -1;
  }

  private static void tryJournalWal(SQLiteDatabase db) {
    try (Cursor c = db.rawQuery("PRAGMA journal_mode=WAL", null)) {
      if (c.moveToFirst()) {
        c.getString(0);
      }
    } catch (SQLException ignored) {
      // ignore
    }
  }

  private static int ensureSchema(SQLiteDatabase db) {
    int ver = readUserVersion(db);
    if (ver < 0) {
      return SqliteRc.SQLITE_ERROR;
    }
    try {
      db.execSQL("PRAGMA foreign_keys = ON;");
    } catch (SQLException e) {
      return SqliteRc.SQLITE_ERROR;
    }
    tryJournalWal(db);
    if (ver == 1) {
      if (execSql(db, "DROP TABLE IF EXISTS todos;") != SqliteRc.SQLITE_OK) {
        return SqliteRc.SQLITE_ERROR;
      }
    }
    if (ver == 2) {
      return execSql(db, MIGRATE_V2_TO_V3);
    }
    if (execSql(db, SCHEMA_DDL_V3) != SqliteRc.SQLITE_OK) {
      return SqliteRc.SQLITE_ERROR;
    }
    ver = readUserVersion(db);
    if (ver < 3 && execSql(db, "PRAGMA user_version = 3;") != SqliteRc.SQLITE_OK) {
      return SqliteRc.SQLITE_ERROR;
    }
    return SqliteRc.SQLITE_OK;
  }

  private static int validateNameColor(String name, String color) {
    if (name == null || color == null || name.isEmpty() || color.isEmpty()) {
      return SqliteRc.SQLITE_MISUSE;
    }
    int nl = name.length();
    int cl = color.length();
    if (nl < 1 || nl > NAME_MAX || cl < 1 || cl > COLOR_MAX) {
      return SqliteRc.SQLITE_RANGE;
    }
    return SqliteRc.SQLITE_OK;
  }

  private static int validateTodoTexts(String title, String description, String status, String priority) {
    if (status == null || priority == null || status.isEmpty() || priority.isEmpty()) {
      return SqliteRc.SQLITE_MISUSE;
    }
    if (title == null || description == null) {
      return SqliteRc.SQLITE_MISUSE;
    }
    int tl = title.length();
    int dl = description.length();
    if (tl < 1 || tl > TITLE_MAX || dl > DESC_MAX) {
      return SqliteRc.SQLITE_RANGE;
    }
    if (!status.equals("pending")
        && !status.equals("in_progress")
        && !status.equals("done")
        && !status.equals("cancelled")
        && !status.equals("archived")) {
      return SqliteRc.SQLITE_CONSTRAINT;
    }
    if (!priority.equals("low")
        && !priority.equals("medium")
        && !priority.equals("high")
        && !priority.equals("urgent")) {
      return SqliteRc.SQLITE_CONSTRAINT;
    }
    return SqliteRc.SQLITE_OK;
  }

  public int storeCategoriesForeach(Consumer<TodoStoreCategoryRow> fn) {
    if (fn == null) {
      return SqliteRc.SQLITE_MISUSE;
    }
    SQLiteDatabase db = db();
    try (Cursor c =
        db.rawQuery(
            "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;", null)) {
      while (c.moveToNext()) {
        fn.accept(new TodoStoreCategoryRow(c.getLong(0), c.getString(1), c.getString(2)));
      }
      return SqliteRc.SQLITE_OK;
    } catch (SQLException e) {
      return SqliteRc.SQLITE_ERROR;
    }
  }

  public int storeCategoryInsert(String name, String color, long[] outId) {
    if (outId == null || outId.length < 1) {
      return SqliteRc.SQLITE_MISUSE;
    }
    int v = validateNameColor(name, color);
    if (v != SqliteRc.SQLITE_OK) {
      return v;
    }
    SQLiteDatabase db = db();
    try {
      SQLiteStatement st = db.compileStatement("INSERT INTO categories(name, color) VALUES(?, ?);");
      st.bindString(1, name);
      st.bindString(2, color);
      long row = st.executeInsert();
      st.close();
      outId[0] = row;
      if (outId[0] <= 0) {
        outId[0] = DatabaseUtils.longForQuery(db, "SELECT last_insert_rowid()", null);
      }
      return outId[0] > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      if (e.getMessage() != null && e.getMessage().contains("UNIQUE")) {
        return SqliteRc.SQLITE_CONSTRAINT;
      }
      return SqliteRc.SQLITE_ERROR;
    }
  }

  public int storeCategoryDeleteById(long id) {
    SQLiteDatabase db = db();
    try {
      SQLiteStatement st = db.compileStatement("DELETE FROM categories WHERE id = ?;");
      st.bindLong(1, id);
      int n = st.executeUpdateDelete();
      st.close();
      return n > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      return SqliteRc.SQLITE_ERROR;
    }
  }

  public int storeTodosForeach(Consumer<TodoStoreTodoRow> fn) {
    if (fn == null) {
      return SqliteRc.SQLITE_MISUSE;
    }
    SQLiteDatabase db = db();
    try (Cursor c =
        db.rawQuery(
            "SELECT id, title, description, status, priority, category_id, parent_id, due_date,"
                + " position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;",
            null)) {
      int iId = c.getColumnIndexOrThrow("id");
      int iTitle = c.getColumnIndexOrThrow("title");
      int iDesc = c.getColumnIndexOrThrow("description");
      int iStatus = c.getColumnIndexOrThrow("status");
      int iPri = c.getColumnIndexOrThrow("priority");
      int iCat = c.getColumnIndexOrThrow("category_id");
      int iPar = c.getColumnIndexOrThrow("parent_id");
      int iDue = c.getColumnIndexOrThrow("due_date");
      int iPos = c.getColumnIndexOrThrow("position");
      int iCr = c.getColumnIndexOrThrow("created_at");
      int iUp = c.getColumnIndexOrThrow("updated_at");
      while (c.moveToNext()) {
        long cat = c.getLong(iCat);
        boolean catNull = c.isNull(iCat);
        long par = c.getLong(iPar);
        boolean parNull = c.isNull(iPar);
        long due = c.getLong(iDue);
        boolean dueNull = c.isNull(iDue);
        fn.accept(
            new TodoStoreTodoRow(
                c.getLong(iId),
                c.getString(iTitle),
                c.getString(iDesc),
                c.getString(iStatus),
                c.getString(iPri),
                catNull,
                cat,
                parNull,
                par,
                dueNull,
                due,
                c.getInt(iPos),
                c.getLong(iCr),
                c.getLong(iUp)));
      }
      return SqliteRc.SQLITE_OK;
    } catch (SQLException e) {
      return SqliteRc.SQLITE_ERROR;
    }
  }

  public int storeTodoInsert(
      String title,
      String description,
      String status,
      String priority,
      boolean categoryIdSet,
      long categoryId,
      boolean parentIdSet,
      long parentId,
      boolean dueDateSet,
      long dueDate,
      long[] outId) {
    if (outId == null || outId.length < 1) {
      return SqliteRc.SQLITE_MISUSE;
    }
    int v = validateTodoTexts(title, description, status, priority);
    if (v != SqliteRc.SQLITE_OK) {
      return v;
    }
    SQLiteDatabase db = db();
    String sql =
        "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)"
            + " VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));";
    try {
      SQLiteStatement st = db.compileStatement(sql);
      st.bindString(1, title);
      st.bindString(2, description);
      st.bindString(3, status);
      st.bindString(4, priority);
      if (categoryIdSet && categoryId > 0) {
        st.bindLong(5, categoryId);
      } else {
        st.bindNull(5);
      }
      if (parentIdSet && parentId > 0) {
        st.bindLong(6, parentId);
      } else {
        st.bindNull(6);
      }
      if (dueDateSet) {
        st.bindLong(7, dueDate);
      } else {
        st.bindNull(7);
      }
      long row = st.executeInsert();
      st.close();
      outId[0] = row;
      if (outId[0] <= 0) {
        outId[0] = DatabaseUtils.longForQuery(db, "SELECT last_insert_rowid()", null);
      }
      return outId[0] > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      return SqliteRc.SQLITE_ERROR;
    }
  }

  public int storeTodoUpdateFull(
      long id,
      String title,
      String description,
      String status,
      String priority,
      boolean categoryIdSet,
      long categoryId,
      boolean parentIdSet,
      long parentId,
      boolean dueDateSet,
      long dueDate,
      int position) {
    int v = validateTodoTexts(title, description, status, priority);
    if (v != SqliteRc.SQLITE_OK) {
      return v;
    }
    SQLiteDatabase db = db();
    String sql =
        "UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?"
            + ", due_date=?, position=?, updated_at=CAST(strftime('%s','now') AS INTEGER) WHERE id=?;";
    try {
      SQLiteStatement st = db.compileStatement(sql);
      st.bindString(1, title);
      st.bindString(2, description);
      st.bindString(3, status);
      st.bindString(4, priority);
      if (categoryIdSet && categoryId > 0) {
        st.bindLong(5, categoryId);
      } else {
        st.bindNull(5);
      }
      if (parentIdSet && parentId > 0) {
        st.bindLong(6, parentId);
      } else {
        st.bindNull(6);
      }
      if (dueDateSet) {
        st.bindLong(7, dueDate);
      } else {
        st.bindNull(7);
      }
      st.bindLong(8, position);
      st.bindLong(9, id);
      int n = st.executeUpdateDelete();
      st.close();
      return n > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      return SqliteRc.SQLITE_ERROR;
    }
  }

  public int storeTodoDeleteById(long id) {
    SQLiteDatabase db = db();
    try {
      SQLiteStatement st = db.compileStatement("DELETE FROM todos WHERE id = ?;");
      st.bindLong(1, id);
      int n = st.executeUpdateDelete();
      st.close();
      return n > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      return SqliteRc.SQLITE_ERROR;
    }
  }

  private static final class TodoOpenHelper extends SQLiteOpenHelper {

    TodoOpenHelper(Context ctx) {
      super(ctx.getApplicationContext(), "todo_app.db", null, DB_VERSION);
    }

    @Override
    public void onConfigure(SQLiteDatabase db) {
      db.setForeignKeyConstraintsEnabled(true);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
      execSql(db, SCHEMA_DDL_V3);
      execSql(db, "PRAGMA user_version = 3;");
    }

    @Override
    @SuppressWarnings("unused")
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
      if (oldVersion == 2) {
        execSql(db, MIGRATE_V2_TO_V3);
        return;
      }
      if (oldVersion == 1) {
        execSql(db, "DROP TABLE IF EXISTS todos;");
      }
      if (oldVersion < 3) {
        execSql(db, SCHEMA_DDL_V3);
        execSql(db, "PRAGMA user_version = 3;");
      }
    }
  }
}
