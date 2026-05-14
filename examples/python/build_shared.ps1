# Build nativeview_shared and run the Python minimal sample (ctypes).
# Run from examples/python in PowerShell. Requires: CMake, MSVC toolchain, Python 3.10+.
$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = if ($env:NV_CMAKE_BUILD_DIR) { $env:NV_CMAKE_BUILD_DIR } else { Join-Path $RepoRoot "build-python-shared" }

cmake -S $RepoRoot -B $BuildDir -DNV_BUILD_SHARED=ON -DNV_BUILD_TESTS=OFF -DNV_ENFORCE_FILE_LIMITS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --target nativeview_shared -j 8

$dll = Get-ChildItem -Path $BuildDir -Recurse -Filter "nativeview.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $dll) {
  throw "nativeview.dll not found under $BuildDir"
}

$env:NATIVEVIEW_LIB = $dll.FullName
$bindingsPy = Join-Path $RepoRoot "bindings\python"
$env:PYTHONPATH = if ($env:PYTHONPATH) { "$bindingsPy;$env:PYTHONPATH" } else { $bindingsPy }

Write-Host "Using NATIVEVIEW_LIB=$($env:NATIVEVIEW_LIB)"
$py = if ($env:PYTHON) { $env:PYTHON } else { "python" }
& $py (Join-Path $PSScriptRoot "minimal.py")
