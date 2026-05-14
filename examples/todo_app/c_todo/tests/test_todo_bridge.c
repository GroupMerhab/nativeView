/*
 * Unit tests for todo_bridge (no window / no nv_send).
 */

#include "todo_bridge.h"
#include "todo_store.h"

#include "nv_arena.h"
#include "nv_json.h"

#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
static char g_last_event[128];
static char g_last_json[16384];

static void fail(const char* m) {
  fprintf(stderr, "test_todo_bridge: %s\n", m);
  g_fail = 1;
}

static void capture(void* u, const char* event, const char* json) {
  (void)u;
  memset(g_last_event, 0, sizeof(g_last_event));
  memset(g_last_json, 0, sizeof(g_last_json));
  if (event) snprintf(g_last_event, sizeof(g_last_event), "%s", event);
  if (json) snprintf(g_last_json, sizeof(g_last_json), "%s", json);
}

static int count_json_array_elems(nv_arena_t* ar, const char* arr_json) {
  nv_json_val_t* v = nv_json_parse(ar, arr_json);
  if (!v || !nv_json_is_array(v)) return -1;
  return (int)nv_json_array_len(v);
}

int main(void) {
  g_fail = 0;
  nv_arena_t* ar = nv_arena_create(65536);
  if (!ar) {
    fail("arena");
    return 1;
  }
  todo_store_t* s = todo_store_open(NULL);
  if (!s) {
    fail("store");
    nv_arena_destroy(ar);
    return 1;
  }

  const char* wire_list =
    "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"categories.list\"}}";
  nv_arena_reset(ar);
  if (todo_bridge_handle_wire(s, ar, wire_list, capture, NULL) != 0) fail("categories.list rc");
  if (strcmp(g_last_event, "categories.list") != 0) fail("event categories.list");
  if (strcmp(g_last_json, "[]") != 0) fail("empty categories json");

  const char* wire_mk =
    "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"categories.create\",\"payload\":{\"name\":\"A\","
    "\"color\":\"#112233\"}}}";
  nv_arena_reset(ar);
  if (todo_bridge_handle_wire(s, ar, wire_mk, capture, NULL) != 0) fail("categories.create rc");
  if (strcmp(g_last_event, "categories.list") != 0) fail("event after create");
  if (count_json_array_elems(ar, g_last_json) != 1) fail("one category");

  const char* wire_todo =
    "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.create\",\"payload\":{"
    "\"title\":\"T1\",\"description\":\"\",\"status\":\"pending\",\"priority\":\"medium\","
    "\"categoryId\":1,\"parentId\":0,\"dueDate\":0}}}";
  nv_arena_reset(ar);
  if (todo_bridge_handle_wire(s, ar, wire_todo, capture, NULL) != 0) fail("todos.create rc");
  if (strcmp(g_last_event, "todos.list") != 0) fail("event todos.list");
  if (count_json_array_elems(ar, g_last_json) != 1) fail("one todo");

  nv_arena_reset(ar);
  nv_json_val_t* arr = nv_json_parse(ar, g_last_json);
  if (!arr || !nv_json_is_array(arr) || nv_json_array_len(arr) < 1) fail("parse todos");
  nv_json_val_t* row = nv_json_array_get(arr, 0);
  long long tid = nv_json_get_int(row, "id");
  char delbuf[256];
  snprintf(delbuf, sizeof(delbuf),
    "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.delete\",\"payload\":{\"id\":%lld}}}", tid);
  nv_arena_reset(ar);
  if (todo_bridge_handle_wire(s, ar, delbuf, capture, NULL) != 0) fail("delete rc");
  nv_arena_reset(ar);
  if (count_json_array_elems(ar, g_last_json) != 0) fail("zero todos after delete");

  todo_store_close(s);
  nv_arena_destroy(ar);
  return g_fail ? 1 : 0;
}
