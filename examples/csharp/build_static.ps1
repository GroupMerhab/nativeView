# Static-link nativeview .lib archives into the C# minimal sample (no NATIVEVIEW_SHARED).
# Run from examples/csharp in PowerShell. Requires: CMake, MSVC-built nativeview, .NET 8 SDK.
#
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot

. (Join-Path $ScriptDir "tools\DotNet.ps1")
$DotNet = Resolve-DotNetExe
$DotNetDir = Split-Path $DotNet -Parent
if ($env:PATH -notlike "*$DotNetDir*") {
  $env:PATH = "$DotNetDir;$env:PATH"
}
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path
$BuildDir = if ($env:NV_CMAKE_BUILD_DIR) { $env:NV_CMAKE_BUILD_DIR } else { Join-Path $RepoRoot "build-csharp-static" }
$CMakeConfig = if ($env:NV_CMAKE_CONFIG) { $env:NV_CMAKE_CONFIG } else { "Release" }

cmake -S $RepoRoot -B $BuildDir -DNV_BUILD_SHARED=OFF -DNV_BUILD_TESTS=OFF -DNV_ENFORCE_FILE_LIMITS=OFF -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $BuildDir --config $CMakeConfig --target nv-core nv-ipc nv-ops nv-runtime nv-platform-win -j 8
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$NvRuntime = Join-Path $BuildDir "modules\nv-runtime\$CMakeConfig\nv-runtime.lib"
$NvPlat = Join-Path $BuildDir "modules\nv-platform-win\$CMakeConfig\nv-platform-win.lib"
$NvOps = Join-Path $BuildDir "modules\nv-ops\$CMakeConfig\nv-ops.lib"
$NvIpc = Join-Path $BuildDir "modules\nv-ipc\$CMakeConfig\nv-ipc.lib"
$NvCore = Join-Path $BuildDir "modules\nv-core\$CMakeConfig\nv-core.lib"

$ArchRaw = if (Test-Path (Join-Path $BuildDir "ARM64")) { "arm64" }
  elseif (Test-Path (Join-Path $BuildDir "x64")) { "x64" }
  elseif (Test-Path (Join-Path $BuildDir "Win32")) { "x86" }
  elseif ($env:VSCMD_ARG_TGT_ARCH) { $env:VSCMD_ARG_TGT_ARCH }
  elseif ($env:Platform) { $env:Platform }
  else { "x64" }
$Arch = switch -Regex ($ArchRaw.ToLower()) { "arm64" { "arm64" } "x86|win32" { "x86" } default { "x64" } }
$WebView2LoaderStatic = Join-Path $BuildDir "_deps\webview2-sdk\build\native\$Arch\WebView2LoaderStatic.lib"
if (-not (Test-Path $WebView2LoaderStatic)) {
  $found = Get-ChildItem -Path (Join-Path $BuildDir "_deps") -Recurse -Filter "WebView2LoaderStatic.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($found) { $WebView2LoaderStatic = $found.FullName }
  else { throw "WebView2LoaderStatic.lib not found under $BuildDir\_deps" }
}
$WebView2LibDir = Split-Path $WebView2LoaderStatic -Parent
$HostDef = Join-Path $RepoRoot "bindings\csharp\nativeview_host.def"
if (-not (Test-Path $HostDef)) { throw "Missing $HostDef (required for Windows static P/Invoke exports)" }

$PropsPath = Join-Path $ScriptDir "NativeView.Static.props"
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

Push-Location $ScriptDir
try {
  & $DotNet publish (Join-Path $ScriptDir "Minimal\Minimal.csproj") -c Release -r win-$(
    switch ($Arch) { "arm64" { "arm64" } "x86" { "x86" } default { "x64" } }
  ) --self-contained false
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Pop-Location
}

Write-Host "Built under: $(Join-Path $ScriptDir 'Minimal\bin\Release')"
