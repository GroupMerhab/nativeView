//! Low-level Rust FFI for the nativeview public C API (`include/nv.h`, `include/nv_hotkey.h`,
//! plus `nv_version_string` / `nv_get_version_info` / `nv_bench_now` from `include/nv_util.h`).
//!
//! # Linking
//! - **Shared** (default): crate feature `link-shared`. Put the shared library on the linker
//!   search path (`nativeview.dll`, `libnativeview.so`, `libnativeview.dylib`) or pass
//!   `-L /path/to/build` when building your binary.
//! - **Static**: `--no-default-features --features link-static` and supply `libnativeview.a` (or a
//!   linker script that resolves `libnativeview`) on the search path.
//! - **External / multi-archive static** (CMake modules): `--no-default-features --features link-none`
//!   and pass the archive chain (`-Wl,--start-group` …) via `RUSTFLAGS` / `build.rs`, as in
//!   `docs/Nim.md` / `docs/Zig.md`.
//! - **macOS GUI:** prefer the shared library; static startup can conflict with AppKit (see `docs/Nim.md`).
//!
//! # Callbacks and lifetimes
//! - Register only `unsafe extern "C" fn` with stable addresses for the whole registration window.
//! - `nv_on_message`: `event` and `json` are valid only inside the callback (copy if needed).
//! - [`NvWindowCfg::title`](struct.NvWindowCfg.html) must outlive [`nv_window_create`](fn.nv_window_create.html) (see `nv.h`).
//!
//! # `nv_eval_js_batch`
//! Pass a pointer to the first element of a contiguous array of null-terminated C strings; each
//! pointer and the array must stay valid until the call returns.
//!
//! SPDX-License-Identifier: Apache-2.0

#![no_std]

use core::ffi::{c_char, c_double, c_int, c_void};

#[cfg(all(feature = "link-shared", feature = "link-static"))]
compile_error!("nativeview: enable only one of `link-shared`, `link-static`, `link-none`");

#[cfg(all(feature = "link-shared", feature = "link-none"))]
compile_error!("nativeview: enable only one of `link-shared`, `link-static`, `link-none`");

#[cfg(all(feature = "link-static", feature = "link-none"))]
compile_error!("nativeview: enable only one of `link-shared`, `link-static`, `link-none`");

#[cfg(not(any(feature = "link-shared", feature = "link-static", feature = "link-none")))]
compile_error!(
    "nativeview: enable `link-shared` (default), `link-static`, or `link-none` so the crate can link nativeview"
);

#[cfg_attr(feature = "link-shared", link(name = "nativeview", kind = "dylib"))]
#[cfg_attr(all(feature = "link-static", not(feature = "link-shared")), link(name = "nativeview", kind = "static"))]
extern "C" {
    pub fn nv_app_create() -> *mut NvApp;
    pub fn nv_app_run(app: *mut NvApp);
    pub fn nv_app_quit(app: *mut NvApp);
    pub fn nv_app_destroy(app: *mut NvApp);

    pub fn nv_window_create(app: *mut NvApp, config: *const NvWindowCfg) -> *mut NvWindow;
    pub fn nv_window_close(window: *mut NvWindow);
    pub fn nv_window_destroy(window: *mut NvWindow);
    pub fn nv_window_show(window: *mut NvWindow);
    pub fn nv_window_hide(window: *mut NvWindow);
    pub fn nv_window_set_title(window: *mut NvWindow, title: *const c_char);
    pub fn nv_window_set_size(window: *mut NvWindow, width: c_int, height: c_int);
    pub fn nv_window_set_min_size(window: *mut NvWindow, width: c_int, height: c_int);
    pub fn nv_window_center(window: *mut NvWindow);
    pub fn nv_window_fullscreen(window: *mut NvWindow, enable: c_int);
    pub fn nv_window_is_fullscreen(window: *mut NvWindow) -> c_int;
    pub fn nv_window_minimize(window: *mut NvWindow);
    pub fn nv_window_maximize(window: *mut NvWindow);
    pub fn nv_window_restore(window: *mut NvWindow);
    pub fn nv_window_focus(window: *mut NvWindow);
    pub fn nv_window_is_focused(window: *mut NvWindow) -> c_int;
    pub fn nv_window_set_resizable(window: *mut NvWindow, enable: c_int);
    pub fn nv_window_set_always_on_top(window: *mut NvWindow, enable: c_int);
    pub fn nv_window_set_opacity(window: *mut NvWindow, opacity_pct: c_int);
    pub fn nv_window_set_zoom_factor(window: *mut NvWindow, factor: c_double);
    pub fn nv_window_request_close(window: *mut NvWindow);
    pub fn nv_window_on_close(
        window: *mut NvWindow,
        callback: Option<NvCloseCb>,
        userdata: *mut c_void,
    );

    pub fn nv_load_url(window: *mut NvWindow, url: *const c_char);
    pub fn nv_load_html(window: *mut NvWindow, html: *const c_char, base_url: *const c_char);
    pub fn nv_load_html_ref(
        window: *mut NvWindow,
        html: *const c_char,
        length: usize,
        base_url: *const c_char,
    );
    pub fn nv_eval_js(window: *mut NvWindow, js: *const c_char);
    pub fn nv_eval_js_batch(window: *mut NvWindow, scripts: *const *const c_char, count: usize);

    pub fn nv_on_message(
        window: *mut NvWindow,
        callback: Option<NvMsgCb>,
        userdata: *mut c_void,
    );
    pub fn nv_on_ready(
        window: *mut NvWindow,
        callback: Option<NvReadyCb>,
        userdata: *mut c_void,
    );
    pub fn nv_send(window: *mut NvWindow, event: *const c_char, json: *const c_char);

    pub fn nv_window_register_hotkey(
        window: *mut NvWindow,
        id: *const c_char,
        combo: *const c_char,
    ) -> c_int;
    pub fn nv_window_unregister_hotkey(window: *mut NvWindow, id: *const c_char);

    pub fn nv_version_string() -> *const c_char;
    pub fn nv_get_version_info(out_info: *mut NvVersionInfo);
    pub fn nv_bench_now() -> u64;
}

/// Opaque application handle (`nv_app_t`).
pub enum NvApp {}

/// Opaque window handle (`nv_window_t`).
pub enum NvWindow {}

pub type NvMsgCb = unsafe extern "C" fn(
    window: *mut NvWindow,
    event: *const c_char,
    json: *const c_char,
    userdata: *mut c_void,
);

pub type NvReadyCb = unsafe extern "C" fn(window: *mut NvWindow, userdata: *mut c_void);

pub type NvCloseCb = unsafe extern "C" fn(window: *mut NvWindow, userdata: *mut c_void);

/// Matches C `nv_window_cfg_t` / Nim `NvWindowCfg` field order and types.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct NvWindowCfg {
    pub title: *const c_char,
    pub width: c_int,
    pub height: c_int,
    pub min_width: c_int,
    pub min_height: c_int,
    pub resizable: c_int,
    pub frameless: c_int,
    pub transparent: c_int,
    pub devtools: c_int,
    pub modal: c_int,
}

/// Matches C `NvVersionInfo` in `include/nv_util.h`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct NvVersionInfo {
    pub major: u16,
    pub minor: u16,
    pub patch: u16,
    pub build_sha: *const c_char,
}

pub const NV_HOTKEY_OK: c_int = 0;
pub const NV_HOTKEY_ERR_INVALID: c_int = -1;
pub const NV_HOTKEY_ERR_ALREADY_EXISTS: c_int = -2;
pub const NV_HOTKEY_ERR_NOT_SUPPORTED: c_int = -3;
pub const NV_HOTKEY_ERR_PLATFORM: c_int = -4;
