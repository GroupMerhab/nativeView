/*
 * Unit tests for todo_store (in-memory SQLite, no UI).
 */

#include "todo_store.h"

#include "sqlite3.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void fail(const char* msg) {
  fprintf(stderr, "test_todo_store: %s\n", msg);
  g_fail = 1;
}

typedef struct {
  int n;
} count_ctx_t;

static void count_cat(void* u, const todo_store_category_row_t* row) {
  (void)row;
  ((count_ctx_t*)u)->n++;
}

typedef struct {
  int n;
} count_todo_ctx_t;

static void count_todo(void* u, const todo_store_todo_row_t* row) {
  (void)row;
  ((count_todo_ctx_t*)u)->n++;
}

int main(void) {
  g_fail = 0;

  todo_store_t* s = todo_store_open(NULL);
  if (!s) {
    fail("todo_store_open(NULL)");
    return 1;
  }

  long long cat1 = 0;
  if (todo_store_category_insert(s, "Work", "#ff00aa", &cat1) != SQLITE_OK) fail("category insert");
  if (cat1 <= 0) fail("category id");

  long long dup = 0;
  if (todo_store_category_insert(s, "Work", "#000000", &dup) == SQLITE_OK) fail("duplicate category name");

  count_ctx_t cc = { 0 };
  if (todo_store_categories_foreach(s, count_cat, &cc) != SQLITE_OK) fail("categories foreach");
  if (cc.n != 1) fail("category count");

  long long tid = 0;
  if (todo_store_todo_insert(s, "Task one", "", "pending", "high", 1, cat1, 0, 0, 0, 0, &tid) != SQLITE_OK)
    fail("todo insert");
  if (tid <= 0) fail("todo id");

  count_todo_ctx_t tc = { 0 };
  if (todo_store_todos_foreach(s, count_todo, &tc) != SQLITE_OK) fail("todos foreach");
  if (tc.n != 1) fail("todo foreach count");

  if (todo_store_todo_update_full(s, tid, "Task one", "desc", "in_progress", "low", 0, 0, 0, 0, 0, 0, 0) !=
      SQLITE_OK)
    fail("todo update");

  if (todo_store_todo_update_full(s, tid, "Task one", "desc", "archived", "low", 0, 0, 0, 0, 0, 0, 0) !=
      SQLITE_OK)
    fail("todo update archived");

  if (todo_store_todo_delete_by_id(s, tid) != SQLITE_OK) fail("todo delete");
  tc.n = 0;
  if (todo_store_todos_foreach(s, count_todo, &tc) != SQLITE_OK) fail("todos foreach 2");
  if (tc.n != 0) fail("todo count after delete");

  if (todo_store_category_delete_by_id(s, cat1) != SQLITE_OK) fail("category delete");
  cc.n = 0;
  if (todo_store_categories_foreach(s, count_cat, &cc) != SQLITE_OK) fail("cat foreach 2");
  if (cc.n != 0) fail("categories empty");

  long long bad = 0;
  if (todo_store_category_insert(s, "", "#fff", &bad) == SQLITE_OK) fail("empty category name");
  if (todo_store_todo_insert(s, "", "x", "pending", "medium", 0, 0, 0, 0, 0, 0, &tid) == SQLITE_OK)
    fail("empty title");

  todo_store_close(s);
  return g_fail ? 1 : 0;
}
