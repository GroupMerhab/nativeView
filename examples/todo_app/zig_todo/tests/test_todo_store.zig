//! Port of nim_todo/tests/test_todo_store.nim
//! SPDX-License-Identifier: Apache-2.0

const std = @import("std");
const ts = @import("ts");

var g_fail: bool = false;

fn fail(comptime msg: []const u8) void {
    std.debug.print("test_todo_store: {s}\n", .{msg});
    g_fail = true;
}

fn mainInner() void {
    g_fail = false;
    const s = ts.storeOpen("") orelse {
        fail("todo_store_open(\"\")");
        return;
    };
    defer ts.storeClose(s);

    var cat1: i64 = 0;
    if (ts.storeCategoryInsert(s, "Work", "#ff00aa", &cat1) != ts.SQLITE_OK) fail("category insert");
    if (cat1 <= 0) fail("category id");

    var dup: i64 = 0;
    if (ts.storeCategoryInsert(s, "Work", "#000000", &dup) == ts.SQLITE_OK) fail("duplicate category name");

    var cat_count: usize = 0;
    const rc1 = ts.storeCategoriesForeach(s, &cat_count, struct {
        fn cb(ud: ?*anyopaque, _: ts.CategoryRow) void {
            const p: *usize = @ptrCast(@alignCast(ud.?));
            p.* += 1;
        }
    }.cb);
    if (rc1 != ts.SQLITE_OK) fail("categories foreach");
    if (cat_count != 1) fail("category count");

    var tid: i64 = 0;
    if (ts.storeTodoInsert(s, "Task one", "", "pending", "high", true, cat1, false, 0, false, 0, &tid) != ts.SQLITE_OK)
        fail("todo insert");
    if (tid <= 0) fail("todo id");

    var todo_count: usize = 0;
    const rc2 = ts.storeTodosForeach(s, &todo_count, struct {
        fn cb(ud: ?*anyopaque, _: ts.TodoRow) void {
            const p: *usize = @ptrCast(@alignCast(ud.?));
            p.* += 1;
        }
    }.cb);
    if (rc2 != ts.SQLITE_OK) fail("todos foreach");
    if (todo_count != 1) fail("todo foreach count");

    if (ts.storeTodoUpdateFull(s, tid, "Task one", "desc", "in_progress", "low", false, 0, false, 0, false, 0, 0) != ts.SQLITE_OK)
        fail("todo update");

    if (ts.storeTodoUpdateFull(s, tid, "Task one", "desc", "archived", "low", false, 0, false, 0, false, 0, 0) != ts.SQLITE_OK)
        fail("todo update archived");

    if (ts.storeTodoDeleteById(s, tid) != ts.SQLITE_OK) fail("todo delete");
    todo_count = 0;
    const rc3 = ts.storeTodosForeach(s, &todo_count, struct {
        fn cb(ud: ?*anyopaque, _: ts.TodoRow) void {
            const p: *usize = @ptrCast(@alignCast(ud.?));
            p.* += 1;
        }
    }.cb);
    if (rc3 != ts.SQLITE_OK) fail("todos foreach 2");
    if (todo_count != 0) fail("todo count after delete");

    if (ts.storeCategoryDeleteById(s, cat1) != ts.SQLITE_OK) fail("category delete");
    cat_count = 0;
    const rc4 = ts.storeCategoriesForeach(s, &cat_count, struct {
        fn cb(ud: ?*anyopaque, _: ts.CategoryRow) void {
            const p: *usize = @ptrCast(@alignCast(ud.?));
            p.* += 1;
        }
    }.cb);
    if (rc4 != ts.SQLITE_OK) fail("cat foreach 2");
    if (cat_count != 0) fail("categories empty");

    var bad: i64 = 0;
    if (ts.storeCategoryInsert(s, "", "#fff", &bad) == ts.SQLITE_OK) fail("empty category name");
    if (ts.storeTodoInsert(s, "", "x", "pending", "medium", false, 0, false, 0, false, 0, &tid) == ts.SQLITE_OK)
        fail("empty title");
}

pub fn main() u8 {
    mainInner();
    return if (g_fail) 1 else 0;
}
