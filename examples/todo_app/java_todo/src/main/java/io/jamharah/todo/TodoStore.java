/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.function.Consumer;

/**
 * SQLite persistence for todo_app (port of {@code nim_todo/todo_store.nim} / {@code
 * rust_todo/src/todo_store.rs}).
 */
public final class TodoStore implements AutoCloseable {

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
          + "  created_at INTEGER NOT NULL DEFAULT (unixepoch()),\n"
          + "  updated_at INTEGER NOT NULL DEFAULT (unixepoch())\n"
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
          + "  created_at INTEGER NOT NULL DEFAULT (unixepoch()),\n"
          + "  updated_at INTEGER NOT NULL DEFAULT (unixepoch())\n"
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

  private final Connection conn;

  private TodoStore(Connection conn) {
    this.conn = conn;
  }

  private static int mapErr(SQLException e) {
    int c = e.getErrorCode();
    return c != 0 ? c : SqliteRc.SQLITE_ERROR;
  }

  private static int execSql(Connection db, String sql) {
    for (String part : sql.split(";")) {
      String p = part.trim();
      if (p.isEmpty()) {
        continue;
      }
      try (Statement st = db.createStatement()) {
        st.execute(p);
      } catch (SQLException e) {
        System.err.println("[todo_store] " + e.getMessage());
        return mapErr(e);
      }
    }
    return SqliteRc.SQLITE_OK;
  }

  private static int readUserVersion(Connection db) {
    try (Statement st = db.createStatement();
        ResultSet rs = st.executeQuery("PRAGMA user_version;")) {
      if (rs.next()) {
        return rs.getInt(1);
      }
    } catch (SQLException ignored) {
      // fall through
    }
    return -1;
  }

  private static int ensureSchema(Connection db) throws SQLException {
    int ver = readUserVersion(db);
    if (ver < 0) {
      return SqliteRc.SQLITE_ERROR;
    }
    if (execSql(db, "PRAGMA foreign_keys = ON;") != SqliteRc.SQLITE_OK) {
      return SqliteRc.SQLITE_ERROR;
    }
    execSql(db, "PRAGMA journal_mode=WAL;");
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

  /** Opens the store; empty {@code path} uses in-memory SQLite. */
  public static TodoStore storeOpen(String path) {
    String uri = path == null || path.isEmpty() ? ":memory:" : path;
    String url = "jdbc:sqlite:" + uri;
    try {
      Connection c = DriverManager.getConnection(url);
      try (Statement st = c.createStatement()) {
        st.execute("PRAGMA busy_timeout = 5000");
      }
      if (ensureSchema(c) != SqliteRc.SQLITE_OK) {
        c.close();
        return null;
      }
      return new TodoStore(c);
    } catch (SQLException e) {
      System.err.println("[todo_store] open failed: " + e.getMessage());
      return null;
    }
  }

  public static void storeClose(TodoStore s) {
    if (s != null) {
      s.close();
    }
  }

  @Override
  public void close() {
    try {
      conn.close();
    } catch (SQLException ignored) {
      // ignore
    }
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
    String q = "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;";
    try (PreparedStatement ps = conn.prepareStatement(q);
        ResultSet rs = ps.executeQuery()) {
      while (rs.next()) {
        fn.accept(
            new TodoStoreCategoryRow(rs.getLong(1), rs.getString(2), rs.getString(3)));
      }
      return SqliteRc.SQLITE_OK;
    } catch (SQLException e) {
      return mapErr(e);
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
    String q = "INSERT INTO categories(name, color) VALUES(?, ?);";
    try (PreparedStatement ps = conn.prepareStatement(q, Statement.RETURN_GENERATED_KEYS)) {
      ps.setString(1, name);
      ps.setString(2, color);
      ps.executeUpdate();
      try (ResultSet keys = ps.getGeneratedKeys()) {
        if (keys.next()) {
          outId[0] = keys.getLong(1);
        }
      }
      if (outId[0] <= 0) {
        return SqliteRc.SQLITE_ERROR;
      }
      return SqliteRc.SQLITE_OK;
    } catch (SQLException e) {
      return mapErr(e);
    }
  }

  public int storeCategoryDeleteById(long id) {
    String q = "DELETE FROM categories WHERE id = ?;";
    try (PreparedStatement ps = conn.prepareStatement(q)) {
      ps.setLong(1, id);
      return ps.executeUpdate() > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      return mapErr(e);
    }
  }

  public int storeTodosForeach(Consumer<TodoStoreTodoRow> fn) {
    if (fn == null) {
      return SqliteRc.SQLITE_MISUSE;
    }
    String q =
        "SELECT id, title, description, status, priority, category_id, parent_id, due_date,"
            + " position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;";
    try (PreparedStatement ps = conn.prepareStatement(q);
        ResultSet rs = ps.executeQuery()) {
      while (rs.next()) {
        long cat = rs.getLong(6);
        boolean catNull = rs.wasNull();
        long par = rs.getLong(7);
        boolean parNull = rs.wasNull();
        long due = rs.getLong(8);
        boolean dueNull = rs.wasNull();
        fn.accept(
            new TodoStoreTodoRow(
                rs.getLong(1),
                rs.getString(2),
                rs.getString(3),
                rs.getString(4),
                rs.getString(5),
                catNull,
                cat,
                parNull,
                par,
                dueNull,
                due,
                rs.getInt(9),
                rs.getLong(10),
                rs.getLong(11)));
      }
      return SqliteRc.SQLITE_OK;
    } catch (SQLException e) {
      return mapErr(e);
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
    String q =
        "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)"
            + " VALUES(?,?,?,?,?,?,?, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));";
    try (PreparedStatement ps = conn.prepareStatement(q, Statement.RETURN_GENERATED_KEYS)) {
      ps.setString(1, title);
      ps.setString(2, description);
      ps.setString(3, status);
      ps.setString(4, priority);
      if (categoryIdSet && categoryId > 0) {
        ps.setLong(5, categoryId);
      } else {
        ps.setNull(5, java.sql.Types.INTEGER);
      }
      if (parentIdSet && parentId > 0) {
        ps.setLong(6, parentId);
      } else {
        ps.setNull(6, java.sql.Types.INTEGER);
      }
      if (dueDateSet) {
        ps.setLong(7, dueDate);
      } else {
        ps.setNull(7, java.sql.Types.INTEGER);
      }
      ps.executeUpdate();
      outId[0] = 0;
      try (ResultSet keys = ps.getGeneratedKeys()) {
        if (keys.next()) {
          outId[0] = keys.getLong(1);
        }
      }
      if (outId[0] <= 0) {
        try (Statement st = conn.createStatement();
            ResultSet rs = st.executeQuery("SELECT last_insert_rowid();")) {
          if (rs.next()) {
            outId[0] = rs.getLong(1);
          }
        }
      }
      if (outId[0] <= 0) {
        return SqliteRc.SQLITE_ERROR;
      }
      return SqliteRc.SQLITE_OK;
    } catch (SQLException e) {
      return mapErr(e);
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
    String q =
        "UPDATE todos SET title=?, description=?, status=?, priority=?, category_id=?, parent_id=?"
            + ", due_date=?, position=?, updated_at=unixepoch() WHERE id=?;";
    try (PreparedStatement ps = conn.prepareStatement(q)) {
      ps.setString(1, title);
      ps.setString(2, description);
      ps.setString(3, status);
      ps.setString(4, priority);
      if (categoryIdSet && categoryId > 0) {
        ps.setLong(5, categoryId);
      } else {
        ps.setNull(5, java.sql.Types.INTEGER);
      }
      if (parentIdSet && parentId > 0) {
        ps.setLong(6, parentId);
      } else {
        ps.setNull(6, java.sql.Types.INTEGER);
      }
      if (dueDateSet) {
        ps.setLong(7, dueDate);
      } else {
        ps.setNull(7, java.sql.Types.INTEGER);
      }
      ps.setInt(8, position);
      ps.setLong(9, id);
      return ps.executeUpdate() > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      return mapErr(e);
    }
  }

  public int storeTodoDeleteById(long id) {
    String q = "DELETE FROM todos WHERE id = ?;";
    try (PreparedStatement ps = conn.prepareStatement(q)) {
      ps.setLong(1, id);
      return ps.executeUpdate() > 0 ? SqliteRc.SQLITE_OK : SqliteRc.SQLITE_ERROR;
    } catch (SQLException e) {
      return mapErr(e);
    }
  }
}
