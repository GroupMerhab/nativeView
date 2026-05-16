# Todo app (C# + nativeview)

> **Status:** This example is **not working end-to-end yet** (especially on **Windows**: static P/Invoke, publish layout, RID/arch mismatch, and WebView2 integration are still being debugged). The build scripts and bindings are in flux; treat this folder as **work in progress**, not a supported demo. See [../../../docs/CSharp.md](../../../docs/CSharp.md#known-limitations).

C# port of [../c_todo](../c_todo) and [../nim_todo](../nim_todo): same SQLite schema and Vue `__nv` bridge. The window uses [../../../bindings/csharp](../../../bindings/csharp) (P/Invoke) with **static** nativeview archives — see [../../../docs/CSharp.md](../../../docs/CSharp.md). Persistence uses [Microsoft.Data.Sqlite](https://www.nuget.org/packages/Microsoft.Data.Sqlite).

## Prerequisites

- [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0) or newer (`dotnet` on PATH, or installed under `C:\Program Files\dotnet`)
- CMake + MSVC (Windows), for static nativeview
- Node.js + npm (optional; skip with `NV_TODO_SKIP_UI_BUILD=ON`)

If `dotnet` is not on PATH, set:

```bat
set DOTNET_EXE=C:\Program Files\dotnet\dotnet.exe
```

## Build

From this directory:

**Windows (cmd or PowerShell):**

```bat
build_static.bat
```

```powershell
.\build_static.ps1
```

**Linux / macOS:**

```bash
./build_static.sh
```

- **Full UI**: runs `npm install` + `npm run build` in [../ui](../ui), embeds `dist/index.html`, configures CMake for static nativeview, publishes **`csharp_todo`**.
- **Skip npm**: `NV_TODO_SKIP_UI_BUILD=ON ./build_static.sh` (or `$env:NV_TODO_SKIP_UI_BUILD='ON'; .\build_static.ps1`) embeds [../ui/fallback_index.html](../ui/fallback_index.html) only.

Optional DB path: first CLI argument (default `./todo_app.db`).

After `build_static.*`, run from **this directory** (the build copies the full `dotnet publish` output here, not just the apphost):

```bash
./csharp_todo
./csharp_todo /path/to/my.db
```

```powershell
.\csharp_todo.exe
```

If you see **`EntryPointNotFoundException` for `nv_app_create`**, rebuild with `.\build_static.ps1` so **`nativeview_host.def`** is linked (`/DEF:` exports `nv_*` from the `.exe` on Windows). A manual `dotnet publish` without that `.def` will fail at runtime.

If you see *The application to execute does not exist: … csharp_todo.dll*, you are running a lone `.exe` without the publish folder — re-run the build script, or run:

```powershell
.\bin\Release\net8.0\win-arm64\publish\csharp_todo.exe
```

(use `win-x64` / `win-x86` instead of `win-arm64` when that matches your build)

## Tests

```bash
./run_tests.sh
```

```bat
run_tests.bat
```

```powershell
.\run_tests.ps1
```

## HTML embed

[tools/embed_html_cs.py](tools/embed_html_cs.py) writes [generated/TodoHtmlEmbed.cs](generated/TodoHtmlEmbed.cs) (`TodoHtmlEmbed.Data` for `nv_load_html_ref`). The C example’s [../c_todo/tools/embed_html.py](../c_todo/tools/embed_html.py) is **not** modified.

## Windows

`build_static.ps1` static-links nativeview the same way as [../nim_todo](../nim_todo) (archives + WebView2). CMake build dir default: `build-csharp-todo-static-<arch>` at the repo root.
