# C# binding — nativeview

Low-level **P/Invoke** for the public C API in [`include/nv.h`](../../include/nv.h) — same surface as [`bindings/nim/nativeview.nim`](../nim/nativeview.nim) (app/window lifecycle, load/eval/send, hotkeys, version helpers). Not a high-level UI framework; you call `NvNative.*` directly and own linking.

| Artifact | Role |
|----------|------|
| **`NativeView.cs`** | `NvNative` imports, structs, delegates, `NvUtf8` helpers |
| **`NativeView.csproj`** | Class library (`net8.0`, not published to NuGet) |
| **`NativeView.Static.props.example`** | MSBuild template for static archive linking |

**Full guide:** [docs/CSharp.md](../../docs/CSharp.md) · **Examples:** [examples/csharp/](../../examples/csharp/)

---

## Requirements

- **.NET 8 SDK** (or retarget `NativeView.csproj` to `net6.0`+)
- **CMake-built nativeview** for your OS ([getting-started.md](../../docs/getting-started.md))
- **Windows:** WebView2 runtime; MSVC-built `.lib` files when static-linking
- Same **CPU / toolchain** as the native archives you link (do not mix MinGW `.a` with MSVC `dotnet publish` on Windows)

---

## Add to your app

```xml
<ItemGroup>
  <ProjectReference Include="path/to/nativeView/bindings/csharp/NativeView.csproj" />
</ItemGroup>
```

Import static link props from your executable project (generated or copied from the example template):

```xml
<Import Project="NativeView.Static.props" Condition="Exists('NativeView.Static.props')" />
```

Minimal flow (see [examples/csharp/Minimal/Program.cs](../../examples/csharp/Minimal/Program.cs)):

```csharp
using System.Runtime.InteropServices;
using NativeView;

IntPtr title = NvUtf8.Alloc("My App")!;
var cfg = new NvWindowCfg { Title = title, Width = 800, Height = 600, Resizable = 1 };

IntPtr app = NvNative.nv_app_create();
IntPtr win = NvNative.nv_window_create(app, ref cfg);
NvUtf8.Free(title);

NvReadyCb onReady = (w, _) => NvNative.nv_load_url(w, NvUtf8.Alloc("https://example.com")!);
NvNative.nv_on_ready(win, onReady, IntPtr.Zero);
NvNative.nv_window_show(win);
NvNative.nv_app_run(app);
NvNative.nv_app_destroy(app);
```

Keep **`onReady`** (and any `nv_on_message` / `nv_window_on_close` handlers) in **static fields** so the GC cannot collect delegates while native code holds their addresses.

---

## Linking modes

| Mode | Define | Native artifact | Who resolves symbols |
|------|--------|-----------------|----------------------|
| **Static** (repo default) | *(none)* | CMake module `.lib` / `.a` chain | `DllImport("nativeview_static")` + resolver → main executable; you pass archives via `NativeLibrary` / `LinkerArg` |
| **Shared** | `NATIVEVIEW_SHARED` | `nativeview.dll`, `libnativeview.so`, `libnativeview.dylib` | Standard `DllImport` load of the shared library |

### Static (recommended here)

1. `cmake -S . -B build -DNV_BUILD_SHARED=OFF` and build `nv-core`, `nv-ipc`, `nv-ops`, `nv-runtime`, `nv-platform-*`.
2. Do **not** define `NATIVEVIEW_SHARED`.
3. Link archives on the **app** project (not the binding DLL alone):
   - Copy **`NativeView.Static.props.example`** → **`NativeView.Static.props`**, set **`NV_CMAKE_BUILD`**, import from your `.csproj`, **or**
   - Run the repo scripts (they write `NativeView.Static.props` for you):
     - **Linux / macOS:** `examples/csharp/build_static.sh`
     - **Windows:** `examples/csharp/build_static.ps1`
4. Platform extras (WebView2 + Win32 libs, WebKitGTK `pkg-config`, macOS frameworks) are documented in [docs/CSharp.md](../../docs/CSharp.md) and mirrored in those scripts.

### Shared (optional)

```bash
cmake -S . -B build -DNV_BUILD_SHARED=ON
cmake --build build --target nativeview_shared
```

```bash
dotnet publish YourApp.csproj -c Release -p:DefineConstants=NATIVEVIEW_SHARED
```

Put **`nativeview.dll`** on `PATH` or beside the executable on Windows.

---

## API surface (`NativeView` namespace)

| Type | Purpose |
|------|---------|
| **`NvNative`** | All `nv_*` cdecl imports |
| **`NvWindowCfg`** | `nv_window_cfg_t` — set `Title` to a stable `IntPtr` (UTF-8) through `nv_window_create` |
| **`NvVersionInfo`** | Output of `nv_get_version_info` |
| **`NvMsgCb`**, **`NvReadyCb`**, **`NvCloseCb`** | Callback delegates |
| **`NvUtf8`** | `Alloc` / `Free` / `PtrToString` for C strings |
| **`NvNative.NvHotkey*`** | Hotkey result constants |

Covers: `nv_app_*`, `nv_window_*`, `nv_load_*`, `nv_eval_js` / `nv_eval_js_batch`, `nv_on_message` / `nv_on_ready`, `nv_send`, `nv_window_register_hotkey`, `nv_version_string`, `nv_get_version_info`, `nv_bench_now`.

Authoritative C definitions: [`include/nv.h`](../../include/nv.h), [`include/nv_hotkey.h`](../../include/nv_hotkey.h).

---

## Rules that bite embedders

1. **Delegates** — store callbacks in static fields (or pin them) for the entire time they are registered.
2. **`nv_on_message`** — copy `event` / `json` with `NvUtf8.PtrToString` inside the callback if you need them later.
3. **`NvWindowCfg.Title`** — must stay valid until `nv_window_create` returns (`NvUtf8.Alloc` or a static buffer).
4. **`nv_eval_js_batch`** — pin a `IntPtr[]` of script pointers with `GCHandle.Alloc(..., Pinned)`; see the minimal example.
5. **Static link** — linking only `NativeView.csproj` is not enough; the **executable** must pull in the nativeview archive chain.

---

## Verify the binding compiles

From this directory (no native link required):

```bash
dotnet build NativeView.csproj -c Release
```

End-to-end GUI samples: [examples/csharp/README.md](../../examples/csharp/README.md) (minimal, experimental). Full todo app: [examples/todo_app/csharp_todo/](../../examples/todo_app/csharp_todo/) — **not working yet** ([docs/CSharp.md](../../docs/CSharp.md#known-limitations)).

---

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| `DllNotFoundException` / missing `nv_*` at runtime | Archives not linked on the app, wrong link order, or missing `NATIVEVIEW_SHARED` when using the DLL |
| Crash in a callback | Collected delegate, bad `userdata`, or freed title string |
| Blank window | Loaded content before `nv_on_ready`; missing WebView2 / WebKitGTK on the machine |

More detail: [docs/CSharp.md](../../docs/CSharp.md) · Parallel notes: [docs/Nim.md](../../docs/Nim.md)

---

## License

Apache 2.0 — same as the nativeview project. See [LICENSE](../../LICENSE).
