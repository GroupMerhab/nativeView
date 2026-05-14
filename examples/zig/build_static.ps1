# Static-link nativeview .lib archives into the Zig minimal sample.
# Run from examples/zig in PowerShell. Requires: CMake, MSVC-built nativeview, Zig (MSVC target).
#
# Fill WebView2LoaderStatic.lib path after configure (see cmake/FetchWebView2.cmake output).

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = if ($env:NV_CMAKE_BUILD_DIR) { $env:NV_CMAKE_BUILD_DIR } else { Join-Path $RepoRoot "build-zig-static" }
$Zig = if ($env:ZIG) { $env:ZIG } else { "zig" }

cmake -S $RepoRoot -B $BuildDir -DNV_BUILD_SHARED=OFF -DNV_BUILD_TESTS=OFF -DNV_ENFORCE_FILE_LIMITS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --target nv-core nv-ipc nv-ops nv-runtime nv-platform-win -j 8

$NvRuntime = Join-Path $BuildDir "modules\nv-runtime\nv-runtime.lib"
$NvPlat = Join-Path $BuildDir "modules\nv-platform-win\nv-platform-win.lib"
$NvOps = Join-Path $BuildDir "modules\nv-ops\nv-ops.lib"
$NvIpc = Join-Path $BuildDir "modules\nv-ipc\nv-ipc.lib"
$NvCore = Join-Path $BuildDir "modules\nv-core\nv-core.lib"

$WebView2LoaderStatic = Join-Path $BuildDir "WebView2LoaderStatic.lib"
if (-not (Test-Path $WebView2LoaderStatic)) {
  Write-Warning "WebView2LoaderStatic.lib not found at $WebView2LoaderStatic — adjust path, then re-run."
}

$NvMod = Join-Path $RepoRoot "bindings\zig\nativeview.zig"
$Minimal = Join-Path $PSScriptRoot "minimal.zig"

Push-Location $PSScriptRoot
try {
  & $Zig @(
    "build-exe",
    "--dep", "nativeview",
    "-Mroot=$Minimal",
    "-Mnativeview=$NvMod",
    "-target", "x86_64-windows-msvc",
    "-OReleaseSmall",
    "/WHOLEARCHIVE:$NvRuntime",
    "/WHOLEARCHIVE:$NvPlat",
    "/WHOLEARCHIVE:$NvOps",
    "/WHOLEARCHIVE:$NvIpc",
    "/WHOLEARCHIVE:$NvCore",
    "/DEFAULTLIB:$WebView2LoaderStatic",
    "user32.lib", "ole32.lib", "shell32.lib", "uuid.lib", "runtimeobject.lib", "propsys.lib", "shlwapi.lib", "gdiplus.lib"
  )
} finally {
  Pop-Location
}

Write-Host "Built: $(Join-Path $PSScriptRoot 'minimal.exe') (adjust WebView2 path if link failed)"
