# Static-link nativeview .lib archives into the Nim minimal sample (no -d:nativeviewShared).
# Run from examples/nim in PowerShell. Requires: CMake, MSVC-built nativeview, Nim (MSVC backend).
#
# Fill $BuildDir after configuring:
#   cmake -S ..\.. -B ..\..\build-nim-static -DNV_BUILD_SHARED=OFF -DNV_BUILD_TESTS=OFF
#   cmake --build ..\..\build-nim-static --target nv-core nv-ipc nv-ops nv-runtime nv-platform-win -j
#
# WebView2LoaderStatic.lib path: under your build tree (see cmake/FetchWebView2.cmake output).

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = if ($env:NV_CMAKE_BUILD_DIR) { $env:NV_CMAKE_BUILD_DIR } else { Join-Path $RepoRoot "build-nim-static" }

cmake -S $RepoRoot -B $BuildDir -DNV_BUILD_SHARED=OFF -DNV_BUILD_TESTS=OFF -DNV_ENFORCE_FILE_LIMITS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --target nv-core nv-ipc nv-ops nv-runtime nv-platform-win -j 8

$NvRuntime = Join-Path $BuildDir "modules\nv-runtime\nv-runtime.lib"
$NvPlat = Join-Path $BuildDir "modules\nv-platform-win\nv-platform-win.lib"
$NvOps = Join-Path $BuildDir "modules\nv-ops\nv-ops.lib"
$NvIpc = Join-Path $BuildDir "modules\nv-ipc\nv-ipc.lib"
$NvCore = Join-Path $BuildDir "modules\nv-core\nv-core.lib"

# TODO: set to the WebView2 static loader .lib from your build (search build dir for WebView2LoaderStatic.lib).
$WebView2LoaderStatic = Join-Path $BuildDir "WebView2LoaderStatic.lib"
if (-not (Test-Path $WebView2LoaderStatic)) {
  Write-Warning "WebView2LoaderStatic.lib not found at $WebView2LoaderStatic — adjust path, then re-run."
}

$PassL = @(
  "/WHOLEARCHIVE:$NvRuntime",
  "/WHOLEARCHIVE:$NvPlat",
  "/WHOLEARCHIVE:$NvOps",
  "/WHOLEARCHIVE:$NvIpc",
  "/WHOLEARCHIVE:$NvCore",
  "/DEFAULTLIB:$WebView2LoaderStatic",
  "user32.lib", "ole32.lib", "shell32.lib", "uuid.lib", "runtimeobject.lib", "propsys.lib", "shlwapi.lib", "gdiplus.lib"
)

$nim = if ($env:NIM) { $env:NIM } else { "nim" }
$bindPath = Join-Path $RepoRoot "bindings\nim"
$passLArgs = $PassL | ForEach-Object { "--passL:$_" }
Push-Location $PSScriptRoot
try {
  $nimArgs = @("c", "-d:release", "--path:$bindPath") + $passLArgs + @("minimal.nim")
  & $nim @nimArgs
} finally {
  Pop-Location
}

Write-Host "Built: $(Join-Path $PSScriptRoot 'minimal.exe') (adjust WebView2 path if link failed)"
