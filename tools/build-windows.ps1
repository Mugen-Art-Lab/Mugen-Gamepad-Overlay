# SPDX-FileCopyrightText: 2026 Mugen Art Lab
# SPDX-License-Identifier: GPL-2.0-or-later

param(
    [ValidateSet("Debug", "RelWithDebInfo", "Release", "MinSizeRel")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$version = (Get-Content (Join-Path $projectRoot "VERSION") -Raw).Trim()
Set-Location $projectRoot

function Find-CMakeExecutable {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        (Join-Path $env:ProgramFiles "CMake\bin\cmake.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "CMake\bin\cmake.exe")
    )

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installations = @(& $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath)
        foreach ($installation in $installations) {
            if (-not [string]::IsNullOrWhiteSpace($installation)) {
                $candidates += Join-Path $installation "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            }
        }
    }

    foreach ($base in @(
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio")
    )) {
        if (Test-Path $base) {
            $matches = Get-Item (Join-Path $base "*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe") -ErrorAction SilentlyContinue
            foreach ($match in $matches) {
                $candidates += $match.FullName
            }
        }
    }

    foreach ($candidate in $candidates | Select-Object -Unique) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Find-VisualStudioCppInstallation {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        return $null
    }

    $installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ([string]::IsNullOrWhiteSpace($installation)) {
        return $null
    }

    return $installation.Trim()
}

if (-not (Test-Path "cmake/common/bootstrap.cmake")) {
    Write-Host ""
    Write-Host "This is the compact source folder, not the generated OBS project." -ForegroundColor Red
    Write-Host "Run CREATE_PROJECT.cmd first. Then run BUILD_WINDOWS.cmd again from the repository root." -ForegroundColor Yellow
    exit 10
}

$cmake = Find-CMakeExecutable
if (-not $cmake) {
    Write-Host ""
    Write-Host "CMake was not found." -ForegroundColor Red
    Write-Host ""
    Write-Host "Recommended fix:" -ForegroundColor Yellow
    Write-Host "  1. Open Visual Studio Installer."
    Write-Host "  2. Choose Modify for Visual Studio / Build Tools."
    Write-Host "  3. Enable 'Desktop development with C++'."
    Write-Host "  4. Make sure 'C++ CMake tools for Windows' is selected."
    Write-Host ""
    Write-Host "Alternative: install CMake for Windows and add it to PATH."
    Write-Host "After installation, close this window and run BUILD_WINDOWS.cmd again."
    exit 11
}

$vsCpp = Find-VisualStudioCppInstallation
if (-not $vsCpp) {
    Write-Host ""
    Write-Host "Microsoft C++ build tools were not found." -ForegroundColor Red
    Write-Host "Open Visual Studio Installer and install 'Desktop development with C++'." -ForegroundColor Yellow
    Write-Host "Make sure the MSVC x64/x86 build tools and a Windows SDK are selected."
    exit 12
}

# Repair projects generated from OBS templates that pin Windows SDK 10.0.22621.
# A plain x64 generator platform lets Visual Studio choose the newest installed SDK.
$presetsPath = Join-Path $projectRoot "CMakePresets.json"
if (Test-Path $presetsPath) {
    $presetsText = Get-Content $presetsPath -Raw
    if ($presetsText -match 'x64,version=10\.0\.22621') {
        Write-Host "Removing the obsolete fixed Windows SDK requirement (10.0.22621)..." -ForegroundColor Yellow
        $presetsText = $presetsText -replace 'x64,version=10\.0\.22621', 'x64'
        [System.IO.File]::WriteAllText($presetsPath, $presetsText, [System.Text.UTF8Encoding]::new($false))
        $failedBuildDir = Join-Path $projectRoot "build_x64"
        if (Test-Path $failedBuildDir) {
            Remove-Item -Recurse -Force $failedBuildDir
        }
    }
}

Write-Host "Using CMake: $cmake"
Write-Host "Using Visual Studio C++ tools: $vsCpp"
Write-Host ""
Write-Host "Configuring..."
& $cmake --preset windows-x64
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Building $Configuration..."
& $cmake --build --preset windows-x64 --config $Configuration
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$dist = Join-Path $projectRoot "dist"
if (Test-Path $dist) {
    Remove-Item -Recurse -Force $dist
}

Write-Host "Collecting plugin files..."
& $cmake --install build_x64 --prefix $dist --config $Configuration
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$packageDir = Join-Path $dist $Configuration
if (-not (Test-Path $packageDir)) {
    $packageDir = $dist
}

Write-Host "Packaging release assets..."
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot "tools\package-release.ps1") `
    -GeneratedProject $projectRoot -Configuration $Configuration -BuildInstallerIfAvailable
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "Done. Release files are under:" -ForegroundColor Green
Write-Host (Join-Path $projectRoot "release\$version")
