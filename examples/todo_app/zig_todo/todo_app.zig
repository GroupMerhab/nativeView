//! Todo app: Vue UI + SQLite + nativeview (Zig port of nim_todo/todo_app.nim).
//! SPDX-License-Identifier: Apache-2.0

const std = @import("std");
const nv = @import("nv");
const ts = @import("ts");
const br = @import("br");
const embed = @import("embed");

const AppCtx = struct {
    store: *ts.TodoStore,
};

var g_ctx: AppCtx = undefined;

fn emitToJs(userdata: ?*anyopaque, event: []const u8, json_body: []const u8) void {
    const win: ?*nv.nv_window_t = @ptrCast(userdata);
    const evz = std.heap.c_allocator.dupeSentinel(u8, event, 0) catch return;
    defer std.heap.c_allocator.free(evz);
    const jz = std.heap.c_allocator.dupeSentinel(u8, json_body, 0) catch return;
    defer std.heap.c_allocator.free(jz);
    nv.nv_send(win, evz.ptr, jz.ptr);
}

fn onReady(window: ?*nv.nv_window_t, userdata: ?*anyopaque) callconv(.c) void {
    _ = window;
    _ = userdata;
    std.debug.print("[todo_app] webview ready\n", .{});
}

fn onMessage(window: ?*nv.nv_window_t, event: ?[*:0]const u8, json_msg: ?[*:0]const u8, userdata: ?*anyopaque) callconv(.c) void {
    const ctx = userdata orelse return;
    const c: *AppCtx = @ptrCast(@alignCast(ctx));
    if (json_msg == null) return;
    const ev = event orelse return;
    if (!std.mem.eql(u8, std.mem.span(ev), "todo")) return;
    const body = std.mem.span(json_msg.?);
    const allocator = std.heap.page_allocator;
    _ = br.bridgeHandleWire(c.store, allocator, body, window, emitToJs);
}

fn onClose(win: ?*nv.nv_window_t, userdata: ?*anyopaque) callconv(.c) void {
    _ = win;
    const app: ?*nv.nv_app_t = @ptrCast(@alignCast(userdata));
    nv.nv_app_quit(app);
}

pub fn main(minimal: std.process.Init.Minimal) void {
    var arg_it = std.process.Args.Iterator.init(minimal.args);
    defer arg_it.deinit();
    _ = arg_it.skip();
    const default_db: [:0]const u8 = "./todo_app.db";
    const db_path: [:0]const u8 = arg_it.next() orelse default_db;

    const store = ts.storeOpen(db_path) orelse {
        std.debug.print("[todo_app] todo_store_open failed for {s}\n", .{db_path});
        return;
    };
    defer ts.storeClose(store);

    const app = nv.nv_app_create() orelse {
        std.debug.print("[todo_app] nv_app_create failed\n", .{});
        return;
    };
    defer nv.nv_app_destroy(app);

    const cfg = nv.nv_window_cfg_t{
        .title = "Todo App",
        .width = 960,
        .height = 720,
        .min_width = 0,
        .min_height = 0,
        .resizable = 1,
        .frameless = 0,
        .transparent = 0,
        .devtools = 1,
        .modal = 0,
    };

    const win = nv.nv_window_create(app, &cfg) orelse {
        std.debug.print("[todo_app] nv_window_create failed\n", .{});
        return;
    };

    g_ctx = .{ .store = store };

    nv.nv_on_ready(win, onReady, null);
    nv.nv_on_message(win, onMessage, &g_ctx);
    nv.nv_window_on_close(win, onClose, @ptrCast(app));

    nv.nv_load_html_ref(
        win,
        @ptrCast(&embed.nv_todo_html_data[0]),
        embed.nv_todo_html_len,
        "about:blank",
    );
    nv.nv_window_show(win);

    nv.nv_app_run(app);
}
