//! Todo app: Vue UI + SQLite + nativeview (Rust port of `nim_todo/todo_app.nim`).
//!
//! SPDX-License-Identifier: Apache-2.0

mod generated {
    include!(concat!(env!("CARGO_MANIFEST_DIR"), "/generated/todo_html_embed.rs"));
}

use std::env;
use std::ffi::{c_char, c_void, CStr, CString};
use std::process::ExitCode;

use nativeview::{
    nv_app_create, nv_app_destroy, nv_app_quit, nv_app_run, nv_load_html_ref, nv_on_message,
    nv_on_ready, nv_send, nv_window_create, nv_window_on_close, nv_window_show, NvApp, NvWindow,
    NvCloseCb, NvMsgCb, NvReadyCb, NvWindowCfg,
};
use rust_todo::todo_bridge::bridge_handle_wire;
use rust_todo::todo_store::{store_close, store_open, TodoStore};

struct AppCtx {
    store: TodoStore,
}

unsafe fn send_json(win: *mut NvWindow, ev: &str, j: &str) {
    let Ok(ce) = CString::new(ev) else {
        return;
    };
    let Ok(cj) = CString::new(j) else {
        return;
    };
    nv_send(win, ce.as_ptr(), cj.as_ptr());
}

unsafe extern "C" fn on_ready(_win: *mut NvWindow, _userdata: *mut c_void) {
    eprintln!("[todo_app] webview ready\n");
}

unsafe extern "C" fn on_message(
    win: *mut NvWindow,
    event: *const c_char,
    json: *const c_char,
    userdata: *mut c_void,
) {
    if userdata.is_null() {
        return;
    }
    let ctx = &*(userdata as *const AppCtx);
    let ev = if event.is_null() {
        ""
    } else {
        CStr::from_ptr(event).to_str().unwrap_or("")
    };
    if ev != "todo" {
        return;
    }
    let body = if json.is_null() {
        ""
    } else {
        CStr::from_ptr(json).to_str().unwrap_or("")
    };
    let mut emit = |e: String, j: String| {
        send_json(win, &e, &j);
    };
    let _ = bridge_handle_wire(&ctx.store, body, &mut emit);
}

unsafe extern "C" fn on_close(_win: *mut NvWindow, userdata: *mut c_void) {
    nv_app_quit(userdata.cast::<NvApp>());
}

fn main() -> ExitCode {
    let db_path = env::args()
        .nth(1)
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| "todo_app.db".to_string());

    let Some(store) = store_open(&db_path) else {
        eprintln!("[todo_app] todo_store_open failed for {db_path}");
        return ExitCode::FAILURE;
    };

    let app = unsafe { nv_app_create() };
    if app.is_null() {
        eprintln!("[todo_app] nv_app_create failed");
        store_close(store);
        return ExitCode::FAILURE;
    }

    let title = CString::new("Todo App").expect("title");
    let cfg = NvWindowCfg {
        title: title.as_ptr(),
        width: 960,
        height: 720,
        min_width: 0,
        min_height: 0,
        resizable: 1,
        frameless: 0,
        transparent: 0,
        devtools: 1,
        modal: 0,
    };

    let win = unsafe { nv_window_create(app, &cfg) };
    if win.is_null() {
        eprintln!("[todo_app] nv_window_create failed");
        store_close(store);
        unsafe {
            nv_app_destroy(app);
        }
        return ExitCode::FAILURE;
    }

    let ctx = Box::new(AppCtx { store });
    let ctx_ptr = Box::into_raw(ctx);

    unsafe {
        nv_on_ready(win, Some(on_ready as NvReadyCb), std::ptr::null_mut());
        nv_on_message(win, Some(on_message as NvMsgCb), ctx_ptr.cast());
        nv_window_on_close(win, Some(on_close as NvCloseCb), app.cast());

        let base = CString::new("about:blank").expect("base url");
        nv_load_html_ref(
            win,
            generated::NV_TODO_HTML_DATA.as_ptr().cast::<c_char>(),
            generated::NV_TODO_HTML_LEN,
            base.as_ptr(),
        );
        nv_window_show(win);

        nv_app_run(app);
    }

    let ctx = unsafe { Box::from_raw(ctx_ptr) };
    store_close(ctx.store);
    unsafe {
        nv_app_destroy(app);
    }

    ExitCode::SUCCESS
}
