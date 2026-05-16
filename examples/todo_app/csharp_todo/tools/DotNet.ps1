# Resolve dotnet.exe when it is not on PATH (common in cmd / double-click .bat).
# Prefers an install that has SDKs (skips runtime-only Program Files (x86) hosts).
# SPDX-License-Identifier: Apache-2.0

function Test-DotNetHasSdk {
    param([Parameter(Mandatory)][string]$Exe)
    try {
        $sdks = & $Exe --list-sdks 2>$null
        return ($LASTEXITCODE -eq 0 -and $sdks)
    } catch {
        return $false
    }
}

function Resolve-DotNetExe {
    $candidates = [System.Collections.Generic.List[string]]::new()

    if ($env:DOTNET_EXE -and (Test-Path -LiteralPath $env:DOTNET_EXE)) {
        $candidates.Add((Resolve-Path -LiteralPath $env:DOTNET_EXE).Path)
    }

    $roots = [System.Collections.Generic.List[string]]::new()
    if ($env:DOTNET_ROOT) { $roots.Add($env:DOTNET_ROOT) }
    if ($env:ProgramFiles) {
        $roots.Add((Join-Path $env:ProgramFiles 'dotnet'))
    }
    if (${env:ProgramFiles(x86)}) {
        $roots.Add((Join-Path ${env:ProgramFiles(x86)} 'dotnet'))
    }
    if ($env:LOCALAPPDATA) {
        $roots.Add((Join-Path $env:LOCALAPPDATA 'Microsoft\dotnet'))
    }

    $relPaths = @(
        'dotnet.exe',
        'x64\dotnet.exe',
        'arm64\dotnet.exe'
    )

    foreach ($root in ($roots | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($root)) { continue }
        foreach ($rel in $relPaths) {
            $candidate = Join-Path $root $rel
            if (Test-Path -LiteralPath $candidate) {
                $candidates.Add((Resolve-Path -LiteralPath $candidate).Path)
            }
        }
    }

    $fromPath = Get-Command dotnet -All -ErrorAction SilentlyContinue
    if ($fromPath) {
        foreach ($cmd in $fromPath) {
            if ($cmd.Source) { $candidates.Add($cmd.Source) }
        }
    }

    foreach ($exe in ($candidates | Select-Object -Unique)) {
        if (Test-DotNetHasSdk -Exe $exe) {
            return $exe
        }
    }

    throw @"
Could not find the .NET SDK (dotnet.exe with installed SDKs).

Install the .NET 8 SDK (or newer): https://dotnet.microsoft.com/download/dotnet/8.0

If it is already installed, either:
  - Open a new terminal after install, or
  - Put C:\Program Files\dotnet before any runtime-only host on PATH, or
  - Set DOTNET_EXE to the full path, for example:
      set DOTNET_EXE=C:\Program Files\dotnet\dotnet.exe
"@
}
