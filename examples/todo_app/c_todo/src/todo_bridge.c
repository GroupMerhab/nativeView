/*
 * todo_bridge.c — IPC dispatch + JSON compose (nv_json / nv_arena).
 */

#include "todo_bridge.h"

#include "nv_json.h"

#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  nv_arena_t* ar;
  nv_json_t*  arr;
} json_arr_ctx_t;

static void emit_err(todo_bridge_emit_fn emit, void* ud, nv_arena_t* ar, int code, const char* msg) {
  if (!emit) return;
  nv_json_t* o = nv_json_object(ar);
  if (!o) return;
  nv_json_int(o, "code", (long long)code);
  nv_json_str(o, "message", msg ? msg : "");
  const char* s = nv_json_serialize(o);
  if (s) emit(ud, "error", s);
}

static nv_json_t* category_row_json(nv_arena_t* ar, const todo_store_category_row_t* r) {
  nv_json_t* o = nv_json_object(ar);
  if (!o) return NULL;
  nv_json_int(o, "id", r->id);
  nv_json_str(o, "name", r->name ? r->name : "");
  nv_json_str(o, "color", r->color ? r->color : "");
  return o;
}

static nv_json_t* todo_row_json(nv_arena_t* ar, const todo_store_todo_row_t* r) {
  nv_json_t* o = nv_json_object(ar);
  if (!o) return NULL;
  nv_json_int(o, "id", r->id);
  nv_json_str(o, "title", r->title ? r->title : "");
  nv_json_str(o, "description", r->description ? r->description : "");
  nv_json_str(o, "status", r->status ? r->status : "");
  nv_json_str(o, "priority", r->priority ? r->priority : "");
  if (r->category_id_is_null) nv_json_null(o, "categoryId");
  else nv_json_int(o, "categoryId", r->category_id);
  if (r->parent_id_is_null) nv_json_null(o, "parentId");
  else nv_json_int(o, "parentId", r->parent_id);
  if (r->due_date_is_null) nv_json_null(o, "dueDate");
  else nv_json_int(o, "dueDate", r->due_date);
  nv_json_int(o, "position", (long long)r->position);
  nv_json_int(o, "createdAt", r->created_at);
  nv_json_int(o, "updatedAt", r->updated_at);
  return o;
}

static void cat_list_cb(void* u, const todo_store_category_row_t* row) {
  json_arr_ctx_t* c = (json_arr_ctx_t*)u;
  nv_json_t* item = category_row_json(c->ar, row);
  if (item) nv_json_array_push(c->arr, item);
}

static void todo_list_cb(void* u, const todo_store_todo_row_t* row) {
  json_arr_ctx_t* c = (json_arr_ctx_t*)u;
  nv_json_t* item = todo_row_json(c->ar, row);
  if (item) nv_json_array_push(c->arr, item);
}

static int emit_categories_list(todo_store_t* store, nv_arena_t* ar, todo_bridge_emit_fn emit,
                                  void* emit_ud) {
  nv_json_t* arr = nv_json_array(ar);
  if (!arr) return -1;
  json_arr_ctx_t ctx = { ar, arr };
  int rc = todo_store_categories_foreach(store, cat_list_cb, &ctx);
  if (rc != SQLITE_OK) {
    emit_err(emit, emit_ud, ar, rc, "categories.foreach failed");
    return -1;
  }
  const char* s = nv_json_serialize(arr);
  if (!s) return -1;
  emit(emit_ud, "categories.list", s);
  return 0;
}

static int emit_todos_list(todo_store_t* store, nv_arena_t* ar, todo_bridge_emit_fn emit,
                           void* emit_ud) {
  nv_json_t* arr = nv_json_array(ar);
  if (!arr) return -1;
  json_arr_ctx_t ctx = { ar, arr };
  int rc = todo_store_todos_foreach(store, todo_list_cb, &ctx);
  if (rc != SQLITE_OK) {
    emit_err(emit, emit_ud, ar, rc, "todos.foreach failed");
    return -1;
  }
  const char* s = nv_json_serialize(arr);
  if (!s) return -1;
  emit(emit_ud, "todos.list", s);
  return 0;
}

static const char* str_or(const nv_json_val_t* pl, const char* key, const char* def) {
  const char* v = pl ? nv_json_get_str(pl, key) : NULL;
  return v ? v : def;
}

int todo_bridge_handle_wire(todo_store_t* store, nv_arena_t* work_arena, const char* raw_ipc_json,
                            todo_bridge_emit_fn emit, void* emit_ud) {
  if (!store || !work_arena || !raw_ipc_json || !emit) return -1;

  nv_json_val_t* root = nv_json_parse(work_arena, raw_ipc_json);
  if (!root || !nv_json_is_object(root)) {
    emit_err(emit, emit_ud, work_arena, -1, "invalid wire json");
    return -1;
  }

  nv_json_val_t* d = nv_json_get_obj(root, "d");
  if (!d || !nv_json_is_object(d)) {
    emit_err(emit, emit_ud, work_arena, -1, "missing wire.d object");
    return -1;
  }

  const char* action = nv_json_get_str(d, "action");
  if (!action) {
    emit_err(emit, emit_ud, work_arena, -1, "missing action");
    return -1;
  }

  nv_json_val_t* pl = nv_json_get_obj(d, "payload");

  if (strcmp(action, "categories.list") == 0) return emit_categories_list(store, work_arena, emit, emit_ud);

  if (strcmp(action, "todos.list") == 0) return emit_todos_list(store, work_arena, emit, emit_ud);

  if (strcmp(action, "categories.create") == 0) {
    if (!pl) {
      emit_err(emit, emit_ud, work_arena, SQLITE_MISUSE, "missing payload");
      return -1;
    }
    const char* name = nv_json_get_str(pl, "name");
    const char* color = str_or(pl, "color", "#6366f1");
    long long id = 0;
    int rc = todo_store_category_insert(store, name, color, &id);
    if (rc != SQLITE_OK) {
      emit_err(emit, emit_ud, work_arena, rc, "category insert failed");
      return -1;
    }
    (void)id;
    if (emit_categories_list(store, work_arena, emit, emit_ud) != 0) return -1;
    return 0;
  }

  if (strcmp(action, "categories.delete") == 0) {
    if (!pl) {
      emit_err(emit, emit_ud, work_arena, SQLITE_MISUSE, "missing payload");
      return -1;
    }
    long long id = nv_json_get_int(pl, "id");
    int rc = todo_store_category_delete_by_id(store, id);
    if (rc != SQLITE_OK) {
      emit_err(emit, emit_ud, work_arena, rc, "category delete failed");
      return -1;
    }
    return emit_categories_list(store, work_arena, emit, emit_ud);
  }

  if (strcmp(action, "todos.create") == 0) {
    if (!pl) {
      emit_err(emit, emit_ud, work_arena, SQLITE_MISUSE, "missing payload");
      return -1;
    }
    const char* title = nv_json_get_str(pl, "title");
    const char* desc = str_or(pl, "description", "");
    const char* status = str_or(pl, "status", "pending");
    const char* pri = str_or(pl, "priority", "medium");
    long long cat = nv_json_get_int(pl, "categoryId");
    long long par = nv_json_get_int(pl, "parentId");
    long long due = nv_json_get_int(pl, "dueDate");
    int cat_set = cat > 0;
    int par_set = par > 0;
    int due_set = due != 0;
    long long new_id = 0;
    int rc = todo_store_todo_insert(store, title, desc, status, pri, cat_set, cat, par_set, par,
                                    due_set, due, &new_id);
    if (rc != SQLITE_OK) {
      emit_err(emit, emit_ud, work_arena, rc, "todo insert failed");
      return -1;
    }
    (void)new_id;
    return emit_todos_list(store, work_arena, emit, emit_ud);
  }

  if (strcmp(action, "todos.update") == 0) {
    if (!pl) {
      emit_err(emit, emit_ud, work_arena, SQLITE_MISUSE, "missing payload");
      return -1;
    }
    long long id = nv_json_get_int(pl, "id");
    const char* title = nv_json_get_str(pl, "title");
    const char* desc = str_or(pl, "description", "");
    const char* status = str_or(pl, "status", "pending");
    const char* pri = str_or(pl, "priority", "medium");
    long long cat = nv_json_get_int(pl, "categoryId");
    long long par = nv_json_get_int(pl, "parentId");
    long long due = nv_json_get_int(pl, "dueDate");
    int pos = (int)nv_json_get_int(pl, "position");
    int cat_set = cat > 0;
    int par_set = par > 0;
    int due_set = due != 0;
    int rc = todo_store_todo_update_full(store, id, title, desc, status, pri, cat_set, cat, par_set,
                                         par, due_set, due, pos);
    if (rc != SQLITE_OK) {
      emit_err(emit, emit_ud, work_arena, rc, "todo update failed");
      return -1;
    }
    return emit_todos_list(store, work_arena, emit, emit_ud);
  }

  if (strcmp(action, "todos.delete") == 0) {
    if (!pl) {
      emit_err(emit, emit_ud, work_arena, SQLITE_MISUSE, "missing payload");
      return -1;
    }
    long long id = nv_json_get_int(pl, "id");
    int rc = todo_store_todo_delete_by_id(store, id);
    if (rc != SQLITE_OK) {
      emit_err(emit, emit_ud, work_arena, rc, "todo delete failed");
      return -1;
    }
    return emit_todos_list(store, work_arena, emit, emit_ud);
  }

  emit_err(emit, emit_ud, work_arena, -1, "unknown action");
  return -1;
}
