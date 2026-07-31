param(
    [Parameter(Mandatory = $true)]
    [string]$PayloadRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
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

$script = Join-Path $PSScriptRoot "Mugen-Gamepad-Overlay.iss"
Write-Host "Building installer with: $iscc"
& $iscc "/DPayloadRoot=$payload" "/DOutputDir=$output" "/DRepoRoot=$repoRoot" $script
exit $LASTEXITCODE
