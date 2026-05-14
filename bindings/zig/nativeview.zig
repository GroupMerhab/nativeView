//! Low-level Zig FFI for the nativeview public C API (`include/nv.h`, `include/nv_hotkey.h`,
//! plus `nv_version_string` / `nv_get_version_info` / `nv_bench_now` from `include/nv_util.h`).
//!
//! Linking: see `docs/Zig.md`. This module has **no** `std` dependency.
//! Wire it from the root module, e.g. `zig build-exe --dep nativeview -Mroot=app.zig -Mnativeview=bindings/zig/nativeview.zig …`.
//!
//! ## `nv_eval_js_batch` (`const char **scripts`, `size_t count`)
//! Pass a pointer to the first element of a slice of null-terminated C strings, e.g.
//! `scripts.ptr` where `scripts` is `[]const [*:0]const u8` (literals work). Storage must
//! stay valid until nativeview consumes the call.
//!
//! SPDX-License-Identifier: Apache-2.0

pub const nv_app_t = opaque {};
pub const nv_window_t = opaque {};

pub const nv_msg_cb_t = *const fn (
    window: ?*nv_window_t,
    event: ?[*:0]const u8,
    json: ?[*:0]const u8,
    userdata: ?*anyopaque,
) callconv(.c) void;

pub const nv_ready_cb_t = *const fn (
    window: ?*nv_window_t,
    userdata: ?*anyopaque,
) callconv(.c) void;

pub const nv_close_cb_t = *const fn (
    window: ?*nv_window_t,
    userdata: ?*anyopaque,
) callconv(.c) void;

/// Matches C `nv_window_cfg_t` / Nim `NvWindowCfg` field order and types.
pub const nv_window_cfg_t = extern struct {
    title: ?[*:0]const u8,
    width: c_int,
    height: c_int,
    min_width: c_int,
    min_height: c_int,
    resizable: c_int,
    frameless: c_int,
    transparent: c_int,
    devtools: c_int,
    modal: c_int,
};

/// Matches C `NvVersionInfo` in `include/nv_util.h`.
pub const nv_version_info_t = extern struct {
    major: u16,
    minor: u16,
    patch: u16,
    build_sha: ?[*:0]const u8,
};

pub const NV_HOTKEY_OK: c_int = 0;
pub const NV_HOTKEY_ERR_INVALID: c_int = -1;
pub const NV_HOTKEY_ERR_ALREADY_EXISTS: c_int = -2;
pub const NV_HOTKEY_ERR_NOT_SUPPORTED: c_int = -3;
pub const NV_HOTKEY_ERR_PLATFORM: c_int = -4;

pub extern "c" fn nv_app_create() ?*nv_app_t;
pub extern "c" fn nv_app_run(app: ?*nv_app_t) void;
pub extern "c" fn nv_app_quit(app: ?*nv_app_t) void;
pub extern "c" fn nv_app_destroy(app: ?*nv_app_t) void;

pub extern "c" fn nv_window_create(app: ?*nv_app_t, config: *const nv_window_cfg_t) ?*nv_window_t;
pub extern "c" fn nv_window_close(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_destroy(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_show(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_hide(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_set_title(window: ?*nv_window_t, title: ?[*:0]const u8) void;
pub extern "c" fn nv_window_set_size(window: ?*nv_window_t, width: c_int, height: c_int) void;
pub extern "c" fn nv_window_set_min_size(window: ?*nv_window_t, width: c_int, height: c_int) void;
pub extern "c" fn nv_window_center(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_fullscreen(window: ?*nv_window_t, enable: c_int) void;
pub extern "c" fn nv_window_is_fullscreen(window: ?*nv_window_t) c_int;
pub extern "c" fn nv_window_minimize(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_maximize(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_restore(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_focus(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_is_focused(window: ?*nv_window_t) c_int;
pub extern "c" fn nv_window_set_resizable(window: ?*nv_window_t, enable: c_int) void;
pub extern "c" fn nv_window_set_always_on_top(window: ?*nv_window_t, enable: c_int) void;
pub extern "c" fn nv_window_set_opacity(window: ?*nv_window_t, opacity_pct: c_int) void;
pub extern "c" fn nv_window_set_zoom_factor(window: ?*nv_window_t, factor: f64) void;
pub extern "c" fn nv_window_request_close(window: ?*nv_window_t) void;
pub extern "c" fn nv_window_on_close(window: ?*nv_window_t, callback: ?nv_close_cb_t, userdata: ?*anyopaque) void;

pub extern "c" fn nv_load_url(window: ?*nv_window_t, url: ?[*:0]const u8) void;
pub extern "c" fn nv_load_html(window: ?*nv_window_t, html: ?[*:0]const u8, base_url: ?[*:0]const u8) void;
pub extern "c" fn nv_load_html_ref(window: ?*nv_window_t, html: ?[*]const u8, length: usize, base_url: ?[*:0]const u8) void;
pub extern "c" fn nv_eval_js(window: ?*nv_window_t, js: ?[*:0]const u8) void;
pub extern "c" fn nv_eval_js_batch(window: ?*nv_window_t, scripts: [*]const [*:0]const u8, count: usize) void;

pub extern "c" fn nv_on_message(window: ?*nv_window_t, callback: ?nv_msg_cb_t, userdata: ?*anyopaque) void;
pub extern "c" fn nv_on_ready(window: ?*nv_window_t, callback: ?nv_ready_cb_t, userdata: ?*anyopaque) void;
pub extern "c" fn nv_send(window: ?*nv_window_t, event: ?[*:0]const u8, json: ?[*:0]const u8) void;

pub extern "c" fn nv_window_register_hotkey(window: ?*nv_window_t, id: ?[*:0]const u8, combo: ?[*:0]const u8) c_int;
pub extern "c" fn nv_window_unregister_hotkey(window: ?*nv_window_t, id: ?[*:0]const u8) void;

pub extern "c" fn nv_version_string() ?[*:0]const u8;
pub extern "c" fn nv_get_version_info(out_info: *nv_version_info_t) void;
pub extern "c" fn nv_bench_now() u64;
