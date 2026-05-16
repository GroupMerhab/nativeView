# Build csharp_todo: optional Vue UI, embed HTML, static-link nativeview + .NET publish.
# Run from examples/todo_app/csharp_todo in PowerShell.
#
# Env:
#   NV_TODO_SKIP_UI_BUILD=ON  — embed ../ui/fallback_index.html only (no npm)
#   NV_CMAKE_BUILD_DIR, NV_CMAKE_CONFIG, NV_CMAKE_PLATFORM — same as nim_todo
#
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tools\DotNet.ps1")
$DotNet = Resolve-DotNetExe
$DotNetDir = Split-Path $DotNet -Parent
if ($env:PATH -notlike "*$DotNetDir*") {
  $env:PATH = "$DotNetDir;$env:PATH"
}

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
$BuildDir = if ($env:NV_CMAKE_BUILD_DIR) { $env:NV_CMAKE_BUILD_DIR } else { Join-Path $RepoRoot ("build-csharp-todo-static-" + $Arch) }
$CMakeConfig = if ($env:NV_CMAKE_CONFIG) { $env:NV_CMAKE_CONFIG } else { "Release" }

$GenDir = Join-Path $PSScriptRoot "generated"
$OutHtmlCs = Join-Path $GenDir "TodoHtmlEmbed.cs"
New-Item -ItemType Directory -Force -Path $GenDir | Out-Null

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
& $python (Join-Path $PSScriptRoot "tools\embed_html_cs.py") $HtmlIn $OutHtmlCs "Embedded Vue todo UI (see examples/todo_app/ui)"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake -S $RepoRoot -B $BuildDir -A $VsPlatform -DNV_BUILD_SHARED=OFF -DNV_BUILD_TESTS=OFF -DNV_ENFORCE_FILE_LIMITS=OFF -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $BuildDir --config $CMakeConfig --target nv-core nv-ipc nv-ops nv-runtime nv-platform-win -j 8
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$NvRuntime = Join-Path $BuildDir "modules\nv-runtime\$CMakeConfig\nv-runtime.lib"
$NvPlat = Join-Path $BuildDir "modules\nv-platform-win\$CMakeConfig\nv-platform-win.lib"
$NvOps = Join-Path $BuildDir "modules\nv-ops\$CMakeConfig\nv-ops.lib"
$NvIpc = Join-Path $BuildDir "modules\nv-ipc\$CMakeConfig\nv-ipc.lib"
$NvCore = Join-Path $BuildDir "modules\nv-core\$CMakeConfig\nv-core.lib"

$WebView2LoaderStatic = Join-Path $BuildDir "_deps\webview2-sdk\build\native\$Arch\WebView2LoaderStatic.lib"
if (-not (Test-Path $WebView2LoaderStatic)) {
  $found = Get-ChildItem -Path (Join-Path $BuildDir "_deps") -Recurse -Filter "WebView2LoaderStatic.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($found) { $WebView2LoaderStatic = $found.FullName }
  else { throw "WebView2LoaderStatic.lib not found under $BuildDir\_deps" }
}
$WebView2LibDir = Split-Path $WebView2LoaderStatic -Parent
$HostDef = Join-Path $RepoRoot "bindings\csharp\nativeview_host.def"
if (-not (Test-Path $HostDef)) { throw "Missing $HostDef (required for Windows static P/Invoke exports)" }

$PropsPath = Join-Path $PSScriptRoot "NativeView.Static.props"
@(
  '<?xml version="1.0" encoding="utf-8"?>',
  '<Project>',
  '  <PropertyGroup>',
  "    <NV_CMAKE_BUILD>$BuildDir</NV_CMAKE_BUILD>",
  "    <NativeViewCMakeConfig>$CMakeConfig</NativeViewCMakeConfig>",
  '  </PropertyGroup>',
  '  <ItemGroup>',
  "    <NativeLibrary Include=`"$NvRuntime`" />",
  "    <NativeLibrary Include=`"$NvPlat`" />",
  "    <NativeLibrary Include=`"$NvOps`" />",
  "    <NativeLibrary Include=`"$NvIpc`" />",
  "    <NativeLibrary Include=`"$NvCore`" />",
  "    <LinkerArg Include=`"/WHOLEARCHIVE:$NvRuntime`" />",
  "    <LinkerArg Include=`"/WHOLEARCHIVE:$NvPlat`" />",
  "    <LinkerArg Include=`"/WHOLEARCHIVE:$NvOps`" />",
  "    <LinkerArg Include=`"/WHOLEARCHIVE:$NvIpc`" />",
  "    <LinkerArg Include=`"/WHOLEARCHIVE:$NvCore`" />",
  "    <LinkerArg Include=`"/DEF:$HostDef`" />",
  "    <LinkerArg Include=`"/LIBPATH:$WebView2LibDir`" />",
  '    <LinkerArg Include="WebView2LoaderStatic.lib" />',
  '    <LinkerArg Include="windowsapp.lib" />',
  '    <LinkerArg Include="user32.lib" />',
  '    <LinkerArg Include="gdi32.lib" />',
  '    <LinkerArg Include="advapi32.lib" />',
  '    <LinkerArg Include="ole32.lib" />',
  '    <LinkerArg Include="oleaut32.lib" />',
  '    <LinkerArg Include="shell32.lib" />',
  '    <LinkerArg Include="uuid.lib" />',
  '    <LinkerArg Include="runtimeobject.lib" />',
  '    <LinkerArg Include="propsys.lib" />',
  '    <LinkerArg Include="shlwapi.lib" />',
  '    <LinkerArg Include="gdiplus.lib" />',
  '  </ItemGroup>',
  '</Project>'
) | Set-Content -Path $PropsPath -Encoding UTF8

$Rid = switch ($Arch) { "arm64" { "win-arm64" } "x86" { "win-x86" } default { "win-x64" } }

Push-Location $PSScriptRoot
try {
  & $DotNet publish TodoApp.csproj -c Release -r $Rid --self-contained false
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Pop-Location
}

$PublishDir = Join-Path $PSScriptRoot "bin\Release\net8.0\$Rid\publish"
$OutExe = Join-Path $PublishDir "csharp_todo.exe"
if (Test-Path $PublishDir) {
  # Framework-dependent publish: apphost .exe needs csharp_todo.dll and dependency DLLs beside it.
  Copy-Item -Path (Join-Path $PublishDir '*') -Destination $PSScriptRoot -Recurse -Force
  Write-Host "Built: $(Join-Path $PSScriptRoot 'csharp_todo.exe') (and publish dependencies in this folder)"
} elseif (Test-Path $OutExe) {
  Write-Host "Built under: $PublishDir (publish folder missing; run dotnet publish again)"
} else {
  Write-Host "Built under: $(Join-Path $PSScriptRoot "bin\Release\net8.0\$Rid")"
}
