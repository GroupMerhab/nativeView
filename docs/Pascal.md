# Free Pascal (FPC) and Lazarus — nativeview

This guide is for **Free Pascal** users who embed nativeview through the import unit
`bindings/pascal/nativeview.pas`, which mirrors the public C API in `include/nv.h` and
`include/nv_hotkey.h`.

## What you need

- A **built** nativeview library for your platform and ABI (see the root `CMakeLists.txt`
  and `AGENTS.md`). On Windows, the WebView2 runtime must be available on the machine
  (Evergreen is typical).
- **Free Pascal** 3.2+ (or **Lazarus** using the FPC backend). Use the same **CPU** and
  **calling convention** family as the C library you link (e.g. Win64 FPC with an x64
  `nativeview.dll` / import chain).

## Two ways to link

### 1. Shared library (DLL / `.so` / `.dylib`)

1. Configure CMake with `-DNV_BUILD_SHARED=ON` and build the `nativeview_shared` target.
   The produced binary is named `nativeview` (`nativeview.dll`, `libnativeview.so`, or
   `libnativeview.dylib`).

2. When **compiling Pascal** code that uses `nativeview.pas`, add **`NATIVEVIEW_SHARED`**
   so the unit resolves symbols against the shared library:
   - Command line: **`-dNATIVEVIEW_SHARED`**
   - Lazarus: **Project → Project Options → Compiler Options → Custom options**:
     `-dNATIVEVIEW_SHARED`

3. If you also compile **C** sources that include `include/nv.h`, define **`NV_SHARED`**
   on the C side so `NV_API` becomes `dllimport` on Windows (see `include/util/nv_log.h`).

4. **Windows:** keep `nativeview.dll` (and any loader DLLs you ship) on the **PATH** or
   next to your `.exe`.

### 2. Static linking

1. Build nativeview the usual way (static archives + platform stack). The CMake target
   **`nativeview`** is an **INTERFACE** target that groups those archives; mirror that
   link set (or use the same `.a` / `.lib` files your C examples use).

2. Do **not** define `NATIVEVIEW_SHARED`. Use the linker to pull in the static
   `nativeview` libraries **and** all **system** and **platform** dependencies
   (WebView2 import libs on Windows, WebKit/GTK on Linux, frameworks on macOS). The
   exact list matches your platform’s `nv-platform-*` `CMakeLists.txt`.

3. **MinGW FPC** and **MSVC-built** static `.lib` files are often incompatible; prefer
   building nativeview with the **same** toolchain you use to link the Pascal
   executable when possible.

## Application layout: “thin main” instead of one huge program file

FPC allows any number of **units**; the **program** (`.lpr` / `.pas` `program`) only needs
to be the **entry point**. A practical pattern for nativeview:

| File | Role |
|------|------|
| `myapp.lpr` | **Only** `uses myapp_main;` then `begin myapp_main.Run; end.` (or similar). |
| `myapp_main.pas` | **Unit** that implements `Run`, creates `nv_app_t` / `nv_window_t`, registers callbacks, calls `nv_app_run`, and tears down. |

**Why:** C callbacks (`nv_on_message`, `nv_on_ready`, etc.) must be **cdecl** procedures
with a **stable** address. They cannot be *nested* methods in the same way as Delphi
instance methods without extra shims. Keeping **one unit** (or a few) with **top-level**
`procedure ...; cdecl;` callbacks next to your app state is easy to audit and link.

**Alternatives (also valid):**

- **Single file:** one `program` that contains everything (works for tiny samples).
- **Lazarus:** a `.lpr` that calls into a **unit** (same as above); set linker options
  in **Project options** instead of a command line.
- **Library / plugin:** a `library` project that exports your own entry points and
  calls nativeview internally — not required for normal apps.

## Types, strings, and callbacks

- **Opaque handles** are `Pointer` (`PNVApp`, `PNVWindow` in the import unit).
- **Text** is **UTF-8** in C (`const char*`). In Pascal, use **UTF-8** stored in
  `UTF8String` / `RawByteString` and pass **`PAnsiChar(...)`** only while the string
  **remains valid** (see `nv.h`: some pointers are only valid for the duration of a
  callback). For `nv_window_cfg_t.title`, keep a **module-level** or **heap**-backed
  string for the whole time the window exists if the API does not copy it (title is
  documented as copied in `nv.h` for `nv_window_set_title`; for `nv_window_cfg_t` keep
  storage alive at least until `nv_window_create` returns).
- **Window config** `TNVWindowCfg` is packed to match C (`{$packrecords c}` in the unit).
- **Callbacks** must be declared **`cdecl`** and match the C typedefs in `nv.h` exactly.
- **`nv_eval_js_batch`:** the C type is `const char**`. Pass a **pointer to the first**
  `PAnsiChar` in an array of pointers (or `nil` if unused).

## Windows: console vs GUI

- Default FPC **console** programs attach a **console** window. For a “window-only” app you
  may use **`{$APPTYPE GUI}`** in the `.lpr` file so no extra console is shown (optional).
- You can still use `AllocConsole` / logging if you need a console for debugging.

## Mapping defines (summary)

| Build | Pascal define | C headers (if used) |
|--------|----------------|----------------------|
| Shared `nativeview` DLL/SO | **`NATIVEVIEW_SHARED`** | **`NV_SHARED`** |
| Static `.a` / `.lib` chain | *(none)* | *(none)* |

## Example layout in this repo

See **`examples/pascal/`**:

- **`nv_minimal.lpr`** — minimal program entry (recommended on Windows / Linux when linking works).
- **`nv_minimal_lib.lpr`**, **`nv_minimal_host.c`** — on **macOS**, use the **library + Clang `main`** flow from **`examples/pascal/build_example.sh`** to avoid AppKit faults with FPC’s default **`program`** startup (seen with both **dylib** and **static** links into nativeview on FPC 3.2 aarch64).
- **`examples/pascal/build_static_example.sh`** — links the **static** nativeview **`.a`** chain into **`nv_minimal_static`** without `NATIVEVIEW_SHARED` (mainly for Linux / experimentation).
- **`nv_minimal_app.pas`** — unit that owns **`RunNativeView`**, window creation, and cdecl callbacks.

Build instructions are in **`examples/pascal/README.md`**.

## Lazarus checklist

1. Add **`bindings/pascal/nativeview.pas`** to the project (or copy the unit into your
   tree and adjust path).
2. Add **`-dNATIVEVIEW_SHARED`** only when linking the **DLL**.
3. **Linker**: add import library / search path for `nativeview.dll`’s import lib on MSVC,
   or `-Fl` / `-Fu` and `-k***` options for static MinGW libraries (see FPC **Project
   options → Linking**).
4. **Run / debug:** ensure the **working directory** or **PATH** finds `nativeview.dll`
   and WebView2 bits on Windows.

## Troubleshooting

- **Undefined reference to `nv_*`:** wrong link order (static), missing whole-archive
  equivalent, or missing `NATIVEVIEW_SHARED` / wrong import lib for the DLL.
- **Crash in callback:** `PAnsiChar` points to a **freed** or **stack** string; or
  callback is not **cdecl** / wrong signature.
- **DLL loads but window is blank:** load URL/HTML after `nv_on_ready` or use
  `nv_load_url` / `nv_load_html` as in the C examples; check platform runtime (WebView2,
  WebKitGTK).

## See also

- `include/nv.h` — authoritative C API
- `docs/Implementation.md` — build options including `NV_BUILD_SHARED`
- `bindings/pascal/nativeview.pas` — Pascal declarations
