# SPDX-FileCopyrightText: 2026 Mugen Art Lab
# SPDX-License-Identifier: GPL-2.0-or-later
# The OBS 33 break occurs at DLL load time. Inspect the actual packaged DLL.
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$StandardZip)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zipPath = (Resolve-Path -LiteralPath $StandardZip).Path
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'vswhere.exe is required for the compiled DLL import check.'
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ toolchain not found.' }
$vsRoot = ($vs | Select-Object -First 1).Trim()
$candidates = @(Get-ChildItem -Path (Join-Path $vsRoot 'VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe') -File -ErrorAction SilentlyContinue)
if (-not $candidates.Count) { throw "dumpbin.exe not found: $vsRoot" }
$dumpbin = ($candidates | Sort-Object FullName -Descending | Select-Object -First 1).FullName

$archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
$tempDir = Join-Path $env:TEMP ("mgo-import-check-" + [guid]::NewGuid().ToString('N'))
try {
    # Verify both OBS 33 and OBS 32 layouts are included and byte-identical.
    $expectedPaths = @(
        'mugen-gamepad-overlay/mugen-gamepad-overlay.dll',
        'mugen-gamepad-overlay/bin/64bit/mugen-gamepad-overlay.dll'
    )
    $entries = @($archive.Entries | Where-Object {
        $_.FullName.Replace('\', '/') -in $expectedPaths
    })
    $allDlls = @($archive.Entries | Where-Object {
        $_.FullName.Replace('\', '/') -match '(^|/)mugen-gamepad-overlay\.dll$'
    })
    if ($entries.Count -ne 2 -or $allDlls.Count -ne 2) {
        throw 'Expected exactly the two OBS 32/33 plugin DLL paths in standard ZIP.'
    }
    $hashes = @()
    foreach ($entry in $entries) {
        $stream = $entry.Open()
        try { $hashes += (Get-FileHash -InputStream $stream -Algorithm SHA256).Hash }
        finally { $stream.Dispose() }
    }
    if ($hashes[0] -ne $hashes[1]) { throw 'Modern and legacy DLL differ inside standard ZIP.' }
    $modern = @($entries | Where-Object {
        $_.FullName.Replace('\', '/') -eq $expectedPaths[0]
    })[0]
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
    $dll = Join-Path $tempDir 'mugen-gamepad-overlay.dll'
    $readStream = $modern.Open()
    $writeStream = [System.IO.File]::Create($dll)
    try { $readStream.CopyTo($writeStream) }
    finally {
        $writeStream.Dispose()
        $readStream.Dispose()
    }
    $output = @(& $dumpbin /imports $dll 2>&1)
    if ($LASTEXITCODE -ne 0) { throw "dumpbin.exe failed: $LASTEXITCODE" }
    $imports = $output -join [Environment]::NewLine
    if ($imports -match '(?i)\bgs_image_file_(?:init|free|ex_init|ex_free|init_texture|update_texture)\b') {
        throw 'Incompatible OBS image-file imports found in plugin DLL.'
    }
    if ($imports -notmatch '(?i)\b(?:obs|libobs)\.dll\b') {
        throw 'Expected OBS module import missing; inspect dumpbin /imports output.'
    }
    Write-Host 'PASS: both OBS 32/33 DLL copies match and packaged binary imports no removed OBS image API.'
}
finally {
    $archive.Dispose()
    if (Test-Path -LiteralPath $tempDir) {
        Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
