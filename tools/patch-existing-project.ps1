$ErrorActionPreference = "Stop"
$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

Add-Type -AssemblyName System.Windows.Forms
$dialog = New-Object System.Windows.Forms.FolderBrowserDialog
$dialog.Description = "Select an existing generated Mugen-Gamepad-Overlay project folder that contains build_x64"
$dialog.ShowNewFolderButton = $false
if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
    Write-Host "Cancelled by user."
    exit 2
}

$target = $dialog.SelectedPath
if (-not (Test-Path (Join-Path $target "cmake\common\bootstrap.cmake"))) {
    throw "The selected folder is not a generated OBS plugin project: $target"
}
if (-not (Test-Path (Join-Path $target "build_x64"))) {
    throw "The selected folder has no build_x64 cache. Select the Mugen-Gamepad-Overlay folder used for a previous successful build."
}

Write-Host "Using existing OBS/Qt/SDL build cache:" -ForegroundColor Cyan
Write-Host "  $target"
Write-Host "Updating only Mugen Gamepad Overlay files..." -ForegroundColor Cyan

foreach ($folder in @("src", "data")) {
    $destination = Join-Path $target $folder
    if (Test-Path $destination) {
        Remove-Item -Recurse -Force $destination
    }
    Copy-Item -Recurse -Force (Join-Path $sourceRoot $folder) $destination
}

Copy-Item -Force (Join-Path $sourceRoot "CMakeLists.txt") (Join-Path $target "CMakeLists.txt")
Copy-Item -Force (Join-Path $sourceRoot "README.md") (Join-Path $target "README.md")
Copy-Item -Force (Join-Path $sourceRoot "LICENSE") (Join-Path $target "LICENSE")
New-Item -ItemType Directory -Force -Path (Join-Path $target "tools") | Out-Null
Copy-Item -Force (Join-Path $sourceRoot "tools\build-windows.ps1") (Join-Path $target "tools\build-windows.ps1")
Copy-Item -Force (Join-Path $sourceRoot "tools\package-release.ps1") (Join-Path $target "tools\package-release.ps1")
Copy-Item -Force (Join-Path $sourceRoot "BUILD_WINDOWS.cmd") (Join-Path $target "BUILD_WINDOWS.cmd")

foreach ($folder in @("installer", "LICENSES", "docs", "qa-tests", "previews")) {
    $destinationFolder = Join-Path $target $folder
    if (Test-Path $destinationFolder) { Remove-Item -Recurse -Force $destinationFolder }
    Copy-Item -Recurse -Force (Join-Path $sourceRoot $folder) $destinationFolder
}
foreach ($file in @("README-RU.md", "INSTALL.md", "INSTALL-RU.md", "AI-DISCLOSURE.md", "CHANGELOG.md", "SUPPORT.md", "CONTRIBUTING.md", "RELEASE-CHECKLIST-RU.md")) {
    Copy-Item -Force (Join-Path $sourceRoot $file) (Join-Path $target $file)
}
$issueTemplateSource = Join-Path $sourceRoot ".github\ISSUE_TEMPLATE"
$issueTemplateDestination = Join-Path $target ".github\ISSUE_TEMPLATE"
New-Item -ItemType Directory -Force -Path $issueTemplateDestination | Out-Null
Copy-Item -Recurse -Force (Join-Path $issueTemplateSource "*") $issueTemplateDestination

# Files extracted from a ZIP can carry timestamps older than objects already in
# build_x64. MSBuild then incorrectly keeps stale .obj files even when their
# source contents changed. Touch all Mugen sources and remove only this plugin's
# object/output files. OBS, Qt and SDL caches remain intact.
$now = Get-Date
Get-ChildItem -LiteralPath (Join-Path $target "src") -Recurse -File | ForEach-Object { $_.LastWriteTime = $now }
Get-ChildItem -LiteralPath (Join-Path $target "data") -Recurse -File | ForEach-Object { $_.LastWriteTime = $now }
(Get-Item -LiteralPath (Join-Path $target "CMakeLists.txt")).LastWriteTime = $now

$buildDir = Join-Path $target "build_x64"
Get-ChildItem -LiteralPath $buildDir -Recurse -Directory -Filter "mugen-gamepad-overlay.dir" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Remove-Item -Recurse -Force
Get-ChildItem -LiteralPath $buildDir -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "mugen-gamepad-overlay.*" } | Remove-Item -Force

$buildspecPath = Join-Path $target "buildspec.json"
$buildspec = Get-Content $buildspecPath -Raw | ConvertFrom-Json
$buildspec | Add-Member -NotePropertyName name -NotePropertyValue "mugen-gamepad-overlay" -Force
$buildspec | Add-Member -NotePropertyName displayName -NotePropertyValue "Mugen Gamepad Overlay" -Force
$buildspec | Add-Member -NotePropertyName version -NotePropertyValue "0.8.0" -Force
$buildspec | Add-Member -NotePropertyName author -NotePropertyValue "Mugen Art Lab" -Force
$buildspec | Add-Member -NotePropertyName website -NotePropertyValue "https://github.com/Mugen-Art-Lab/Mugen-Gamepad-Overlay" -Force
$json = $buildspec | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText($buildspecPath, $json, [System.Text.UTF8Encoding]::new($false))

Write-Host "Reconfiguring and rebuilding. Existing downloads will be reused..." -ForegroundColor Green
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $target "tools\build-windows.ps1") -Configuration Release
exit $LASTEXITCODE
