/* SPDX-License-Identifier: Apache-2.0 */
package io.jamharah.todo;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.Test;

class TodoStoreTest {

  @Test
  void todoStoreLifecycle() {
    TodoStore s = TodoStore.storeOpen("");
    assertTrue(s != null, "todo_store_open(\"\")");

    long[] cat1 = new long[1];
    assertEquals(SqliteRc.SQLITE_OK, s.storeCategoryInsert("Work", "#ff00aa", cat1));
    assertTrue(cat1[0] > 0, "category id");

    long[] dup = new long[1];
    assertNotEquals(
        SqliteRc.SQLITE_OK,
        s.storeCategoryInsert("Work", "#000000", dup),
        "duplicate category name");

    AtomicInteger catCount = new AtomicInteger();
    assertEquals(SqliteRc.SQLITE_OK, s.storeCategoriesForeach(r -> catCount.incrementAndGet()));
    assertEquals(1, catCount.get(), "category count");

    long[] tid = new long[1];
    assertEquals(
        SqliteRc.SQLITE_OK,
        s.storeTodoInsert(
            "Task one",
            "",
            "pending",
            "high",
            true,
            cat1[0],
            false,
            0,
            false,
            0,
            tid));
    assertTrue(tid[0] > 0, "todo id");

    AtomicInteger todoCount = new AtomicInteger();
    assertEquals(SqliteRc.SQLITE_OK, s.storeTodosForeach(r -> todoCount.incrementAndGet()));
    assertEquals(1, todoCount.get(), "todo foreach count");

    assertEquals(
        SqliteRc.SQLITE_OK,
        s.storeTodoUpdateFull(
            tid[0], "Task one", "desc", "in_progress", "low", false, 0, false, 0, false, 0, 0));

    assertEquals(
        SqliteRc.SQLITE_OK,
        s.storeTodoUpdateFull(
            tid[0], "Task one", "desc", "archived", "low", false, 0, false, 0, false, 0, 0));

    assertEquals(SqliteRc.SQLITE_OK, s.storeTodoDeleteById(tid[0]));
    todoCount.set(0);
    assertEquals(SqliteRc.SQLITE_OK, s.storeTodosForeach(r -> todoCount.incrementAndGet()));
    assertEquals(0, todoCount.get(), "todo count after delete");

    assertEquals(SqliteRc.SQLITE_OK, s.storeCategoryDeleteById(cat1[0]));
    catCount.set(0);
    assertEquals(SqliteRc.SQLITE_OK, s.storeCategoriesForeach(r -> catCount.incrementAndGet()));
    assertEquals(0, catCount.get(), "categories empty");

    long[] bad = new long[1];
    assertNotEquals(SqliteRc.SQLITE_OK, s.storeCategoryInsert("", "#fff", bad), "empty category name");
    assertNotEquals(
        SqliteRc.SQLITE_OK,
        s.storeTodoInsert("", "x", "pending", "medium", false, 0, false, 0, false, 0, tid),
        "empty title");

    TodoStore.storeClose(s);
  }
}
