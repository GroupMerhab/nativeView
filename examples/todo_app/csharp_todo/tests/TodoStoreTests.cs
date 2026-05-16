// Port of c_todo/tests/test_todo_store.c / nim_todo/tests/test_todo_store.nim
// SPDX-License-Identifier: Apache-2.0

using Jamharah.Todo;
using Xunit;

namespace Jamharah.Todo.Tests;

public sealed class TodoStoreTests
{
    [Fact]
    public void Store_crud_and_validation()
    {
        var s = TodoStoreApi.StoreOpen("");
        Assert.NotNull(s);
        try
        {
            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreCategoryInsert(s, "Work", "#ff00aa", out var cat1));
            Assert.True(cat1 > 0);

            Assert.NotEqual(SqliteRc.Ok, TodoStoreApi.StoreCategoryInsert(s, "Work", "#000000", out _));

            var catCount = 0;
            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreCategoriesForeach(s, _ => catCount++));
            Assert.Equal(1, catCount);

            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreTodoInsert(
                s, "Task one", "", "pending", "high", true, cat1, false, 0, false, 0, out var tid));
            Assert.True(tid > 0);

            var todoCount = 0;
            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreTodosForeach(s, _ => todoCount++));
            Assert.Equal(1, todoCount);

            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreTodoUpdateFull(
                s, tid, "Task one", "desc", "in_progress", "low", false, 0, false, 0, false, 0, 0));
            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreTodoUpdateFull(
                s, tid, "Task one", "desc", "archived", "low", false, 0, false, 0, false, 0, 0));

            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreTodoDeleteById(s, tid));
            todoCount = 0;
            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreTodosForeach(s, _ => todoCount++));
            Assert.Equal(0, todoCount);

            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreCategoryDeleteById(s, cat1));
            catCount = 0;
            Assert.Equal(SqliteRc.Ok, TodoStoreApi.StoreCategoriesForeach(s, _ => catCount++));
            Assert.Equal(0, catCount);

            Assert.NotEqual(SqliteRc.Ok, TodoStoreApi.StoreCategoryInsert(s, "", "#fff", out _));
            Assert.NotEqual(SqliteRc.Ok, TodoStoreApi.StoreTodoInsert(
                s, "", "x", "pending", "medium", false, 0, false, 0, false, 0, out _));
        }
        finally
        {
            TodoStoreApi.StoreClose(s);
        }
    }
}
