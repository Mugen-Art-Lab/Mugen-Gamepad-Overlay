# SPDX-FileCopyrightText: 2026 Mugen Art Lab
# SPDX-License-Identifier: GPL-2.0-or-later

param(
    [Parameter(Mandatory = $true)]
    [string]$PayloadRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$version = (Get-Content (Join-Path $repoRoot "VERSION") -Raw).Trim()
$payload = (Resolve-Path $PayloadRoot).Path
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$output = (Resolve-Path $OutputDir).Path

function Find-Iscc {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

$iscc = Find-Iscc
if (-not $iscc) {
    Write-Host ""
    Write-Host "Inno Setup 6 was not found. The ZIP packages are still ready." -ForegroundColor Yellow
    Write-Host "Install it with: winget install JRSoftware.InnoSetup" -ForegroundColor Yellow
    exit 0
}

$generatedInfoDir = Join-Path $output "installer-info"
New-Item -ItemType Directory -Path $generatedInfoDir -Force | Out-Null
$infoEn = Join-Path $generatedInfoDir "AFTER-INSTALL-EN.txt"
$infoRu = Join-Path $generatedInfoDir "AFTER-INSTALL-RU.txt"

function Write-VersionedInfo([string]$Source, [string]$Destination) {
    # Windows PowerShell 5.1 otherwise treats UTF-8 without BOM as ANSI.
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false, $true)
    $text = [System.IO.File]::ReadAllText($Source, $utf8NoBom)
    $text = $text.Replace("@VERSION@", $version)
    $utf8Bom = [System.Text.UTF8Encoding]::new($true)
    [System.IO.File]::WriteAllText($Destination, $text, $utf8Bom)
}

Write-VersionedInfo (Join-Path $PSScriptRoot "AFTER-INSTALL-EN.txt") $infoEn
Write-VersionedInfo (Join-Path $PSScriptRoot "AFTER-INSTALL-RU.txt") $infoRu

$script = Join-Path $PSScriptRoot "Mugen-Gamepad-Overlay.iss"
Write-Host "Building installer with: $iscc"
& $iscc "/DMyAppVersion=$version" "/DPayloadRoot=$payload" "/DOutputDir=$output" "/DRepoRoot=$repoRoot" "/DInfoAfterEN=$infoEn" "/DInfoAfterRU=$infoRu" $script
$code = $LASTEXITCODE
Remove-Item -LiteralPath $generatedInfoDir -Recurse -Force -ErrorAction SilentlyContinue
exit $code
