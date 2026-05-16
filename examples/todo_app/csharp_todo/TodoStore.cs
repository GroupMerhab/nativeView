// SQLite persistence for todo_app (port of c_todo/src/todo_store.c / nim_todo/todo_store.nim).
// SPDX-License-Identifier: Apache-2.0

using Microsoft.Data.Sqlite;

namespace Jamharah.Todo;

public static class SqliteRc
{
    public const int Ok = 0;
    public const int Error = 1;
    public const int Misuse = 21;
    public const int Range = 25;
    public const int Constraint = 19;
}

public sealed record TodoStoreCategoryRow(long Id, string Name, string Color);

public sealed record TodoStoreTodoRow(
    long Id,
    string Title,
    string Description,
    string Status,
    string Priority,
    bool CategoryIdIsNull,
    long CategoryId,
    bool ParentIdIsNull,
    long ParentId,
    bool DueDateIsNull,
    long DueDate,
    int Position,
    long CreatedAt,
    long UpdatedAt);

public sealed class TodoStore
{
    internal SqliteConnection Conn { get; }

    internal TodoStore(SqliteConnection conn) => Conn = conn;
}

public static class TodoStoreApi
{
    private const int TitleMax = 255;
    private const int DescMax = 4096;
    private const int NameMax = 64;
    private const int ColorMax = 32;

    private const string SchemaDdlV3 = """
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
        """;

    private const string MigrateV2ToV3 = """
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
        """;

    public static void StoreClose(TodoStore? s)
    {
        if (s is null)
            return;
        s.Conn.Close();
        s.Conn.Dispose();
    }

    public static TodoStore? StoreOpen(string path)
    {
        var uri = string.IsNullOrEmpty(path) ? ":memory:" : path;
        try
        {
            var conn = new SqliteConnection($"Data Source={uri}");
            conn.Open();
            using (var cmd = conn.CreateCommand())
            {
                cmd.CommandText = "PRAGMA busy_timeout = 5000;";
                cmd.ExecuteNonQuery();
            }
            if (EnsureSchema(conn) != SqliteRc.Ok)
            {
                conn.Close();
                conn.Dispose();
                return null;
            }
            return new TodoStore(conn);
        }
        catch (SqliteException)
        {
            Console.Error.WriteLine("[todo_store] open failed");
            return null;
        }
    }

    private static int ExecSql(SqliteConnection conn, string sql)
    {
        try
        {
            using var cmd = conn.CreateCommand();
            cmd.CommandText = sql;
            cmd.ExecuteNonQuery();
            return SqliteRc.Ok;
        }
        catch (SqliteException e)
        {
            Console.Error.WriteLine($"[todo_store] {e.Message}");
            return SqliteRc.Error;
        }
    }

    private static int ReadUserVersion(SqliteConnection conn)
    {
        try
        {
            using var cmd = conn.CreateCommand();
            cmd.CommandText = "PRAGMA user_version;";
            var v = cmd.ExecuteScalar();
            return v is long l ? (int)l : v is int i ? i : -1;
        }
        catch (SqliteException)
        {
            return -1;
        }
    }

    private static int EnsureSchema(SqliteConnection conn)
    {
        var ver = ReadUserVersion(conn);
        if (ver < 0)
            return SqliteRc.Error;

        if (ExecSql(conn, "PRAGMA foreign_keys = ON;") != SqliteRc.Ok)
            return SqliteRc.Error;
        ExecSql(conn, "PRAGMA journal_mode=WAL;");

        if (ver == 1)
        {
            if (ExecSql(conn, "DROP TABLE IF EXISTS todos;") != SqliteRc.Ok)
                return SqliteRc.Error;
        }

        if (ver == 2)
        {
            try
            {
                using var cmd = conn.CreateCommand();
                cmd.CommandText = MigrateV2ToV3;
                cmd.ExecuteNonQuery();
            }
            catch (SqliteException e)
            {
                Console.Error.WriteLine($"[todo_store] {e.Message}");
                return SqliteRc.Error;
            }
            return SqliteRc.Ok;
        }

        if (ExecSql(conn, SchemaDdlV3) != SqliteRc.Ok)
            return SqliteRc.Error;

        ver = ReadUserVersion(conn);
        if (ver < 3 && ExecSql(conn, "PRAGMA user_version = 3;") != SqliteRc.Ok)
            return SqliteRc.Error;
        return SqliteRc.Ok;
    }

    private static int ValidateNameColor(string name, string color)
    {
        if (string.IsNullOrEmpty(name) || string.IsNullOrEmpty(color))
            return SqliteRc.Misuse;
        if (name.Length is < 1 or > NameMax || color.Length is < 1 or > ColorMax)
            return SqliteRc.Range;
        return SqliteRc.Ok;
    }

    private static int ValidateTodoTexts(string title, string description, string status, string priority)
    {
        if (string.IsNullOrEmpty(status) || string.IsNullOrEmpty(priority))
            return SqliteRc.Misuse;
        if (title.Length is < 1 or > TitleMax || description.Length > DescMax)
            return SqliteRc.Range;
        if (status is not ("pending" or "in_progress" or "done" or "cancelled" or "archived"))
            return SqliteRc.Constraint;
        if (priority is not ("low" or "medium" or "high" or "urgent"))
            return SqliteRc.Constraint;
        return SqliteRc.Ok;
    }

    public static int StoreCategoriesForeach(TodoStore? s, Action<TodoStoreCategoryRow> fn)
    {
        if (s is null)
            return SqliteRc.Misuse;
        const string q = "SELECT id, name, color FROM categories ORDER BY name COLLATE NOCASE ASC;";
        try
        {
            using var cmd = s.Conn.CreateCommand();
            cmd.CommandText = q;
            using var reader = cmd.ExecuteReader();
            while (reader.Read())
            {
                fn(new TodoStoreCategoryRow(
                    reader.GetInt64(0),
                    reader.IsDBNull(1) ? "" : reader.GetString(1),
                    reader.IsDBNull(2) ? "" : reader.GetString(2)));
            }
            return SqliteRc.Ok;
        }
        catch (SqliteException)
        {
            return SqliteRc.Error;
        }
    }

    public static int StoreCategoryInsert(TodoStore? s, string name, string color, out long outId)
    {
        outId = 0;
        if (s is null)
            return SqliteRc.Misuse;
        var v = ValidateNameColor(name, color);
        if (v != SqliteRc.Ok)
            return v;
        const string q = "INSERT INTO categories(name, color) VALUES($name, $color);";
        try
        {
            using var cmd = s.Conn.CreateCommand();
            cmd.CommandText = q;
            cmd.Parameters.AddWithValue("$name", name);
            cmd.Parameters.AddWithValue("$color", color);
            cmd.ExecuteNonQuery();
            using var idCmd = s.Conn.CreateCommand();
            idCmd.CommandText = "SELECT last_insert_rowid();";
            outId = (long)(idCmd.ExecuteScalar() ?? 0L);
            return SqliteRc.Ok;
        }
        catch (SqliteException ex) when (ex.SqliteErrorCode == 19)
        {
            return SqliteRc.Constraint;
        }
        catch (SqliteException)
        {
            return SqliteRc.Error;
        }
    }

    public static int StoreCategoryDeleteById(TodoStore? s, long id)
    {
        if (s is null)
            return SqliteRc.Misuse;
        const string q = "DELETE FROM categories WHERE id = $id;";
        try
        {
            using var cmd = s.Conn.CreateCommand();
            cmd.CommandText = q;
            cmd.Parameters.AddWithValue("$id", id);
            return cmd.ExecuteNonQuery() > 0 ? SqliteRc.Ok : SqliteRc.Error;
        }
        catch (SqliteException)
        {
            return SqliteRc.Error;
        }
    }

    public static int StoreTodosForeach(TodoStore? s, Action<TodoStoreTodoRow> fn)
    {
        if (s is null)
            return SqliteRc.Misuse;
        const string q =
            "SELECT id, title, description, status, priority, category_id, parent_id, due_date," +
            " position, created_at, updated_at FROM todos ORDER BY position ASC, id ASC;";
        try
        {
            using var cmd = s.Conn.CreateCommand();
            cmd.CommandText = q;
            using var reader = cmd.ExecuteReader();
            while (reader.Read())
            {
                var catNull = reader.IsDBNull(5);
                var parNull = reader.IsDBNull(6);
                var dueNull = reader.IsDBNull(7);
                fn(new TodoStoreTodoRow(
                    reader.GetInt64(0),
                    reader.IsDBNull(1) ? "" : reader.GetString(1),
                    reader.IsDBNull(2) ? "" : reader.GetString(2),
                    reader.IsDBNull(3) ? "" : reader.GetString(3),
                    reader.IsDBNull(4) ? "" : reader.GetString(4),
                    catNull,
                    catNull ? 0 : reader.GetInt64(5),
                    parNull,
                    parNull ? 0 : reader.GetInt64(6),
                    dueNull,
                    dueNull ? 0 : reader.GetInt64(7),
                    reader.IsDBNull(8) ? 0 : reader.GetInt32(8),
                    reader.IsDBNull(9) ? 0 : reader.GetInt64(9),
                    reader.IsDBNull(10) ? 0 : reader.GetInt64(10)));
            }
            return SqliteRc.Ok;
        }
        catch (SqliteException)
        {
            return SqliteRc.Error;
        }
    }

    public static int StoreTodoInsert(
        TodoStore? s,
        string title,
        string description,
        string status,
        string priority,
        bool categoryIdSet,
        long categoryId,
        bool parentIdSet,
        long parentId,
        bool dueDateSet,
        long dueDate,
        out long outId)
    {
        outId = 0;
        if (s is null)
            return SqliteRc.Misuse;
        var v = ValidateTodoTexts(title, description, status, priority);
        if (v != SqliteRc.Ok)
            return v;
        const string q =
            "INSERT INTO todos(title, description, status, priority, category_id, parent_id, due_date, position)" +
            " VALUES($t,$d,$st,$pr,$cat,$par,$due, (SELECT COALESCE(MAX(position), -1) + 1 FROM todos));";
        try
        {
            using var cmd = s.Conn.CreateCommand();
            cmd.CommandText = q;
            cmd.Parameters.AddWithValue("$t", title);
            cmd.Parameters.AddWithValue("$d", description);
            cmd.Parameters.AddWithValue("$st", status);
            cmd.Parameters.AddWithValue("$pr", priority);
            cmd.Parameters.AddWithValue("$cat", categoryIdSet && categoryId > 0 ? categoryId : DBNull.Value);
            cmd.Parameters.AddWithValue("$par", parentIdSet && parentId > 0 ? parentId : DBNull.Value);
            cmd.Parameters.AddWithValue("$due", dueDateSet ? dueDate : DBNull.Value);
            cmd.ExecuteNonQuery();
            using var idCmd = s.Conn.CreateCommand();
            idCmd.CommandText = "SELECT last_insert_rowid();";
            outId = (long)(idCmd.ExecuteScalar() ?? 0L);
            return SqliteRc.Ok;
        }
        catch (SqliteException)
        {
            return SqliteRc.Error;
        }
    }

    public static int StoreTodoUpdateFull(
        TodoStore? s,
        long id,
        string title,
        string description,
        string status,
        string priority,
        bool categoryIdSet,
        long categoryId,
        bool parentIdSet,
        long parentId,
        bool dueDateSet,
        long dueDate,
        int position)
    {
        if (s is null)
            return SqliteRc.Misuse;
        var v = ValidateTodoTexts(title, description, status, priority);
        if (v != SqliteRc.Ok)
            return v;
        const string q =
            "UPDATE todos SET title=$t, description=$d, status=$st, priority=$pr, category_id=$cat, parent_id=$par," +
            " due_date=$due, position=$pos, updated_at=unixepoch() WHERE id=$id;";
        try
        {
            using var cmd = s.Conn.CreateCommand();
            cmd.CommandText = q;
            cmd.Parameters.AddWithValue("$t", title);
            cmd.Parameters.AddWithValue("$d", description);
            cmd.Parameters.AddWithValue("$st", status);
            cmd.Parameters.AddWithValue("$pr", priority);
            cmd.Parameters.AddWithValue("$cat", categoryIdSet && categoryId > 0 ? categoryId : DBNull.Value);
            cmd.Parameters.AddWithValue("$par", parentIdSet && parentId > 0 ? parentId : DBNull.Value);
            cmd.Parameters.AddWithValue("$due", dueDateSet ? dueDate : DBNull.Value);
            cmd.Parameters.AddWithValue("$pos", position);
            cmd.Parameters.AddWithValue("$id", id);
            return cmd.ExecuteNonQuery() > 0 ? SqliteRc.Ok : SqliteRc.Error;
        }
        catch (SqliteException)
        {
            return SqliteRc.Error;
        }
    }

    public static int StoreTodoDeleteById(TodoStore? s, long id)
    {
        if (s is null)
            return SqliteRc.Misuse;
        const string q = "DELETE FROM todos WHERE id = $id;";
        try
        {
            using var cmd = s.Conn.CreateCommand();
            cmd.CommandText = q;
            cmd.Parameters.AddWithValue("$id", id);
            return cmd.ExecuteNonQuery() > 0 ? SqliteRc.Ok : SqliteRc.Error;
        }
        catch (SqliteException)
        {
            return SqliteRc.Error;
        }
    }
}
