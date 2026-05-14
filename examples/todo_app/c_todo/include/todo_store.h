/*
 * todo_store — SQLite persistence (categories + todos) for todo_app.
 *
 * NULL path opens in-memory DB (tests). All pointer args NULL-safe where noted.
 * Return codes follow SQLite (SQLITE_OK, SQLITE_CONSTRAINT, SQLITE_MISUSE, …).
 */

#ifndef TODO_STORE_H
#define TODO_STORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct todo_store todo_store_t;

typedef struct {
  long long id;
  const char* name;
  const char* color;
} todo_store_category_row_t;

typedef struct {
  long long id;
  const char* title;
  const char* description;
  const char* status;
  const char* priority;
  int         category_id_is_null;
  long long   category_id;
  int         parent_id_is_null;
  long long   parent_id;
  int         due_date_is_null;
  long long   due_date;
  int         position;
  long long   created_at;
  long long   updated_at;
} todo_store_todo_row_t;

typedef void (*todo_store_category_cb)(void* userdata, const todo_store_category_row_t* row);
typedef void (*todo_store_todo_cb)(void* userdata, const todo_store_todo_row_t* row);

todo_store_t* todo_store_open(const char* path_utf8_or_null_memory);
void          todo_store_close(todo_store_t* store);

int todo_store_categories_foreach(todo_store_t* store, todo_store_category_cb cb, void* userdata);
int todo_store_category_insert(todo_store_t* store, const char* name, const char* color,
                               long long* out_id);
int todo_store_category_delete_by_id(todo_store_t* store, long long id);

int todo_store_todos_foreach(todo_store_t* store, todo_store_todo_cb cb, void* userdata);
int todo_store_todo_insert(todo_store_t* store, const char* title, const char* description,
                           const char* status, const char* priority, int category_id_set,
                           long long category_id, int parent_id_set, long long parent_id,
                           int due_date_set, long long due_date, long long* out_id);
int todo_store_todo_update_full(todo_store_t* store, long long id, const char* title,
                                 const char* description, const char* status, const char* priority,
                                 int category_id_set, long long category_id, int parent_id_set,
                                 long long parent_id, int due_date_set, long long due_date,
                                 int position);
int todo_store_todo_delete_by_id(todo_store_t* store, long long id);

#ifdef __cplusplus
}
#endif

#endif /* TODO_STORE_H */
