// Minimal nativeview sample: nv_on_ready → nv_load_html, nv_eval_js_batch, teardown.
// Build with examples/csharp/build_static.sh (Linux/macOS) or build_static.ps1 (Windows).
//
// SPDX-License-Identifier: Apache-2.0

using System.Runtime.InteropServices;
using NativeView;

const string Html = """
<!doctype html>
<html><head><meta charset="utf-8"><title>C# + nativeview</title></head>
<body><p id="t">loading…</p>
<script>window.__nv.on('ping', function(d){ document.getElementById('t').textContent = d.msg; });</script>
</body></html>
""";

IntPtr gTitle = NvUtf8.Alloc("nativeview (C# minimal)")!;

NvReadyCb onReady = OnReady;
NvMsgCb onMessage = OnMessage;
NvCloseCb onClose = OnClose;

try
{
    var verPtr = NvNative.nv_version_string();
    if (verPtr != IntPtr.Zero)
        Console.WriteLine(NvUtf8.PtrToString(verPtr));

    NvVersionInfo info = default;
    NvNative.nv_get_version_info(ref info);
    Console.WriteLine($"nativeview {info.Major}.{info.Minor}.{info.Patch}");

    var app = NvNative.nv_app_create();
    if (app == IntPtr.Zero)
    {
        Console.Error.WriteLine("nv_app_create failed");
        return 1;
    }

    var cfg = new NvWindowCfg
    {
        Title = gTitle,
        Width = 800,
        Height = 600,
        Resizable = 1,
        Devtools = 1,
    };

    var win = NvNative.nv_window_create(app, ref cfg);
    if (win == IntPtr.Zero)
    {
        Console.Error.WriteLine("nv_window_create failed");
        NvNative.nv_app_destroy(app);
        return 1;
    }

    NvNative.nv_on_ready(win, onReady, IntPtr.Zero);
    NvNative.nv_on_message(win, onMessage, IntPtr.Zero);
    NvNative.nv_window_on_close(win, onClose, app);

    NvNative.nv_window_show(win);
    NvNative.nv_app_run(app);
    NvNative.nv_app_destroy(app);
    return 0;
}
finally
{
    NvUtf8.Free(gTitle);
}

void OnReady(IntPtr win, IntPtr userdata)
{
    _ = userdata;
    var html = NvUtf8.Alloc(Html)!;
    var baseUrl = NvUtf8.Alloc("about:blank")!;
    try
    {
        NvNative.nv_load_html(win, html, baseUrl);

        var scriptPtrs = new IntPtr[]
        {
            NvUtf8.Alloc("document.title='csharp'")!,
            NvUtf8.Alloc("void 0")!,
        };
        var handle = GCHandle.Alloc(scriptPtrs, GCHandleType.Pinned);
        try
        {
            NvNative.nv_eval_js_batch(win, handle.AddrOfPinnedObject(), (UIntPtr)scriptPtrs.Length);
        }
        finally
        {
            handle.Free();
            foreach (var p in scriptPtrs)
                NvUtf8.Free(p);
        }

        var ev = NvUtf8.Alloc("ping")!;
        var json = NvUtf8.Alloc("""{"msg":"hello from C#"}""")!;
        try
        {
            NvNative.nv_send(win, ev, json);
        }
        finally
        {
            NvUtf8.Free(ev);
            NvUtf8.Free(json);
        }
    }
    finally
    {
        NvUtf8.Free(html);
        NvUtf8.Free(baseUrl);
    }
}

void OnMessage(IntPtr win, IntPtr eventName, IntPtr json, IntPtr userdata)
{
    _ = win; _ = eventName; _ = json; _ = userdata;
}

void OnClose(IntPtr win, IntPtr userdata)
{
    _ = win;
    NvNative.nv_app_quit(userdata);
}
