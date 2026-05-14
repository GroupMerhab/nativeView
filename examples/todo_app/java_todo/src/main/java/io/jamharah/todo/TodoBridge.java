/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonNull;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import java.util.ArrayList;
import java.util.List;
import java.util.function.BiConsumer;

/**
 * IPC dispatch + JSON (port of {@code nim_todo/todo_bridge.nim} / {@code rust_todo/src/todo_bridge.rs}).
 */
public final class TodoBridge {

  private static final Gson GSON = new Gson();

  private TodoBridge() {}

  private static void emitErr(BiConsumer<String, String> emit, int code, String msg) {
    JsonObject o = new JsonObject();
    o.addProperty("code", code);
    o.addProperty("message", msg);
    emit.accept("error", GSON.toJson(o));
  }

  private static String jStr(JsonObject pl, String k) {
    if (pl == null || !pl.has(k)) {
      return "";
    }
    JsonElement e = pl.get(k);
    return e != null && e.isJsonPrimitive() && e.getAsJsonPrimitive().isString() ? e.getAsString() : "";
  }

  private static String jStrOr(JsonObject pl, String k, String def) {
    String v = jStr(pl, k);
    return !v.isEmpty() ? v : def;
  }

  private static long jInt64(JsonObject pl, String k) {
    if (pl == null || !pl.has(k)) {
      return 0;
    }
    JsonElement e = pl.get(k);
    if (e == null || e.isJsonNull()) {
      return 0;
    }
    try {
      if (e.isJsonPrimitive()) {
        if (e.getAsJsonPrimitive().isNumber()) {
          return e.getAsLong();
        }
        if (e.getAsJsonPrimitive().isString()) {
          String s = e.getAsString();
          if (s.isEmpty()) {
            return 0;
          }
          return Long.parseLong(s);
        }
      }
    } catch (NumberFormatException ignored) {
      return 0;
    }
    return 0;
  }

  private static JsonObject categoryRowJson(TodoStoreCategoryRow r) {
    JsonObject o = new JsonObject();
    o.addProperty("id", r.id);
    o.addProperty("name", r.name);
    o.addProperty("color", r.color);
    return o;
  }

  private static JsonObject todoRowJson(TodoStoreTodoRow r) {
    JsonObject o = new JsonObject();
    o.addProperty("id", r.id);
    o.addProperty("title", r.title);
    o.addProperty("description", r.description);
    o.addProperty("status", r.status);
    o.addProperty("priority", r.priority);
    if (r.categoryIdIsNull) {
      o.add("categoryId", JsonNull.INSTANCE);
    } else {
      o.addProperty("categoryId", r.categoryId);
    }
    if (r.parentIdIsNull) {
      o.add("parentId", JsonNull.INSTANCE);
    } else {
      o.addProperty("parentId", r.parentId);
    }
    if (r.dueDateIsNull) {
      o.add("dueDate", JsonNull.INSTANCE);
    } else {
      o.addProperty("dueDate", r.dueDate);
    }
    o.addProperty("position", r.position);
    o.addProperty("createdAt", r.createdAt);
    o.addProperty("updatedAt", r.updatedAt);
    return o;
  }

  private static int emitCategoriesList(TodoStore s, BiConsumer<String, String> emit) {
    List<TodoStoreCategoryRow> rows = new ArrayList<>();
    int rc = s.storeCategoriesForeach(rows::add);
    if (rc != SqliteRc.SQLITE_OK) {
      emitErr(emit, rc, "categories.foreach failed");
      return -1;
    }
    JsonArray arr = new JsonArray();
    for (TodoStoreCategoryRow r : rows) {
      arr.add(categoryRowJson(r));
    }
    emit.accept("categories.list", GSON.toJson(arr));
    return 0;
  }

  private static int emitTodosList(TodoStore s, BiConsumer<String, String> emit) {
    List<TodoStoreTodoRow> rows = new ArrayList<>();
    int rc = s.storeTodosForeach(rows::add);
    if (rc != SqliteRc.SQLITE_OK) {
      emitErr(emit, rc, "todos.foreach failed");
      return -1;
    }
    JsonArray arr = new JsonArray();
    for (TodoStoreTodoRow r : rows) {
      arr.add(todoRowJson(r));
    }
    emit.accept("todos.list", GSON.toJson(arr));
    return 0;
  }

  /** Handles one {@code __nv} wire JSON envelope. Returns {@code 0} on success, {@code -1} on failure. */
  public static int bridgeHandleWire(TodoStore s, String rawIpcJson, BiConsumer<String, String> emit) {
    if (s == null || emit == null || rawIpcJson == null || rawIpcJson.isEmpty()) {
      return -1;
    }
    JsonObject root;
    try {
      JsonElement el = com.google.gson.JsonParser.parseString(rawIpcJson);
      root = el != null && el.isJsonObject() ? el.getAsJsonObject() : null;
    } catch (JsonParseException e) {
      emitErr(emit, -1, "invalid wire json");
      return -1;
    }
    if (root == null) {
      emitErr(emit, -1, "invalid wire json");
      return -1;
    }
    if (!root.has("d") || !root.get("d").isJsonObject()) {
      emitErr(emit, -1, "missing wire.d object");
      return -1;
    }
    JsonObject d = root.getAsJsonObject("d");
    String action = "";
    if (d.has("action") && d.get("action").isJsonPrimitive()) {
      action = d.get("action").getAsString();
    }
    if (action.isEmpty()) {
      emitErr(emit, -1, "missing action");
      return -1;
    }
    JsonObject pl = null;
    if (d.has("payload") && d.get("payload").isJsonObject()) {
      pl = d.getAsJsonObject("payload");
    }

    if ("categories.list".equals(action)) {
      return emitCategoriesList(s, emit);
    }
    if ("todos.list".equals(action)) {
      return emitTodosList(s, emit);
    }
    if ("categories.create".equals(action)) {
      if (pl == null) {
        emitErr(emit, SqliteRc.SQLITE_MISUSE, "missing payload");
        return -1;
      }
      String name = jStr(pl, "name");
      String color = jStrOr(pl, "color", "#6366f1");
      long[] newId = new long[1];
      int rc = s.storeCategoryInsert(name, color, newId);
      if (rc != SqliteRc.SQLITE_OK) {
        emitErr(emit, rc, "category insert failed");
        return -1;
      }
      return emitCategoriesList(s, emit);
    }
    if ("categories.delete".equals(action)) {
      if (pl == null) {
        emitErr(emit, SqliteRc.SQLITE_MISUSE, "missing payload");
        return -1;
      }
      long id = jInt64(pl, "id");
      int rc = s.storeCategoryDeleteById(id);
      if (rc != SqliteRc.SQLITE_OK) {
        emitErr(emit, rc, "category delete failed");
        return -1;
      }
      return emitCategoriesList(s, emit);
    }
    if ("todos.create".equals(action)) {
      if (pl == null) {
        emitErr(emit, SqliteRc.SQLITE_MISUSE, "missing payload");
        return -1;
      }
      String title = jStr(pl, "title");
      String desc = jStrOr(pl, "description", "");
      String status = jStrOr(pl, "status", "pending");
      String pri = jStrOr(pl, "priority", "medium");
      long cat = jInt64(pl, "categoryId");
      long par = jInt64(pl, "parentId");
      long due = jInt64(pl, "dueDate");
      boolean catSet = cat > 0;
      boolean parSet = par > 0;
      boolean dueSet = due != 0;
      long[] newId = new long[1];
      int rc =
          s.storeTodoInsert(title, desc, status, pri, catSet, cat, parSet, par, dueSet, due, newId);
      if (rc != SqliteRc.SQLITE_OK) {
        emitErr(emit, rc, "todo insert failed");
        return -1;
      }
      return emitTodosList(s, emit);
    }
    if ("todos.update".equals(action)) {
      if (pl == null) {
        emitErr(emit, SqliteRc.SQLITE_MISUSE, "missing payload");
        return -1;
      }
      long id = jInt64(pl, "id");
      String title = jStr(pl, "title");
      String desc = jStrOr(pl, "description", "");
      String status = jStrOr(pl, "status", "pending");
      String pri = jStrOr(pl, "priority", "medium");
      long cat = jInt64(pl, "categoryId");
      long par = jInt64(pl, "parentId");
      long due = jInt64(pl, "dueDate");
      int pos = (int) jInt64(pl, "position");
      boolean catSet = cat > 0;
      boolean parSet = par > 0;
      boolean dueSet = due != 0;
      int rc =
          s.storeTodoUpdateFull(id, title, desc, status, pri, catSet, cat, parSet, par, dueSet, due, pos);
      if (rc != SqliteRc.SQLITE_OK) {
        emitErr(emit, rc, "todo update failed");
        return -1;
      }
      return emitTodosList(s, emit);
    }
    if ("todos.delete".equals(action)) {
      if (pl == null) {
        emitErr(emit, SqliteRc.SQLITE_MISUSE, "missing payload");
        return -1;
      }
      long id = jInt64(pl, "id");
      int rc = s.storeTodoDeleteById(id);
      if (rc != SqliteRc.SQLITE_OK) {
        emitErr(emit, rc, "todo delete failed");
        return -1;
      }
      return emitTodosList(s, emit);
    }
    emitErr(emit, -1, "unknown action");
    return -1;
  }
}
