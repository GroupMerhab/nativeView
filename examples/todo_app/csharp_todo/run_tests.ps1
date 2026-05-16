# Run C# unit tests (no nativeview link required).
$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot

. (Join-Path $ScriptDir "tools\DotNet.ps1")
$DotNet = Resolve-DotNetExe
$Gen = Join-Path $ScriptDir "generated\TodoHtmlEmbed.cs"
if (-not (Test-Path $Gen)) {
  $python = if ($env:PYTHON) { $env:PYTHON } else { "python" }
  $HtmlIn = (Resolve-Path (Join-Path $ScriptDir "..\ui\fallback_index.html")).Path
  New-Item -ItemType Directory -Force -Path (Join-Path $ScriptDir "generated") | Out-Null
  & $python (Join-Path $ScriptDir "tools\embed_html_cs.py") $HtmlIn $Gen "Embedded Vue todo UI (fallback for tests)"
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Push-Location $ScriptDir
try {
  & $DotNet test (Join-Path $ScriptDir "tests\TodoApp.Tests.csproj") -c Release
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Pop-Location
}
Write-Host "csharp_todo tests: OK"
