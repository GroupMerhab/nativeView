# C# — nativeview

## Known limitations

| Sample | Status |
|--------|--------|
| **`examples/csharp/Minimal/`** | Experimental; static linking on Windows needs **`nativeview_host.def`** (`/DEF:`). Not CI-guaranteed on every platform. |
| **`examples/todo_app/csharp_todo/`** | **Not working end-to-end yet** — Vue + SQLite todo port; Windows static P/Invoke, full publish output, and RID/arch alignment are still unresolved. Do not expect a runnable GUI until this is fixed. |

The P/Invoke binding (**`bindings/csharp/NativeView.cs`**) and build scripts are present for early adopters; report issues against the repo if you hit `EntryPointNotFoundException`, missing **`csharp_todo.dll`**, or a blank window after a successful build.

**macOS:** Static linking with the archive order and frameworks used in **`examples/csharp/build_static.sh`** matches the C examples and is the normal path for the minimal sample. **Optional:** define **`NATIVEVIEW_SHARED`** and load **`libnativeview.dylib`** if you prefer a shared library (see below).

This guide is for **C#** embedders using **`bindings/csharp/NativeView.cs`**, which mirrors the public C API in `include/nv.h` and `include/nv_hotkey.h`, plus `nv_version_string`, `nv_get_version_info`, and `nv_bench_now` from `include/nv_util.h` (same surface as **`bindings/nim/nativeview.nim`**).

## What you need

- A **built** nativeview library for your platform (root `CMakeLists.txt`, `AGENTS.md`). On Windows, the WebView2 runtime must be present. If CMake’s build directory lives inside the repo tree, you may need **`-DNV_ENFORCE_FILE_LIMITS=OFF`** during development (the **`examples/csharp/build_static.*`** scripts pass this).
- **.NET 8 SDK** (or retarget `NativeView.csproj` to `net6.0`+). Use the same **CPU** and **native toolchain** as the `.lib` / `.a` files you link (MSVC-built archives with MSVC-linked apps).

## Project reference

```xml
<ItemGroup>
  <ProjectReference Include="path/to/bindings/csharp/NativeView.csproj" />
</ItemGroup>
```

## Static linking (default for this repo’s docs and scripts)

1. Configure CMake with **`-DNV_BUILD_SHARED=OFF`** (default) and build the static module archives (`nv-core`, `nv-ipc`, `nv-ops`, `nv-runtime`, `nv-platform-*`).

2. Do **not** define **`NATIVEVIEW_SHARED`**. Imports use the logical name `nativeview_static`, resolved via **`NativeLibrary.SetDllImportResolver`** into the main executable (symbols must come from **`NativeLibrary`** / **`LinkerArg`** items on your app project).

3. **GNU ld / MinGW:** wrap the five in-tree archives in **`-Wl,--start-group`** … **`-Wl,--end-group`** (see **`examples/csharp/build_static.sh`**).

4. **MSVC:** use **`/WHOLEARCHIVE:`** per archive when needed (see **`examples/csharp/build_static.ps1`** and the `nativeview_shared` block in the root `CMakeLists.txt`). Link **`bindings/csharp/nativeview_host.def`** (`/DEF:…`) so `nv_*` symbols are exported from your `.exe` for P/Invoke (static `.lib` code is linked in but not visible to `GetProcAddress` without this). Add WebView2 + Win32 system libraries. Do **not** define **`NV_SHARED`** for a fully static consumer that never compiles `nv.h` in a separate C translation unit.

5. **Linux:** after the archive group, append **`pkg-config --libs webkit2gtk-4.1`** and optional indicator/notify/x11 packages (mirror `modules/nv-platform-linux/CMakeLists.txt` or **`examples/nim/build_static.sh`**).

6. **macOS:** archive order matches the C **`hello`** target / **`examples/pascal/build_static_example.sh`**. Add **`-framework Carbon -framework Cocoa -framework CoreServices -framework WebKit -framework UserNotifications -lobjc`**.

7. Template: **`bindings/csharp/NativeView.Static.props.example`** — copy to **`NativeView.Static.props`**, set **`NV_CMAKE_BUILD`**, import from your app `.csproj`.

## Shared library (optional)

1. Configure CMake with **`-DNV_BUILD_SHARED=ON`** and build **`nativeview_shared`** (`nativeview.dll`, `libnativeview.so`, `libnativeview.dylib`).

2. Define **`NATIVEVIEW_SHARED`** when compiling the binding (e.g. **`-p:DefineConstants=NATIVEVIEW_SHARED`** or `<DefineConstants>` in the project).

3. **Windows:** keep **`nativeview.dll`** on **`PATH`** or next to your `.exe`.

4. If you compile **C** sources that include **`include/nv.h`** with MSVC, define **`NV_SHARED`** where required (see `include/util/nv_log.h`).

## Callbacks, GC, and string lifetimes

- Register only **`[UnmanagedFunctionPointer(CallingConvention.Cdecl)]`** delegates stored in **static fields** (or otherwise kept alive) for the whole registration window. The GC must not collect a delegate while native code might call it.

- **`userdata`:** pass an **`IntPtr`** to pinned or native memory. Do not pass a pointer to a stack-only value unless it outlives every callback.

- **`nv_on_message`:** `event` and `json` pointers are valid **only inside the callback**. Copy with **`NvUtf8.PtrToString`** if needed later.

- **`NvWindowCfg.Title`:** must remain valid through **`nv_window_create`** (see `nv.h`). Use **`NvUtf8.Alloc`** and **`NvUtf8.Free`** after create, or a static **`IntPtr`** to immortal UTF-8.

## `nv_eval_js_batch`

The C type is `const char **scripts`. Build a contiguous array of **`IntPtr`** (UTF-8 null-terminated), pin it with **`GCHandle.Alloc(..., GCHandleType.Pinned)`**, pass **`handle.AddrOfPinnedObject()`** plus **`(UIntPtr)count`**. Each script pointer and the array must stay valid until the call returns.

## Mapping defines (summary)

| Build | C# define | C headers (separate C TU with MSVC) |
|--------|-----------|-------------------------------------|
| Shared `nativeview` DLL/SO/dylib | **`NATIVEVIEW_SHARED`** | **`NV_SHARED`** |
| Static `.a` / `.lib` chain | *(none)* | *(none)* |

## Example layout in this repo

- **`bindings/csharp/NativeView.cs`** — P/Invoke module.
- **`bindings/csharp/NativeView.csproj`** — class library (not published to NuGet).
- **`examples/csharp/Minimal/`** — minimal sample (`nv_on_ready`, `nv_load_html`, `nv_eval_js_batch`, teardown).
- **`examples/todo_app/csharp_todo/`** — full todo port (**not working yet**; see [Known limitations](#known-limitations)).
- **`examples/csharp/build_static.sh`** / **`build_static.ps1`** — CMake static build + `dotnet publish` with native link flags.

## Troubleshooting

- **`DllNotFoundException` / `EntryPointNotFoundException` for `nv_*` on Windows (static):** native code is linked but not **exported** from the `.exe` — add **`/DEF:bindings/csharp/nativeview_host.def`** (the repo build scripts do this). Wrong static link order or missing **`/WHOLEARCHIVE:`** can also drop symbols. For a shared build, define **`NATIVEVIEW_SHARED`** and ship **`nativeview.dll`** next to the app.

- **Crash in callback:** collected delegate, invalid **`userdata`**, or freed UTF-8 backing **`NvWindowCfg.Title`**.

- **Blank window:** load URL/HTML after **`nv_on_ready`**; verify WebView2 / WebKitGTK on the host.

## See also

- `include/nv.h` — authoritative C API
- `docs/Nim.md` — parallel embedding guide (link order and lifetimes match)
- `docs/Implementation.md` — `NV_BUILD_SHARED` and layout pointers
