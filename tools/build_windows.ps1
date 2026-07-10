<#
.SYNOPSIS
    Builds the Windows (u5win) target via MSBuild, locating it automatically
    through vswhere rather than assuming a fixed install path.

.PARAMETER Configuration
    Debug, Release, or "Strict Debug" (see u5win.slnx). Defaults to Release.

.PARAMETER Platform
    x64 or x86. Defaults to x64.

.EXAMPLE
    .\tools\build_windows.ps1
    .\tools\build_windows.ps1 -Configuration Debug -Platform x64
#>
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

function Find-MSBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $vswhere = "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
    }
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe |
            Select-Object -First 1
        if ($found -and (Test-Path $found)) { return $found }
    }

    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    return $null
}

$msbuild = Find-MSBuild
if (-not $msbuild) {
    throw "MSBuild not found. Install Visual Studio 2022+ or the Build Tools (winget install Microsoft.VisualStudio.2022.BuildTools or Microsoft.VisualStudio.BuildTools) with the 'Desktop development with C++' workload."
}

$solution = Resolve-Path (Join-Path $PSScriptRoot "..\u5win\u5win.slnx")

Write-Host "MSBuild: $msbuild"
Write-Host "Solution: $solution"
Write-Host "Configuration|Platform: $Configuration|$Platform"

& $msbuild $solution "/p:Configuration=$Configuration" "/p:Platform=$Platform" /m
exit $LASTEXITCODE
