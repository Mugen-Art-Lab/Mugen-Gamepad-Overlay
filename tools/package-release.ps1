# SPDX-FileCopyrightText: 2026 Mugen Art Lab
# SPDX-License-Identifier: GPL-2.0-or-later

param(
    [Parameter(Mandatory = $true)]
    [string]$GeneratedProject,

    [ValidateSet("Debug", "RelWithDebInfo", "Release", "MinSizeRel")]
    [string]$Configuration = "Release",

    [switch]$BuildInstallerIfAvailable
)

$ErrorActionPreference = "Stop"
$pluginName = "mugen-gamepad-overlay"
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$projectRoot = (Resolve-Path $GeneratedProject).Path
$version = (Get-Content (Join-Path $sourceRoot "VERSION") -Raw).Trim()

function Find-PluginDll {
    param([string]$Root, [string]$Config)

    $preferred = @(
        (Join-Path $Root "dist\$Config\obs-plugins\64bit\mugen-gamepad-overlay.dll"),
        (Join-Path $Root "dist\$Config\bin\64bit\mugen-gamepad-overlay.dll"),
        (Join-Path $Root "dist\obs-plugins\64bit\mugen-gamepad-overlay.dll"),
        (Join-Path $Root "dist\bin\64bit\mugen-gamepad-overlay.dll"),
        (Join-Path $Root "build_x64\rundir\$Config\obs-plugins\64bit\mugen-gamepad-overlay.dll")
    )

    foreach ($candidate in $preferred) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $found = Get-ChildItem -LiteralPath $Root -Recurse -File -Filter "mugen-gamepad-overlay.dll" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "\\(dist|build_x64)\\" } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if (-not $found) {
        throw "mugen-gamepad-overlay.dll was not found. Build and install the plugin first."
    }

    return $found.FullName
}

function Reset-Directory {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-CommonDocuments {
    param([string]$Destination)

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRoot "README.md") -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "README-RU.md") -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "INSTALL.md") -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "INSTALL-RU.md") -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "CHANGELOG.md") -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "AI-DISCLOSURE.md") -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "SUPPORT.md") -Destination $Destination -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "LICENSE") -Destination (Join-Path $Destination "LICENSE") -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "LICENSES") -Destination (Join-Path $Destination "LICENSES") -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot "docs") -Destination (Join-Path $Destination "docs") -Recurse -Force

    $dataDestination = Join-Path $Destination "data"
    New-Item -ItemType Directory -Path $dataDestination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRoot "data\THIRD-PARTY-NOTICES.txt") -Destination $dataDestination -Force
}

function New-ZipFromDirectory {
    param([string]$SourceDirectory, [string]$ZipPath)
    if (Test-Path -LiteralPath $ZipPath) {
        Remove-Item -LiteralPath $ZipPath -Force
    }
    Compress-Archive -Path (Join-Path $SourceDirectory "*") -DestinationPath $ZipPath -CompressionLevel Optimal
}

$dll = Find-PluginDll -Root $projectRoot -Config $Configuration
$localeSource = Join-Path $sourceRoot "data\locale"
if (-not (Test-Path -LiteralPath $localeSource)) {
    throw "Locale directory is missing: $localeSource"
}

$releaseRoot = Join-Path $projectRoot "release\$version"
$workRoot = Join-Path $projectRoot "release-work\$version"
$standardStage = Join-Path $workRoot "standard"
$portableStage = Join-Path $workRoot "portable"
$payloadStage = Join-Path $workRoot "installer-payload"

Reset-Directory $releaseRoot
Reset-Directory $workRoot

try {
    # Standard all-users layout: extract the ZIP into ProgramData\obs-studio\plugins.
    $standardPlugin = Join-Path $standardStage $pluginName
    New-Item -ItemType Directory -Path (Join-Path $standardPlugin "bin\64bit") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $standardPlugin "data\locale") -Force | Out-Null
    Copy-Item -LiteralPath $dll -Destination (Join-Path $standardPlugin "bin\64bit\mugen-gamepad-overlay.dll") -Force
    Copy-Item -Path (Join-Path $localeSource "*") -Destination (Join-Path $standardPlugin "data\locale") -Force
    Copy-CommonDocuments -Destination $standardPlugin

    # Portable/custom OBS layout: extract the ZIP into the OBS root directory.
    $portableData = Join-Path $portableStage "data\obs-plugins\$pluginName"
    New-Item -ItemType Directory -Path (Join-Path $portableStage "obs-plugins\64bit") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $portableData "locale") -Force | Out-Null
    Copy-Item -LiteralPath $dll -Destination (Join-Path $portableStage "obs-plugins\64bit\mugen-gamepad-overlay.dll") -Force
    Copy-Item -Path (Join-Path $localeSource "*") -Destination (Join-Path $portableData "locale") -Force
    Copy-CommonDocuments -Destination $portableData

    # Installer payload mirrors the recommended ProgramData plugin folder.
    $payloadPlugin = Join-Path $payloadStage $pluginName
    New-Item -ItemType Directory -Path (Join-Path $payloadPlugin "bin\64bit") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $payloadPlugin "data\locale") -Force | Out-Null
    Copy-Item -LiteralPath $dll -Destination (Join-Path $payloadPlugin "bin\64bit\mugen-gamepad-overlay.dll") -Force
    Copy-Item -Path (Join-Path $localeSource "*") -Destination (Join-Path $payloadPlugin "data\locale") -Force
    Copy-CommonDocuments -Destination $payloadPlugin

    $standardZip = Join-Path $releaseRoot "Mugen-Gamepad-Overlay-$version-Windows-x64.zip"
    $portableZip = Join-Path $releaseRoot "Mugen-Gamepad-Overlay-$version-Windows-x64-portable.zip"
    New-ZipFromDirectory -SourceDirectory $standardStage -ZipPath $standardZip
    New-ZipFromDirectory -SourceDirectory $portableStage -ZipPath $portableZip

    if ($BuildInstallerIfAvailable) {
        & (Join-Path $sourceRoot "installer\build-installer.ps1") -PayloadRoot $payloadStage -OutputDir $releaseRoot
        if ($LASTEXITCODE -ne 0) {
            throw "Installer build failed with code $LASTEXITCODE"
        }
    }

    $assets = Get-ChildItem -LiteralPath $releaseRoot -File |
        Where-Object { $_.Extension -in @(".zip", ".exe") } |
        Sort-Object Name

    $hashLines = foreach ($asset in $assets) {
        $hash = (Get-FileHash -LiteralPath $asset.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $($asset.Name)"
    }
    $hashLines | Set-Content -LiteralPath (Join-Path $releaseRoot "SHA256SUMS.txt") -Encoding ASCII

    Write-Host ""
    Write-Host "Release packages:" -ForegroundColor Green
    $assets | ForEach-Object { Write-Host "  $($_.FullName)" }
    if (-not ($assets | Where-Object { $_.Extension -eq ".exe" })) {
        Write-Host "  Installer was not built. Install Inno Setup 6 and run BUILD_WINDOWS.cmd again." -ForegroundColor Yellow
    }
    Write-Host "  $(Join-Path $releaseRoot 'SHA256SUMS.txt')"
}
finally {
    if (Test-Path -LiteralPath $workRoot) {
        Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
