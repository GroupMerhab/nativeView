// Port of c_todo/tests/test_todo_store.c / nim_todo/tests/test_todo_store.nim

use rusqlite::ffi::SQLITE_OK;
use rust_todo::todo_store::{
    store_categories_foreach, store_category_delete_by_id, store_category_insert, store_close,
    store_open, store_todo_delete_by_id, store_todo_insert, store_todo_update_full,
    store_todos_foreach, TodoStoreCategoryRow, TodoStoreTodoRow,
};

#[test]
fn todo_store_lifecycle() {
    let s = store_open("").expect("todo_store_open(\"\")");

    let mut cat1 = 0i64;
    assert_eq!(store_category_insert(&s, "Work", "#ff00aa", &mut cat1), SQLITE_OK);
    assert!(cat1 > 0, "category id");

    let mut dup = 0i64;
    assert_ne!(
        store_category_insert(&s, "Work", "#000000", &mut dup),
        SQLITE_OK,
        "duplicate category name"
    );

    let mut cat_count = 0;
    assert_eq!(
        store_categories_foreach(&s, |_r: TodoStoreCategoryRow| {
            cat_count += 1;
        }),
        SQLITE_OK
    );
    assert_eq!(cat_count, 1, "category count");

    let mut tid = 0i64;
    assert_eq!(
        store_todo_insert(
            &s,
            "Task one",
            "",
            "pending",
            "high",
            true,
            cat1,
            false,
            0,
            false,
            0,
            &mut tid,
        ),
        SQLITE_OK
    );
    assert!(tid > 0, "todo id");

    let mut todo_count = 0;
    assert_eq!(
        store_todos_foreach(&s, |_r: TodoStoreTodoRow| {
            todo_count += 1;
        }),
        SQLITE_OK
    );
    assert_eq!(todo_count, 1, "todo foreach count");

    assert_eq!(
        store_todo_update_full(
            &s,
            tid,
            "Task one",
            "desc",
            "in_progress",
            "low",
            false,
            0,
            false,
            0,
            false,
            0,
            0,
        ),
        SQLITE_OK
    );

    assert_eq!(
        store_todo_update_full(
            &s,
            tid,
            "Task one",
            "desc",
            "archived",
            "low",
            false,
            0,
            false,
            0,
            false,
            0,
            0,
        ),
        SQLITE_OK
    );

    assert_eq!(store_todo_delete_by_id(&s, tid), SQLITE_OK);
    todo_count = 0;
    assert_eq!(
        store_todos_foreach(&s, |_r: TodoStoreTodoRow| {
            todo_count += 1;
        }),
        SQLITE_OK
    );
    assert_eq!(todo_count, 0, "todo count after delete");

    assert_eq!(store_category_delete_by_id(&s, cat1), SQLITE_OK);
    cat_count = 0;
    assert_eq!(
        store_categories_foreach(&s, |_r: TodoStoreCategoryRow| {
            cat_count += 1;
        }),
        SQLITE_OK
    );
    assert_eq!(cat_count, 0, "categories empty");

    let mut bad = 0i64;
    assert_ne!(
        store_category_insert(&s, "", "#fff", &mut bad),
        SQLITE_OK,
        "empty category name"
    );
    assert_ne!(
        store_todo_insert(
            &s,
            "",
            "x",
            "pending",
            "medium",
            false,
            0,
            false,
            0,
            false,
            0,
            &mut tid,
        ),
        SQLITE_OK,
        "empty title"
    );

    store_close(s);
}
