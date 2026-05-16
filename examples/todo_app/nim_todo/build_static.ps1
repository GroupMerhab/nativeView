$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$HostArch = if ($env:PROCESSOR_ARCHITECTURE) { $env:PROCESSOR_ARCHITECTURE } else { "AMD64" }
$VsPlatform = if ($env:NV_CMAKE_PLATFORM) {
  $env:NV_CMAKE_PLATFORM
} else {
  switch -Regex ($HostArch.ToUpper()) {
    "ARM64" { "ARM64" }
    "AMD64|X64" { "x64" }
    default { "Win32" }
  }
}
$Arch = switch -Regex ($VsPlatform.ToLower()) { "arm64" { "arm64" } "win32|x86" { "x86" } default { "x64" } }
$BuildDir = if ($env:NV_CMAKE_BUILD_DIR) { $env:NV_CMAKE_BUILD_DIR } else { Join-Path $RepoRoot ("build-nim-todo-static-" + $Arch) }
$CMakeConfig = if ($env:NV_CMAKE_CONFIG) { $env:NV_CMAKE_CONFIG } else { "Release" }

$GenDir = Join-Path $PSScriptRoot "generated"
$OutHtmlNim = Join-Path $GenDir "todo_html_embed.nim"
New-Item -ItemType Directory -Force -Path $GenDir | Out-Null

$nimble = if ($env:NIMBLE) { $env:NIMBLE } else { "nimble" }
& $nimble install -y db_connector
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (($env:NV_TODO_SKIP_UI_BUILD -eq "ON") -or ($env:NV_TODO_SKIP_UI_BUILD -eq "1")) {
  $HtmlIn = (Resolve-Path (Join-Path $PSScriptRoot "..\ui\fallback_index.html")).Path
} else {
  $UiRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\ui")).Path
  Push-Location $UiRoot
  try {
    & npm install
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & npm run build
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  } finally {
    Pop-Location
  }
  $HtmlIn = (Resolve-Path (Join-Path $UiRoot "dist\index.html")).Path
}

$python = if ($env:PYTHON) { $env:PYTHON } else { "python" }
& $python (Join-Path $PSScriptRoot "tools\embed_html_nim.py") $HtmlIn $OutHtmlNim "Embedded Vue todo UI (see examples/todo_app/ui)"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake -S $RepoRoot -B $BuildDir -A $VsPlatform -DNV_BUILD_SHARED=OFF -DNV_BUILD_TESTS=OFF -DNV_ENFORCE_FILE_LIMITS=OFF -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $BuildDir --config $CMakeConfig --target nv-core nv-ipc nv-ops nv-runtime nv-platform-win -j 8
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$SqliteBuildDir = Join-Path $BuildDir "sqlite"
cmake -S (Join-Path $RepoRoot "sqlite") -B $SqliteBuildDir -A $VsPlatform -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $SqliteBuildDir --config $CMakeConfig --target SQLite3 -j 8
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$SqliteLib = Join-Path $SqliteBuildDir "$CMakeConfig\sqlite3.lib"
if (-not (Test-Path $SqliteLib)) {
  $found = Get-ChildItem -Path $SqliteBuildDir -Recurse -Filter "sqlite3.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($found) {
    $SqliteLib = $found.FullName
  } else {
    throw "sqlite3.lib not found under $SqliteBuildDir"
  }
}

$NvRuntime = Join-Path $BuildDir "modules\nv-runtime\$CMakeConfig\nv-runtime.lib"
$NvPlat = Join-Path $BuildDir "modules\nv-platform-win\$CMakeConfig\nv-platform-win.lib"
$NvOps = Join-Path $BuildDir "modules\nv-ops\$CMakeConfig\nv-ops.lib"
$NvIpc = Join-Path $BuildDir "modules\nv-ipc\$CMakeConfig\nv-ipc.lib"
$NvCore = Join-Path $BuildDir "modules\nv-core\$CMakeConfig\nv-core.lib"
$WebView2LoaderStatic = Join-Path $BuildDir "_deps\webview2-sdk\build\native\$Arch\WebView2LoaderStatic.lib"
if (-not (Test-Path $WebView2LoaderStatic)) {
  $found = Get-ChildItem -Path (Join-Path $BuildDir "_deps") -Recurse -Filter "WebView2LoaderStatic.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($found) {
    $WebView2LoaderStatic = $found.FullName
  } else {
    throw "WebView2LoaderStatic.lib not found under $BuildDir\_deps"
  }
}
$WebView2LibDir = Split-Path $WebView2LoaderStatic -Parent

$PassL = @(
  "/link",
  "/WHOLEARCHIVE:$NvRuntime",
  "/WHOLEARCHIVE:$NvPlat",
  "/WHOLEARCHIVE:$NvOps",
  "/WHOLEARCHIVE:$NvIpc",
  "/WHOLEARCHIVE:$NvCore",
  "/LIBPATH:$WebView2LibDir",
  "WebView2LoaderStatic.lib",
  $SqliteLib,
  "windowsapp.lib", "user32.lib", "gdi32.lib", "advapi32.lib", "ole32.lib", "oleaut32.lib", "shell32.lib", "uuid.lib",
  "runtimeobject.lib", "propsys.lib", "shlwapi.lib", "gdiplus.lib"
)

$nim = if ($env:NIM) { $env:NIM } else { "nim" }
$bindPath = Join-Path $RepoRoot "bindings\nim"
$sqliteInclude = Join-Path $RepoRoot "sqlite"
$outExe = Join-Path $PSScriptRoot "nim_todo.exe"
$passLArgs = $PassL | ForEach-Object { "--passL:$_" }
$NimCpu = switch ($Arch) { "arm64" { "arm64" } "x86" { "i386" } default { "amd64" } }
$VcVarsArch = switch ($Arch) { "arm64" { "arm64" } "x86" { "x86" } default { "amd64" } }

$ccFile = Get-ChildItem -Path (Join-Path $BuildDir "CMakeFiles") -Recurse -Filter "CMakeCCompiler.cmake" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $ccFile) { throw "CMakeCCompiler.cmake not found under $BuildDir\CMakeFiles" }
$ccLine = Get-Content $ccFile.FullName | Select-Object -First 1
if ($ccLine -notmatch 'set\(CMAKE_C_COMPILER "([^"]+)"\)') { throw "Failed to parse CMAKE_C_COMPILER from $($ccFile.FullName)" }
$clExe = $Matches[1]
$vcDir = Split-Path $clExe -Parent
while ($vcDir -and (Split-Path $vcDir -Leaf) -ne "VC") { $vcDir = Split-Path $vcDir -Parent }
$vcDir = if ($vcDir) { $vcDir } else { throw "Failed to locate Visual Studio VC directory from $clExe" }
$vcvarsall = Join-Path $vcDir "Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvarsall)) { throw "vcvarsall.bat not found at $vcvarsall" }

Push-Location $PSScriptRoot
try {
  $nimArgs = @(
    "c",
    "-d:release",
    "--opt:size",
    "--cc:vcc",
    "--cpu:$NimCpu",
    "--passC:/MD",
    "--passC:/I$sqliteInclude",
    "--path:$bindPath"
  ) + $passLArgs + @(
    "-o:$outExe",
    "todo_app.nim"
  )
  $nimCmdParts = @($nim) + $nimArgs
  $nimCmdLine = ($nimCmdParts | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
  & cmd /d /s /c ('"' + $vcvarsall + '" ' + $VcVarsArch + ' >nul && ' + $nimCmdLine)
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Pop-Location
}

Write-Host "Built: $outExe"
