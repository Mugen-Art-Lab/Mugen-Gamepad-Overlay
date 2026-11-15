# SPDX-FileCopyrightText: 2026 Mugen Art Lab
# SPDX-License-Identifier: GPL-2.0-or-later
# Verify the shipped plugin metadata, rather than only checking source files.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$StandardZip,
    [Parameter(Mandatory = $true)][string]$PortableZip,
    [Parameter(Mandatory = $true)][string]$Version
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$plugin = 'mugen-gamepad-overlay'
function Assert-PackageMetadata {
    param([string]$ArchivePath, [string[]]$RequiredPaths)
    $archive = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $ArchivePath).Path)
    try {
        $entries = @($archive.Entries | Where-Object {
            $_.FullName.Replace('\', '/') -match '(^|/)manifest\.json$'
        })
        if ($entries.Count -ne $RequiredPaths.Count) {
            throw "Expected $($RequiredPaths.Count) manifests in $ArchivePath, found $($entries.Count)"
        }
        foreach ($required in $RequiredPaths) {
            $matches = @($entries | Where-Object { $_.FullName.Replace('\', '/') -eq $required })
            if ($matches.Count -ne 1) { throw "Missing or duplicated manifest: $required in $ArchivePath" }
            $stream = $matches[0].Open()
            $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::UTF8)
            try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json }
            finally { $reader.Dispose() }
            if ($manifest.id -cne $plugin) { throw "Wrong id in $required" }
            if ($manifest.version -cne $Version) { throw "Wrong version in ${required}: $($manifest.version), expected $Version" }
            if ($manifest.display_name -cne 'Mugen Gamepad Overlay') { throw "Wrong display name in $required" }
            if (-not $manifest.urls.repository) { throw "Missing repository URL in $required" }
        }
    }
    finally { $archive.Dispose() }
}

Assert-PackageMetadata -ArchivePath $StandardZip -RequiredPaths @(
    "$plugin/data/manifest.json"
)
Assert-PackageMetadata -ArchivePath $PortableZip -RequiredPaths @(
    "plugins/$plugin/data/manifest.json",
    "data/obs-plugins/$plugin/manifest.json"
)
Write-Host "PASS: OBS plugin manifests carry $plugin version $Version in standard and both portable layouts."
