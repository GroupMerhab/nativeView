// IPC dispatch + JSON (port of c_todo/src/todo_bridge.c / nim_todo/todo_bridge.nim).
// SPDX-License-Identifier: Apache-2.0

using System.Text.Json;

namespace Jamharah.Todo;

public static class TodoBridge
{
    private static void EmitErr(Action<string, string> emit, int code, string msg)
    {
        var o = JsonSerializer.Serialize(new { code, message = msg });
        emit("error", o);
    }

    private static string JStr(JsonElement? pl, string k)
    {
        if (pl is not { } root || root.ValueKind != JsonValueKind.Object)
            return "";
        if (!root.TryGetProperty(k, out var v) || v.ValueKind != JsonValueKind.String)
            return "";
        return v.GetString() ?? "";
    }

    private static string JStrOr(JsonElement? pl, string k, string defaultValue)
    {
        var s = JStr(pl, k);
        return s.Length > 0 ? s : defaultValue;
    }

    private static long JInt64(JsonElement? pl, string k)
    {
        if (pl is not { } root || root.ValueKind != JsonValueKind.Object)
            return 0;
        if (!root.TryGetProperty(k, out var v))
            return 0;
        return v.ValueKind switch
        {
            JsonValueKind.Number => v.TryGetInt64(out var n) ? n : 0,
            JsonValueKind.True => 1,
            JsonValueKind.False => 0,
            _ => 0,
        };
    }

    private static object CategoryRowJson(TodoStoreCategoryRow r) =>
        new { id = r.Id, name = r.Name, color = r.Color };

    private static object TodoRowJson(TodoStoreTodoRow r)
    {
        return new
        {
            id = r.Id,
            title = r.Title,
            description = r.Description,
            status = r.Status,
            priority = r.Priority,
            categoryId = r.CategoryIdIsNull ? null : (object?)r.CategoryId,
            parentId = r.ParentIdIsNull ? null : (object?)r.ParentId,
            dueDate = r.DueDateIsNull ? null : (object?)r.DueDate,
            position = r.Position,
            createdAt = r.CreatedAt,
            updatedAt = r.UpdatedAt,
        };
    }

    private static int EmitCategoriesList(TodoStore? s, Action<string, string> emit)
    {
        var rows = new List<TodoStoreCategoryRow>();
        var rc = TodoStoreApi.StoreCategoriesForeach(s, rows.Add);
        if (rc != SqliteRc.Ok)
        {
            EmitErr(emit, rc, "categories.foreach failed");
            return -1;
        }
        emit("categories.list", JsonSerializer.Serialize(rows.Select(CategoryRowJson)));
        return 0;
    }

    private static int EmitTodosList(TodoStore? s, Action<string, string> emit)
    {
        var rows = new List<TodoStoreTodoRow>();
        var rc = TodoStoreApi.StoreTodosForeach(s, rows.Add);
        if (rc != SqliteRc.Ok)
        {
            EmitErr(emit, rc, "todos.foreach failed");
            return -1;
        }
        emit("todos.list", JsonSerializer.Serialize(rows.Select(TodoRowJson)));
        return 0;
    }

    public static int BridgeHandleWire(TodoStore? s, string rawIpcJson, Action<string, string> emit)
    {
        if (s is null || string.IsNullOrEmpty(rawIpcJson))
            return -1;

        JsonDocument doc;
        try
        {
            doc = JsonDocument.Parse(rawIpcJson);
        }
        catch (JsonException)
        {
            EmitErr(emit, -1, "invalid wire json");
            return -1;
        }

        using (doc)
        {
            var root = doc.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
            {
                EmitErr(emit, -1, "invalid wire json");
                return -1;
            }

            if (!root.TryGetProperty("d", out var d) || d.ValueKind != JsonValueKind.Object)
            {
                EmitErr(emit, -1, "missing wire.d object");
                return -1;
            }

            var action = d.TryGetProperty("action", out var actEl) && actEl.ValueKind == JsonValueKind.String
                ? actEl.GetString() ?? ""
                : "";
            if (action.Length == 0)
            {
                EmitErr(emit, -1, "missing action");
                return -1;
            }

            JsonElement? pld = d.TryGetProperty("payload", out var pl) && pl.ValueKind == JsonValueKind.Object
                ? pl
                : null;

            return action switch
            {
                "categories.list" => EmitCategoriesList(s, emit),
                "todos.list" => EmitTodosList(s, emit),
                "categories.create" => HandleCategoryCreate(s, pld, emit),
                "categories.delete" => HandleCategoryDelete(s, pld, emit),
                "todos.create" => HandleTodoCreate(s, pld, emit),
                "todos.update" => HandleTodoUpdate(s, pld, emit),
                "todos.delete" => HandleTodoDelete(s, pld, emit),
                _ => UnknownAction(emit),
            };
        }
    }

    private static int UnknownAction(Action<string, string> emit)
    {
        EmitErr(emit, -1, "unknown action");
        return -1;
    }

    private static int HandleCategoryCreate(TodoStore? s, JsonElement? pld, Action<string, string> emit)
    {
        if (pld is null)
        {
            EmitErr(emit, SqliteRc.Misuse, "missing payload");
            return -1;
        }
        var name = JStr(pld, "name");
        var color = JStrOr(pld, "color", "#6366f1");
        var rc = TodoStoreApi.StoreCategoryInsert(s, name, color, out _);
        if (rc != SqliteRc.Ok)
        {
            EmitErr(emit, rc, "category insert failed");
            return -1;
        }
        return EmitCategoriesList(s, emit);
    }

    private static int HandleCategoryDelete(TodoStore? s, JsonElement? pld, Action<string, string> emit)
    {
        if (pld is null)
        {
            EmitErr(emit, SqliteRc.Misuse, "missing payload");
            return -1;
        }
        var id = JInt64(pld, "id");
        var rc = TodoStoreApi.StoreCategoryDeleteById(s, id);
        if (rc != SqliteRc.Ok)
        {
            EmitErr(emit, rc, "category delete failed");
            return -1;
        }
        return EmitCategoriesList(s, emit);
    }

    private static int HandleTodoCreate(TodoStore? s, JsonElement? pld, Action<string, string> emit)
    {
        if (pld is null)
        {
            EmitErr(emit, SqliteRc.Misuse, "missing payload");
            return -1;
        }
        var title = JStr(pld, "title");
        var desc = JStrOr(pld, "description", "");
        var status = JStrOr(pld, "status", "pending");
        var pri = JStrOr(pld, "priority", "medium");
        var cat = JInt64(pld, "categoryId");
        var par = JInt64(pld, "parentId");
        var due = JInt64(pld, "dueDate");
        var rc = TodoStoreApi.StoreTodoInsert(
            s, title, desc, status, pri, cat > 0, cat, par > 0, par, due != 0, due, out _);
        if (rc != SqliteRc.Ok)
        {
            EmitErr(emit, rc, "todo insert failed");
            return -1;
        }
        return EmitTodosList(s, emit);
    }

    private static int HandleTodoUpdate(TodoStore? s, JsonElement? pld, Action<string, string> emit)
    {
        if (pld is null)
        {
            EmitErr(emit, SqliteRc.Misuse, "missing payload");
            return -1;
        }
        var id = JInt64(pld, "id");
        var title = JStr(pld, "title");
        var desc = JStrOr(pld, "description", "");
        var status = JStrOr(pld, "status", "pending");
        var pri = JStrOr(pld, "priority", "medium");
        var cat = JInt64(pld, "categoryId");
        var par = JInt64(pld, "parentId");
        var due = JInt64(pld, "dueDate");
        var pos = (int)JInt64(pld, "position");
        var rc = TodoStoreApi.StoreTodoUpdateFull(
            s, id, title, desc, status, pri, cat > 0, cat, par > 0, par, due != 0, due, pos);
        if (rc != SqliteRc.Ok)
        {
            EmitErr(emit, rc, "todo update failed");
            return -1;
        }
        return EmitTodosList(s, emit);
    }

    private static int HandleTodoDelete(TodoStore? s, JsonElement? pld, Action<string, string> emit)
    {
        if (pld is null)
        {
            EmitErr(emit, SqliteRc.Misuse, "missing payload");
            return -1;
        }
        var id = JInt64(pld, "id");
        var rc = TodoStoreApi.StoreTodoDeleteById(s, id);
        if (rc != SqliteRc.Ok)
        {
            EmitErr(emit, rc, "todo delete failed");
            return -1;
        }
        return EmitTodosList(s, emit);
    }
}
