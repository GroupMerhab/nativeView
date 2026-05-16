// Todo app: Vue UI + SQLite + nativeview (C# port of nim_todo/todo_app.nim).
// SPDX-License-Identifier: Apache-2.0

using System.Runtime.InteropServices;
using NativeView;

namespace Jamharah.Todo;

internal sealed class AppCtx
{
    public TodoStore? Store;
    public IntPtr App;
    public IntPtr Win;
}

internal static class Program
{
    private static IntPtr _gTitle = IntPtr.Zero;
    private static AppCtx? _gCtx;
    private static NvReadyCb? _onReady;
    private static NvMsgCb? _onMessage;
    private static NvCloseCb? _onClose;

    public static int Main(string[] args)
    {
        var dbPath = args.Length >= 1 && !string.IsNullOrEmpty(args[0]) ? args[0] : "todo_app.db";
        var store = TodoStoreApi.StoreOpen(dbPath);
        if (store is null)
        {
            Console.Error.WriteLine($"[todo_app] todo_store_open failed for {dbPath}");
            return 1;
        }

        var app = NvNative.nv_app_create();
        if (app == IntPtr.Zero)
        {
            Console.Error.WriteLine("[todo_app] nv_app_create failed");
            TodoStoreApi.StoreClose(store);
            return 1;
        }

        _gTitle = NvUtf8.Alloc("Todo App")!;
        var cfg = new NvWindowCfg
        {
            Title = _gTitle,
            Width = 960,
            Height = 720,
            MinWidth = 0,
            MinHeight = 0,
            Resizable = 1,
            Frameless = 0,
            Transparent = 0,
            Devtools = 1,
            Modal = 0,
        };

        var win = NvNative.nv_window_create(app, ref cfg);
        if (win == IntPtr.Zero)
        {
            Console.Error.WriteLine("[todo_app] nv_window_create failed");
            TodoStoreApi.StoreClose(store);
            NvNative.nv_app_destroy(app);
            NvUtf8.Free(_gTitle);
            return 1;
        }

        _gCtx = new AppCtx { Store = store, App = app, Win = win };

        _onReady = OnReady;
        _onMessage = OnMessage;
        _onClose = OnClose;

        NvNative.nv_on_ready(win, _onReady, IntPtr.Zero);
        NvNative.nv_on_message(win, _onMessage, IntPtr.Zero);
        NvNative.nv_window_on_close(win, _onClose, app);

        LoadEmbeddedHtml(win);
        NvNative.nv_window_show(win);
        NvNative.nv_app_run(app);

        TodoStoreApi.StoreClose(store);
        _gCtx = null;
        NvNative.nv_app_destroy(app);
        NvUtf8.Free(_gTitle);
        _gTitle = IntPtr.Zero;
        return 0;
    }

    private static void LoadEmbeddedHtml(IntPtr win)
    {
        var baseUrl = NvUtf8.Alloc("about:blank")!;
        try
        {
            var data = TodoHtmlEmbed.Data;
            unsafe
            {
                fixed (byte* ptr = data)
                {
                    NvNative.nv_load_html_ref(win, (IntPtr)ptr, (UIntPtr)data.Length, baseUrl);
                }
            }
        }
        finally
        {
            NvUtf8.Free(baseUrl);
        }
    }

    private static void SendJson(IntPtr win, string ev, string json)
    {
        var e = NvUtf8.Alloc(ev)!;
        var j = NvUtf8.Alloc(json)!;
        try
        {
            NvNative.nv_send(win, e, j);
        }
        finally
        {
            NvUtf8.Free(e);
            NvUtf8.Free(j);
        }
    }

    private static void OnReady(IntPtr win, IntPtr userdata)
    {
        _ = win;
        _ = userdata;
        Console.Error.WriteLine("[todo_app] webview ready");
    }

    private static void OnMessage(IntPtr win, IntPtr eventName, IntPtr json, IntPtr userdata)
    {
        _ = userdata;
        var ctx = _gCtx;
        if (ctx?.Store is null)
            return;
        var ev = NvUtf8.PtrToString(eventName);
        if (ev != "todo")
            return;
        var body = NvUtf8.PtrToString(json);
        if (body is null)
            return;
        TodoBridge.BridgeHandleWire(ctx.Store, body, (e, j) => SendJson(win, e, j));
    }

    private static void OnClose(IntPtr win, IntPtr userdata)
    {
        _ = win;
        NvNative.nv_app_quit(userdata);
    }
}
