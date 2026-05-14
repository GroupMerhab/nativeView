//! Minimal nativeview sample: `nv_on_ready` → `nv_load_html`, `nv_eval_js_batch`, teardown.
//! Build with `examples/zig/build_static.sh` (Linux/macOS) or `build_static.ps1` (Windows).
//! Requires `--dep nativeview` and `-Mnativeview=…/bindings/zig/nativeview.zig` (see the scripts).
//!
//! SPDX-License-Identifier: Apache-2.0

const std = @import("std");
const nv = @import("nativeview");

const html: [:0]const u8 =
    \\<!doctype html>
    \\<html><head><meta charset="utf-8"><title>Zig + nativeview</title></head>
    \\<body><p id="t">loading…</p>
    \\<script>window.__nv.on('ping', function(d){ document.getElementById('t').textContent = d.msg; });</script>
    \\</body></html>
;

fn onReady(window: ?*nv.nv_window_t, userdata: ?*anyopaque) callconv(.c) void {
    _ = userdata;
    nv.nv_load_html(window, html.ptr, "about:blank");
    const scripts = [_][*:0]const u8{ "document.title='zig'", "void 0" };
    nv.nv_eval_js_batch(window, scripts[0..].ptr, scripts.len);
    nv.nv_send(window, "ping", "{\"msg\":\"hello from Zig\"}");
}

fn onMessage(window: ?*nv.nv_window_t, event: ?[*:0]const u8, json: ?[*:0]const u8, userdata: ?*anyopaque) callconv(.c) void {
    _ = window;
    _ = event;
    _ = json;
    _ = userdata;
}

fn onClose(window: ?*nv.nv_window_t, userdata: ?*anyopaque) callconv(.c) void {
    _ = window;
    const app: ?*nv.nv_app_t = @ptrCast(@alignCast(userdata));
    nv.nv_app_quit(app);
}

pub fn main() void {
    if (nv.nv_version_string()) |ver| {
        std.debug.print("{s}\n", .{std.mem.span(ver)});
    }

    var info: nv.nv_version_info_t = undefined;
    nv.nv_get_version_info(&info);
    std.debug.print("nativeview {}.{}.{}\n", .{ info.major, info.minor, info.patch });

    const app = nv.nv_app_create() orelse {
        return;
    };

    const cfg = nv.nv_window_cfg_t{
        .title = "nativeview (Zig minimal)",
        .width = 800,
        .height = 600,
        .min_width = 0,
        .min_height = 0,
        .resizable = 1,
        .frameless = 0,
        .transparent = 0,
        .devtools = 1,
        .modal = 0,
    };

    const win = nv.nv_window_create(app, &cfg) orelse {
        nv.nv_app_destroy(app);
        return;
    };

    nv.nv_on_ready(win, onReady, null);
    nv.nv_on_message(win, onMessage, null);
    nv.nv_window_on_close(win, onClose, @ptrCast(app));

    nv.nv_window_show(win);
    nv.nv_app_run(app);
    nv.nv_app_destroy(app);
}
