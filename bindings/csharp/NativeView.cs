// Low-level C# P/Invoke for the nativeview public C API (include/nv.h, include/nv_hotkey.h,
// plus nv_version_string / nv_get_version_info / nv_bench_now from include/nv_util.h).
//
// Linking: see docs/CSharp.md. Default in this repo is static (NATIVEVIEW_SHARED not defined);
// define NATIVEVIEW_SHARED when loading nativeview.dll / libnativeview.so / libnativeview.dylib.
//
// SPDX-License-Identifier: Apache-2.0

using System.Runtime.InteropServices;

namespace NativeView;

/// <summary>Low-level cdecl imports mirroring <c>bindings/nim/nativeview.nim</c>.</summary>
public static class NvNative
{
#if NATIVEVIEW_SHARED
    private const string NvLib =
        OperatingSystem.IsWindows() ? "nativeview" :
        OperatingSystem.IsMacOS() ? "libnativeview.dylib" :
        "libnativeview.so";
#else
    /// <summary>Logical name; static builds resolve it to the main executable module.</summary>
    private const string NvLib = "nativeview_static";

    static NvNative()
    {
        NativeLibrary.SetDllImportResolver(
            typeof(NvNative).Assembly,
            static (libraryName, _, _) =>
                libraryName == NvLib ? NativeLibrary.GetMainProgramHandle() : IntPtr.Zero);
    }
#endif

    public const int NvHotkeyOk = 0;
    public const int NvHotkeyErrInvalid = -1;
    public const int NvHotkeyErrAlreadyExists = -2;
    public const int NvHotkeyErrNotSupported = -3;
    public const int NvHotkeyErrPlatform = -4;

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern IntPtr nv_app_create();

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_app_run(IntPtr app);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_app_quit(IntPtr app);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_app_destroy(IntPtr app);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern IntPtr nv_window_create(IntPtr app, ref NvWindowCfg config);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_close(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_destroy(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_show(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_hide(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_set_title(IntPtr window, IntPtr title);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_set_size(IntPtr window, int width, int height);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_set_min_size(IntPtr window, int width, int height);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_center(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_fullscreen(IntPtr window, int enable);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern int nv_window_is_fullscreen(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_minimize(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_maximize(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_restore(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_focus(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern int nv_window_is_focused(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_set_resizable(IntPtr window, int enable);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_set_always_on_top(IntPtr window, int enable);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_set_opacity(IntPtr window, int opacityPct);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_set_zoom_factor(IntPtr window, double factor);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_request_close(IntPtr window);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_on_close(IntPtr window, NvCloseCb? callback, IntPtr userdata);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_load_url(IntPtr window, IntPtr url);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_load_html(IntPtr window, IntPtr html, IntPtr baseUrl);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_load_html_ref(IntPtr window, IntPtr html, UIntPtr length, IntPtr baseUrl);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_eval_js(IntPtr window, IntPtr js);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_eval_js_batch(IntPtr window, IntPtr scripts, UIntPtr count);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_on_message(IntPtr window, NvMsgCb? callback, IntPtr userdata);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_on_ready(IntPtr window, NvReadyCb? callback, IntPtr userdata);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_send(IntPtr window, IntPtr eventName, IntPtr json);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern int nv_window_register_hotkey(IntPtr window, IntPtr id, IntPtr combo);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_window_unregister_hotkey(IntPtr window, IntPtr id);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern IntPtr nv_version_string();

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern void nv_get_version_info(ref NvVersionInfo outInfo);

    [DllImport(NvLib, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
    public static extern ulong nv_bench_now();
}

/// <summary>Matches C <c>nv_window_cfg_t</c> / Nim <c>NvWindowCfg</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NvWindowCfg
{
    public IntPtr Title;
    public int Width;
    public int Height;
    public int MinWidth;
    public int MinHeight;
    public int Resizable;
    public int Frameless;
    public int Transparent;
    public int Devtools;
    public int Modal;
}

/// <summary>Matches C <c>NvVersionInfo</c> in <c>include/nv_util.h</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NvVersionInfo
{
    public ushort Major;
    public ushort Minor;
    public ushort Patch;
    public IntPtr BuildSha;
}

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void NvMsgCb(IntPtr window, IntPtr eventName, IntPtr json, IntPtr userdata);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void NvReadyCb(IntPtr window, IntPtr userdata);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void NvCloseCb(IntPtr window, IntPtr userdata);

/// <summary>UTF-8 helpers for C string parameters (caller owns allocation lifetime).</summary>
public static class NvUtf8
{
    public static IntPtr Alloc(string? s)
    {
        if (s is null)
            return IntPtr.Zero;
        var bytes = System.Text.Encoding.UTF8.GetBytes(s + "\0");
        var ptr = Marshal.AllocHGlobal(bytes.Length);
        Marshal.Copy(bytes, 0, ptr, bytes.Length);
        return ptr;
    }

    public static void Free(IntPtr ptr)
    {
        if (ptr != IntPtr.Zero)
            Marshal.FreeHGlobal(ptr);
    }

    public static string? PtrToString(IntPtr ptr)
    {
        if (ptr == IntPtr.Zero)
            return null;
        var len = 0;
        while (Marshal.ReadByte(ptr, len) != 0)
            len++;
        if (len == 0)
            return string.Empty;
        var bytes = new byte[len];
        Marshal.Copy(ptr, bytes, 0, len);
        return System.Text.Encoding.UTF8.GetString(bytes);
    }
}
