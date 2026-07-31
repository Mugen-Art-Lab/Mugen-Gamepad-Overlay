param(
    [string]$Destination = (Join-Path $PSScriptRoot "..\build-workspace")
)

# SPDX-FileCopyrightText: 2026 Mugen Art Lab
# SPDX-License-Identifier: GPL-2.0-or-later

$ErrorActionPreference = "Stop"
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$destinationPath = [System.IO.Path]::GetFullPath($Destination)
$version = (Get-Content (Join-Path $sourceRoot "VERSION") -Raw).Trim()

# The source repository intentionally stays compact. A pinned revision of the
# official OBS plugin template supplies the OBS build infrastructure.
$templateRevision = "7540ffa"
$templateUrl = "https://github.com/obsproject/obs-plugintemplate/archive/$templateRevision.zip"
$temp = Join-Path $env:TEMP ("mugen-gamepad-overlay-template-" + [guid]::NewGuid().ToString("N"))
$zip = Join-Path $temp "template.zip"

function Remove-WorkspaceItem {
    param([string]$RelativePath)

    $path = Join-Path $destinationPath $RelativePath
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

if (Test-Path -LiteralPath $destinationPath) {
    throw "Build workspace already exists: $destinationPath`nDelete it only when you intentionally want a clean workspace."
}

try {
    New-Item -ItemType Directory -Force -Path $temp | Out-Null
    Write-Host "Downloading the pinned official OBS plugin template ($templateRevision)..."
    Invoke-WebRequest -Uri $templateUrl -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $temp

    $template = Get-ChildItem -LiteralPath $temp -Directory |
        Where-Object { $_.Name -like "obs-plugintemplate-*" } |
        Select-Object -First 1
    if (-not $template) {
        throw "The OBS plugin template archive had an unexpected structure."
    }

    Copy-Item -Recurse -Force $template.FullName $destinationPath

    # The downloaded template is build infrastructure, not a second public
    # repository. Remove template-only repository metadata and sample content
    # before placing the Mugen Gamepad Overlay files into the workspace.
    foreach ($item in @(
        ".github", ".clang-format", ".gersemirc", ".gitignore",
        "README.md", "README-RU.md", "LICENSE", "src", "data",
        "docs", "qa-tests", "installer", "LICENSES", "tools",
        "BUILD_WINDOWS.cmd", "CREATE_PROJECT.cmd", "VERSION",
        "AI-DISCLOSURE.md", "CHANGELOG.md", "CONTRIBUTING.md",
        "INSTALL.md", "INSTALL-RU.md", "SUPPORT.md",
        "OBS-TEMPLATE-REVISION.txt", "GENERATED-WORKSPACE.txt"
    )) {
        Remove-WorkspaceItem $item
    }

    # Files needed to compile the plugin and to assemble release packages.
    foreach ($folder in @("src", "data", "LICENSES", "docs")) {
        Copy-Item -Recurse -Force (Join-Path $sourceRoot $folder) (Join-Path $destinationPath $folder)
    }

    $installerDestination = Join-Path $destinationPath "installer"
    New-Item -ItemType Directory -Force -Path $installerDestination | Out-Null
    foreach ($file in @(
        "Mugen-Gamepad-Overlay.iss",
        "build-installer.ps1",
        "AFTER-INSTALL-EN.txt",
        "AFTER-INSTALL-RU.txt"
    )) {
        Copy-Item -Force (Join-Path $sourceRoot "installer\$file") (Join-Path $installerDestination $file)
    }

    foreach ($file in @(
        "CMakeLists.txt", "LICENSE", "VERSION",
        "README.md", "README-RU.md", "INSTALL.md", "INSTALL-RU.md",
        "AI-DISCLOSURE.md", "CHANGELOG.md", "SUPPORT.md"
    )) {
        Copy-Item -Force (Join-Path $sourceRoot $file) (Join-Path $destinationPath $file)
    }

    New-Item -ItemType Directory -Force -Path (Join-Path $destinationPath "tools") | Out-Null
    Copy-Item -Force (Join-Path $sourceRoot "tools\build-windows.ps1") (Join-Path $destinationPath "tools\build-windows.ps1")
    Copy-Item -Force (Join-Path $sourceRoot "tools\package-release.ps1") (Join-Path $destinationPath "tools\package-release.ps1")

    # Keep the template's dependency metadata and change only plugin identity.
    $buildspecPath = Join-Path $destinationPath "buildspec.json"
    $buildspec = Get-Content $buildspecPath -Raw | ConvertFrom-Json
    $buildspec | Add-Member -NotePropertyName name -NotePropertyValue "mugen-gamepad-overlay" -Force
    $buildspec | Add-Member -NotePropertyName displayName -NotePropertyValue "Mugen Gamepad Overlay" -Force
    $buildspec | Add-Member -NotePropertyName version -NotePropertyValue $version -Force
    $buildspec | Add-Member -NotePropertyName author -NotePropertyValue "Mugen Art Lab" -Force
    $buildspec | Add-Member -NotePropertyName website -NotePropertyValue "https://github.com/Mugen-Art-Lab/Mugen-Gamepad-Overlay" -Force
    $buildspec | Add-Member -NotePropertyName email -NotePropertyValue "" -Force
    [System.IO.File]::WriteAllText(
        $buildspecPath,
        ($buildspec | ConvertTo-Json -Depth 20),
        [System.Text.UTF8Encoding]::new($false)
    )

    # Let Visual Studio choose the newest installed Windows SDK instead of an
    # older fixed SDK version that some template revisions may specify.
    $presetsPath = Join-Path $destinationPath "CMakePresets.json"
    $presets = Get-Content $presetsPath -Raw | ConvertFrom-Json
    $windowsPreset = $presets.configurePresets |
        Where-Object { $_.name -eq "windows-x64" } |
        Select-Object -First 1
    if (-not $windowsPreset) {
        throw "Could not find the windows-x64 preset in CMakePresets.json"
    }
    $windowsPreset.architecture = "x64"
    [System.IO.File]::WriteAllText(
        $presetsPath,
        ($presets | ConvertTo-Json -Depth 30),
        [System.Text.UTF8Encoding]::new($false)
    )

    @(
        "Official OBS plugin template",
        "Repository: https://github.com/obsproject/obs-plugintemplate",
        "Pinned revision: $templateRevision"
    ) | Set-Content -LiteralPath (Join-Path $destinationPath "OBS-TEMPLATE-REVISION.txt") -Encoding UTF8

    @(
        "GENERATED BUILD WORKSPACE - DO NOT COMMIT",
        "",
        "This directory was created automatically from the compact source repository",
        "and pinned OBS plugin template revision $templateRevision.",
        "",
        "It is not a second Git repository. Git ignores build-workspace/.",
        "Edit source files in the repository root, not here.",
        "Run BUILD_WINDOWS.cmd only from the repository root.",
        "The whole directory can be deleted whenever a clean rebuild is needed.",
        "Release files are created under release\$version."
    ) | Set-Content -LiteralPath (Join-Path $destinationPath "GENERATED-WORKSPACE.txt") -Encoding UTF8

    Write-Host ""
    Write-Host "Build workspace created (generated, ignored by Git, safe to delete):" -ForegroundColor Green
    Write-Host "  $destinationPath"
    Write-Host "Run BUILD_WINDOWS.cmd from the repository root."
}
finally {
    if (Test-Path -LiteralPath $temp) {
        Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
    }
}
