<#
.SYNOPSIS
    Extracts the 15 BGM tracks for u5win from the C64 "Lost Music" program
    (Minstrel Dragon / UDIC) and writes them as BGM/01.ogg .. BGM/15.ogg.

.DESCRIPTION
    u5win/ULTMVMSC.P00 is a raw Commodore 64 program (a "SYS 2064" BASIC
    loader wrapping 6502 machine code), not a playable audio file. It plays
    music through the C64's SID chip and lets the user pick one of 15 songs
    by pressing keys 'a' through 'o'.

    This script runs the program in VICE's x64sc emulator, records each
    track's actual SID audio output to .ogg, trims the boot-time silence
    (and the final second, which may contain a cutoff click since the
    emulator is force-killed rather than cleanly stopped), and writes the
    results to -OutputDir.

    Track order is assumed to match the game's BGM_ID numbering directly:
    key 'a' -> BGM 1 -> 01.ogg, key 'b' -> BGM 2 -> 02.ogg, etc. This
    matches the SDL port's expectation of exactly 15 tracks
    (src/audio/aud_sdl.c: AUDIO_SDL_LoadBgmTable loads BGM/01.ogg..15.ogg).

.PARAMETER SourceFile
    Path to ULTMVMSC.P00. Defaults to u5win/ULTMVMSC.P00 next to this script.

.PARAMETER OutputDir
    Directory to write 01.ogg..15.ogg into. Defaults to u5win/x64/Release/BGM.

.PARAMETER VicePath
    Path to x64sc.exe. Auto-detected from the standard winget install
    location if omitted.

.PARAMETER CaptureSeconds
    How many seconds of each track to record before trimming. Default 75 -
    generous enough to capture a full loop for any track in this pack.

.PARAMETER StartTrack
.PARAMETER EndTrack
    Track range to (re-)capture, 1-15. Useful for redoing a single track
    that came out too short (see warning below).

.EXAMPLE
    # Requires VICE (winget install VICE-Team.VICE.GTK3) and
    # ffmpeg (winget install Gyan.FFmpeg) to be installed first.
    .\tools\extract_bgm.ps1

.EXAMPLE
    # Redo just track 6 after a bad capture
    .\tools\extract_bgm.ps1 -StartTrack 6 -EndTrack 6
#>
param(
    [string]$SourceFile = "$PSScriptRoot\..\u5win\ULTMVMSC.P00",
    [string]$OutputDir = "$PSScriptRoot\..\u5win\x64\Release\BGM",
    [string]$VicePath,
    [int]$CaptureSeconds = 75,
    [int]$StartTrack = 1,
    [int]$EndTrack = 15
)

$ErrorActionPreference = "Stop"

function Find-WingetExe([string]$exeName, [string]$packageFilter) {
    $cmd = Get-Command $exeName -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $pkgDir = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Directory -Filter $packageFilter -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $pkgDir) { return $null }

    $found = Get-ChildItem $pkgDir.FullName -Recurse -Filter $exeName -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { return $found.FullName }
    return $null
}

if (-not $VicePath) {
    $VicePath = Find-WingetExe "x64sc.exe" "*VICE*"
}
if (-not $VicePath -or -not (Test-Path $VicePath)) {
    throw "x64sc.exe not found. Install VICE (winget install VICE-Team.VICE.GTK3) or pass -VicePath explicitly."
}

$ffmpeg = Find-WingetExe "ffmpeg.exe" "*FFmpeg*"
$ffprobe = Find-WingetExe "ffprobe.exe" "*FFmpeg*"
if (-not $ffmpeg -or -not $ffprobe) {
    throw "ffmpeg/ffprobe not found. Install ffmpeg (winget install Gyan.FFmpeg) or ensure it's on PATH."
}

$SourceFile = (Resolve-Path $SourceFile).Path
if (-not (Test-Path $SourceFile)) {
    throw "Source file not found: $SourceFile"
}

$rawDir = Join-Path $env:TEMP "ultima5-sdl_bgm_raw"
New-Item -ItemType Directory -Force -Path $rawDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class U5BgmWin32 {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
}
"@

$letters = "abcdefghijklmno".ToCharArray()

for ($trackNum = $StartTrack; $trackNum -le $EndTrack; $trackNum++) {
    $letter = $letters[$trackNum - 1]
    $rawFile = Join-Path $rawDir ("{0:D2}.ogg" -f $trackNum)
    $outFile = Join-Path $OutputDir ("{0:D2}.ogg" -f $trackNum)

    Write-Host "Capturing track $trackNum (key '$letter')..."

    $proc = Start-Process -FilePath $VicePath `
        -ArgumentList "-soundrecdev", "ogg", "-soundrecarg", "`"$rawFile`"", "-autostart", "`"$SourceFile`"" `
        -PassThru

    Start-Sleep -Seconds 4
    try { [U5BgmWin32]::SetForegroundWindow($proc.MainWindowHandle) | Out-Null } catch {}
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait("$letter")

    Start-Sleep -Seconds $CaptureSeconds

    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1

    $totalDur = [double](& $ffprobe -v error -show_entries format=duration -of csv=p=0 $rawFile)
    if ($totalDur -le 5.0) {
        Write-Warning "Track $trackNum came out too short ($totalDur s) - likely a failed capture (VICE window may not have had focus in time). Re-run with -StartTrack $trackNum -EndTrack $trackNum."
        continue
    }

    $endTime = $totalDur - 1.0
    & $ffmpeg -y -i $rawFile -ss 3.0 -to $endTime -c:a libvorbis -q:a 4 $outFile 2>&1 | Out-Null

    Write-Host "  -> $outFile ($([math]::Round($endTime - 3.0, 1))s)"
}

Write-Host "Done."
