//! Port of nim_todo/tests/test_todo_bridge.nim
//! SPDX-License-Identifier: Apache-2.0

const std = @import("std");
const ts = @import("ts");
const br = @import("br");
const json = std.json;

var g_fail: bool = false;
var g_last_event: std.array_list.Managed(u8) = undefined;
var g_last_json: std.array_list.Managed(u8) = undefined;

fn fail(comptime msg: []const u8) void {
    std.debug.print("test_todo_bridge: {s}\n", .{msg});
    g_fail = true;
}

fn capture(ud: ?*anyopaque, ev: []const u8, j: []const u8) void {
    _ = ud;
    g_last_event.clearRetainingCapacity();
    g_last_json.clearRetainingCapacity();
    g_last_event.appendSlice(ev) catch return;
    g_last_json.appendSlice(j) catch return;
}

fn countJsonArrayElems(allocator: std.mem.Allocator, arr_json: []const u8) i32 {
    const v = json.parseFromSlice(json.Value, allocator, arr_json, .{}) catch return -1;
    defer v.deinit();
    if (v.value != .array) return -1;
    return @intCast(v.value.array.items.len);
}

fn mainInner() void {
    g_fail = false;
    g_last_event = std.array_list.Managed(u8).init(std.heap.page_allocator);
    g_last_json = std.array_list.Managed(u8).init(std.heap.page_allocator);
    defer g_last_event.deinit();
    defer g_last_json.deinit();

    const allocator = std.heap.page_allocator;
    const s = ts.storeOpen("") orelse {
        fail("store");
        return;
    };
    defer ts.storeClose(s);

    const wire_list = "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"categories.list\"}}";
    if (br.bridgeHandleWire(s, allocator, wire_list, null, capture) != 0) fail("categories.list rc");
    if (!std.mem.eql(u8, g_last_event.items, "categories.list")) fail("event categories.list");
    if (!std.mem.eql(u8, g_last_json.items, "[]")) fail("empty categories json");

    const wire_mk = "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"categories.create\",\"payload\":{\"name\":\"A\",\"color\":\"#112233\"}}}";
    if (br.bridgeHandleWire(s, allocator, wire_mk, null, capture) != 0) fail("categories.create rc");
    if (!std.mem.eql(u8, g_last_event.items, "categories.list")) fail("event after create");
    if (countJsonArrayElems(allocator, g_last_json.items) != 1) fail("one category");

    const wire_todo = "{\"e\":\"todo\",\"s\":0,\"d\":{\"action\":\"todos.create\",\"payload\":{\"title\":\"T1\",\"description\":\"\",\"status\":\"pending\",\"priority\":\"medium\",\"categoryId\":1,\"parentId\":0,\"dueDate\":0}}}";
    if (br.bridgeHandleWire(s, allocator, wire_todo, null, capture) != 0) fail("todos.create rc");
    if (!std.mem.eql(u8, g_last_event.items, "todos.list")) fail("event todos.list");
    if (countJsonArrayElems(allocator, g_last_json.items) != 1) fail("one todo");

    const parsed = json.parseFromSlice(json.Value, allocator, g_last_json.items, .{}) catch {
        fail("parse todos");
        return;
    };
    defer parsed.deinit();
    if (parsed.value != .array or parsed.value.array.items.len < 1) {
        fail("parse todos");
        return;
    }
    const row = parsed.value.array.items[0];
    if (row != .object) {
        fail("parse todos row");
        return;
    }
    const tid = row.object.get("id") orelse {
        fail("todo id");
        return;
    };
    const tid_num: i64 = switch (tid) {
        .integer => |i| i,
        else => {
            fail("todo id type");
            return;
        },
    };

    const delbuf = std.fmt.allocPrint(allocator, "{{\"e\":\"todo\",\"s\":0,\"d\":{{\"action\":\"todos.delete\",\"payload\":{{\"id\":{d}}}}}}}", .{tid_num}) catch {
        fail("alloc delbuf");
        return;
    };
    defer allocator.free(delbuf);
    if (br.bridgeHandleWire(s, allocator, delbuf, null, capture) != 0) fail("delete rc");
    if (countJsonArrayElems(allocator, g_last_json.items) != 0) fail("zero todos after delete");
}

pub fn main() u8 {
    mainInner();
    return if (g_fail) 1 else 0;
}
