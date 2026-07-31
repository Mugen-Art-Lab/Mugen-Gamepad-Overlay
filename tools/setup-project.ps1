param(
    [string]$Destination = (Join-Path $PSScriptRoot "..\Mugen-Gamepad-Overlay")
)

$ErrorActionPreference = "Stop"
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$temp = Join-Path $env:TEMP ("mugen-gamepad-overlay-template-" + [guid]::NewGuid().ToString("N"))
$zip = Join-Path $temp "template.zip"

New-Item -ItemType Directory -Force -Path $temp | Out-Null
Write-Host "Downloading the official OBS plugin template..."
Invoke-WebRequest -Uri "https://github.com/obsproject/obs-plugintemplate/archive/refs/heads/master.zip" -OutFile $zip
Expand-Archive -Path $zip -DestinationPath $temp
$template = Join-Path $temp "obs-plugintemplate-master"

if (Test-Path $Destination) {
    throw "Destination already exists: $Destination"
}

Copy-Item -Recurse -Force $template $Destination

Remove-Item -Recurse -Force (Join-Path $Destination "src")
Copy-Item -Recurse -Force (Join-Path $sourceRoot "src") (Join-Path $Destination "src")

$dataDestination = Join-Path $Destination "data"
if (Test-Path $dataDestination) {
    Remove-Item -Recurse -Force $dataDestination
}
Copy-Item -Recurse -Force (Join-Path $sourceRoot "data") $dataDestination

Copy-Item -Force (Join-Path $sourceRoot "CMakeLists.txt") (Join-Path $Destination "CMakeLists.txt")
Copy-Item -Force (Join-Path $sourceRoot "README.md") (Join-Path $Destination "README.md")
Copy-Item -Force (Join-Path $sourceRoot "LICENSE") (Join-Path $Destination "LICENSE")
New-Item -ItemType Directory -Force -Path (Join-Path $Destination "tools") | Out-Null
Copy-Item -Force (Join-Path $sourceRoot "tools\build-windows.ps1") (Join-Path $Destination "tools\build-windows.ps1")
Copy-Item -Force (Join-Path $sourceRoot "tools\package-release.ps1") (Join-Path $Destination "tools\package-release.ps1")
Copy-Item -Force (Join-Path $sourceRoot "BUILD_WINDOWS.cmd") (Join-Path $Destination "BUILD_WINDOWS.cmd")

foreach ($folder in @("installer", "LICENSES", "docs", "qa-tests", "previews")) {
    $destinationFolder = Join-Path $Destination $folder
    if (Test-Path $destinationFolder) { Remove-Item -Recurse -Force $destinationFolder }
    Copy-Item -Recurse -Force (Join-Path $sourceRoot $folder) $destinationFolder
}
foreach ($file in @("README-RU.md", "INSTALL.md", "INSTALL-RU.md", "AI-DISCLOSURE.md", "CHANGELOG.md", "SUPPORT.md", "CONTRIBUTING.md", "RELEASE-CHECKLIST-RU.md")) {
    Copy-Item -Force (Join-Path $sourceRoot $file) (Join-Path $Destination $file)
}
$issueTemplateSource = Join-Path $sourceRoot ".github\ISSUE_TEMPLATE"
$issueTemplateDestination = Join-Path $Destination ".github\ISSUE_TEMPLATE"
New-Item -ItemType Directory -Force -Path $issueTemplateDestination | Out-Null
Copy-Item -Recurse -Force (Join-Path $issueTemplateSource "*") $issueTemplateDestination

# Preserve the template's current dependency versions and hashes, changing only
# the project metadata that bootstrap.cmake reads.
$buildspecPath = Join-Path $Destination "buildspec.json"
$buildspec = Get-Content $buildspecPath -Raw | ConvertFrom-Json
$buildspec | Add-Member -NotePropertyName name -NotePropertyValue "mugen-gamepad-overlay" -Force
$buildspec | Add-Member -NotePropertyName displayName -NotePropertyValue "Mugen Gamepad Overlay" -Force
$buildspec | Add-Member -NotePropertyName version -NotePropertyValue "0.8.0" -Force
$buildspec | Add-Member -NotePropertyName author -NotePropertyValue "Mugen Art Lab" -Force
$buildspec | Add-Member -NotePropertyName website -NotePropertyValue "https://github.com/Mugen-Art-Lab/Mugen-Gamepad-Overlay" -Force
$buildspec | Add-Member -NotePropertyName email -NotePropertyValue "" -Force
$json = $buildspec | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText($buildspecPath, $json, [System.Text.UTF8Encoding]::new($false))

# The upstream OBS template currently pins Windows SDK 10.0.22621 in
# CMakePresets.json. Use the newest SDK installed on the build machine instead,
# so users do not need to install an older SDK alongside a current one.
$presetsPath = Join-Path $Destination "CMakePresets.json"
$presets = Get-Content $presetsPath -Raw | ConvertFrom-Json
$windowsPreset = $presets.configurePresets | Where-Object { $_.name -eq "windows-x64" } | Select-Object -First 1
if (-not $windowsPreset) {
    throw "Could not find the windows-x64 preset in CMakePresets.json"
}
$windowsPreset.architecture = "x64"
$presetsJson = $presets | ConvertTo-Json -Depth 30
[System.IO.File]::WriteAllText($presetsPath, $presetsJson, [System.Text.UTF8Encoding]::new($false))

Remove-Item -Recurse -Force $temp
Write-Host ""
Write-Host "Project created: $Destination"
Write-Host "Run 'cmake --list-presets' in that folder, then build a Windows x64 preset."
