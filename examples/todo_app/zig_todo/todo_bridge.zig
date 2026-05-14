//! IPC dispatch + JSON (port of nim_todo/todo_bridge.nim).
//! SPDX-License-Identifier: Apache-2.0

const std = @import("std");
const ts = @import("ts");
const json = std.json;

pub const EmitFn = *const fn (userdata: ?*anyopaque, event: []const u8, json_body: []const u8) void;

fn emitErr(allocator: std.mem.Allocator, emit: EmitFn, userdata: ?*anyopaque, code: i32, msg: []const u8) void {
    const s = std.fmt.allocPrint(allocator, "{{\"code\":{d},\"message\":\"{s}\"}}", .{ code, msg }) catch return;
    defer allocator.free(s);
    emit(userdata, "error", s);
}

fn jStr(pl: json.Value, key: []const u8) []const u8 {
    if (pl != .object) return "";
    const v = pl.object.get(key) orelse return "";
    return switch (v) {
        .string => |s| s,
        else => "",
    };
}

fn jStrOr(pl: json.Value, key: []const u8, default: []const u8) []const u8 {
    const s = jStr(pl, key);
    if (s.len > 0) return s;
    return default;
}

fn jInt64(pl: json.Value, key: []const u8) i64 {
    if (pl != .object) return 0;
    const v = pl.object.get(key) orelse return 0;
    return switch (v) {
        .integer => |i| i,
        .float => |f| @intFromFloat(f),
        else => 0,
    };
}

fn categoryValue(allocator: std.mem.Allocator, r: ts.CategoryRow) !json.Value {
    var obj: json.ObjectMap = .empty;
    try obj.putNoClobber(allocator, "id", .{ .integer = r.id });
    try obj.putNoClobber(allocator, "name", .{ .string = try allocator.dupe(u8, r.name) });
    try obj.putNoClobber(allocator, "color", .{ .string = try allocator.dupe(u8, r.color) });
    return .{ .object = obj };
}

fn todoValue(allocator: std.mem.Allocator, r: ts.TodoRow) !json.Value {
    var obj: json.ObjectMap = .empty;
    try obj.putNoClobber(allocator, "id", .{ .integer = r.id });
    try obj.putNoClobber(allocator, "title", .{ .string = try allocator.dupe(u8, r.title) });
    try obj.putNoClobber(allocator, "description", .{ .string = try allocator.dupe(u8, r.description) });
    try obj.putNoClobber(allocator, "status", .{ .string = try allocator.dupe(u8, r.status) });
    try obj.putNoClobber(allocator, "priority", .{ .string = try allocator.dupe(u8, r.priority) });
    if (r.category_id_is_null) {
        try obj.putNoClobber(allocator, "categoryId", .null);
    } else {
        try obj.putNoClobber(allocator, "categoryId", .{ .integer = r.category_id });
    }
    if (r.parent_id_is_null) {
        try obj.putNoClobber(allocator, "parentId", .null);
    } else {
        try obj.putNoClobber(allocator, "parentId", .{ .integer = r.parent_id });
    }
    if (r.due_date_is_null) {
        try obj.putNoClobber(allocator, "dueDate", .null);
    } else {
        try obj.putNoClobber(allocator, "dueDate", .{ .integer = r.due_date });
    }
    try obj.putNoClobber(allocator, "position", .{ .integer = @intCast(r.position) });
    try obj.putNoClobber(allocator, "createdAt", .{ .integer = r.created_at });
    try obj.putNoClobber(allocator, "updatedAt", .{ .integer = r.updated_at });
    return .{ .object = obj };
}

const CatCollectCtx = struct {
    allocator: std.mem.Allocator,
    arr: *json.Array,
    err: ?anyerror = null,
};

fn catCollectCb(ud: ?*anyopaque, row: ts.CategoryRow) void {
    const ctx: *CatCollectCtx = @ptrCast(@alignCast(ud.?));
    if (ctx.err != null) return;
    const v = categoryValue(ctx.allocator, row) catch |e| {
        ctx.err = e;
        return;
    };
    ctx.arr.append(v) catch |e| {
        ctx.err = e;
    };
}

fn emitCategoriesList(allocator: std.mem.Allocator, store: *ts.TodoStore, emit: EmitFn, userdata: ?*anyopaque) i32 {
    var arr = json.Array.init(allocator);
    defer arr.deinit();
    var ctx = CatCollectCtx{ .allocator = allocator, .arr = &arr };
    const rc = ts.storeCategoriesForeach(store, &ctx, catCollectCb);
    if (rc != ts.SQLITE_OK) {
        emitErr(allocator, emit, userdata, @intCast(rc), "categories.foreach failed");
        return -1;
    }
    if (ctx.err) |_| {
        emitErr(allocator, emit, userdata, -1, "categories.foreach alloc failed");
        return -1;
    }
    const out = std.json.Stringify.valueAlloc(allocator, json.Value{ .array = arr }, .{}) catch {
        emitErr(allocator, emit, userdata, -1, "categories list stringify failed");
        return -1;
    };
    defer allocator.free(out);
    emit(userdata, "categories.list", out);
    return 0;
}

const TodoCollectCtx = struct {
    allocator: std.mem.Allocator,
    arr: *json.Array,
    err: ?anyerror = null,
};

fn todoCollectCb(ud: ?*anyopaque, row: ts.TodoRow) void {
    const ctx: *TodoCollectCtx = @ptrCast(@alignCast(ud.?));
    if (ctx.err != null) return;
    const v = todoValue(ctx.allocator, row) catch |e| {
        ctx.err = e;
        return;
    };
    ctx.arr.append(v) catch |e| {
        ctx.err = e;
    };
}

fn emitTodosList(allocator: std.mem.Allocator, store: *ts.TodoStore, emit: EmitFn, userdata: ?*anyopaque) i32 {
    var arr = json.Array.init(allocator);
    defer arr.deinit();
    var ctx = TodoCollectCtx{ .allocator = allocator, .arr = &arr };
    const rc = ts.storeTodosForeach(store, &ctx, todoCollectCb);
    if (rc != ts.SQLITE_OK) {
        emitErr(allocator, emit, userdata, @intCast(rc), "todos.foreach failed");
        return -1;
    }
    if (ctx.err) |_| {
        emitErr(allocator, emit, userdata, -1, "todos.foreach alloc failed");
        return -1;
    }
    const out = std.json.Stringify.valueAlloc(allocator, json.Value{ .array = arr }, .{}) catch {
        emitErr(allocator, emit, userdata, -1, "todos list stringify failed");
        return -1;
    };
    defer allocator.free(out);
    emit(userdata, "todos.list", out);
    return 0;
}

pub fn bridgeHandleWire(
    store: *ts.TodoStore,
    allocator: std.mem.Allocator,
    raw_ipc_json: []const u8,
    userdata: ?*anyopaque,
    emit: EmitFn,
) i32 {
    if (raw_ipc_json.len == 0) return -1;

    const parsed = json.parseFromSlice(json.Value, allocator, raw_ipc_json, .{}) catch {
        emitErr(allocator, emit, userdata, -1, "invalid wire json");
        return -1;
    };
    defer parsed.deinit();

    const root = parsed.value;
    if (root != .object) {
        emitErr(allocator, emit, userdata, -1, "invalid wire json");
        return -1;
    }
    const d = root.object.get("d") orelse {
        emitErr(allocator, emit, userdata, -1, "missing wire.d object");
        return -1;
    };
    if (d != .object) {
        emitErr(allocator, emit, userdata, -1, "missing wire.d object");
        return -1;
    }

    const action_val = d.object.get("action") orelse {
        emitErr(allocator, emit, userdata, -1, "missing action");
        return -1;
    };
    const action = switch (action_val) {
        .string => |s| s,
        else => "",
    };
    if (action.len == 0) {
        emitErr(allocator, emit, userdata, -1, "missing action");
        return -1;
    }

    const pl: ?json.Value = blk: {
        const p = d.object.get("payload") orelse break :blk null;
        if (p == .object) break :blk p;
        break :blk null;
    };

    if (std.mem.eql(u8, action, "categories.list")) {
        return emitCategoriesList(allocator, store, emit, userdata);
    }
    if (std.mem.eql(u8, action, "todos.list")) {
        return emitTodosList(allocator, store, emit, userdata);
    }

    if (std.mem.eql(u8, action, "categories.create")) {
        const plv = pl orelse {
            emitErr(allocator, emit, userdata, @intCast(ts.SQLITE_MISUSE), "missing payload");
            return -1;
        };
        const name = jStr(plv, "name");
        const color = jStrOr(plv, "color", "#6366f1");
        var new_id: i64 = 0;
        const rc = ts.storeCategoryInsert(store, name, color, &new_id);
        if (rc != ts.SQLITE_OK) {
            emitErr(allocator, emit, userdata, @intCast(rc), "category insert failed");
            return -1;
        }
        return emitCategoriesList(allocator, store, emit, userdata);
    }

    if (std.mem.eql(u8, action, "categories.delete")) {
        const plv = pl orelse {
            emitErr(allocator, emit, userdata, @intCast(ts.SQLITE_MISUSE), "missing payload");
            return -1;
        };
        const id = jInt64(plv, "id");
        const rc = ts.storeCategoryDeleteById(store, id);
        if (rc != ts.SQLITE_OK) {
            emitErr(allocator, emit, userdata, @intCast(rc), "category delete failed");
            return -1;
        }
        return emitCategoriesList(allocator, store, emit, userdata);
    }

    if (std.mem.eql(u8, action, "todos.create")) {
        const plv = pl orelse {
            emitErr(allocator, emit, userdata, @intCast(ts.SQLITE_MISUSE), "missing payload");
            return -1;
        };
        const title = jStr(plv, "title");
        const desc = jStrOr(plv, "description", "");
        const status = jStrOr(plv, "status", "pending");
        const pri = jStrOr(plv, "priority", "medium");
        const cat = jInt64(plv, "categoryId");
        const par = jInt64(plv, "parentId");
        const due = jInt64(plv, "dueDate");
        const cat_set = cat > 0;
        const par_set = par > 0;
        const due_set = due != 0;
        var new_id: i64 = 0;
        const rc = ts.storeTodoInsert(store, title, desc, status, pri, cat_set, cat, par_set, par, due_set, due, &new_id);
        if (rc != ts.SQLITE_OK) {
            emitErr(allocator, emit, userdata, @intCast(rc), "todo insert failed");
            return -1;
        }
        return emitTodosList(allocator, store, emit, userdata);
    }

    if (std.mem.eql(u8, action, "todos.update")) {
        const plv = pl orelse {
            emitErr(allocator, emit, userdata, @intCast(ts.SQLITE_MISUSE), "missing payload");
            return -1;
        };
        const id = jInt64(plv, "id");
        const title = jStr(plv, "title");
        const desc = jStrOr(plv, "description", "");
        const status = jStrOr(plv, "status", "pending");
        const pri = jStrOr(plv, "priority", "medium");
        const cat = jInt64(plv, "categoryId");
        const par = jInt64(plv, "parentId");
        const due = jInt64(plv, "dueDate");
        const pos: c_int = @truncate(jInt64(plv, "position"));
        const cat_set = cat > 0;
        const par_set = par > 0;
        const due_set = due != 0;
        const rc = ts.storeTodoUpdateFull(store, id, title, desc, status, pri, cat_set, cat, par_set, par, due_set, due, pos);
        if (rc != ts.SQLITE_OK) {
            emitErr(allocator, emit, userdata, @intCast(rc), "todo update failed");
            return -1;
        }
        return emitTodosList(allocator, store, emit, userdata);
    }

    if (std.mem.eql(u8, action, "todos.delete")) {
        const plv = pl orelse {
            emitErr(allocator, emit, userdata, @intCast(ts.SQLITE_MISUSE), "missing payload");
            return -1;
        };
        const id = jInt64(plv, "id");
        const rc = ts.storeTodoDeleteById(store, id);
        if (rc != ts.SQLITE_OK) {
            emitErr(allocator, emit, userdata, @intCast(rc), "todo delete failed");
            return -1;
        }
        return emitTodosList(allocator, store, emit, userdata);
    }

    emitErr(allocator, emit, userdata, -1, "unknown action");
    return -1;
}
