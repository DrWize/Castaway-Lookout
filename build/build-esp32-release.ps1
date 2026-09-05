[CmdletBinding()]
param(
    [string]$Version = '2026.1.0-rc.4',
    [string]$BuildDirectory,
    [string]$SourceRevision
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildDirectory) {
    $BuildDirectory = Join-Path $projectRoot 'esp32\build-web'
}
if (!$SourceRevision) {
    $SourceRevision = (& git -C $projectRoot rev-parse HEAD | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Cannot determine the release source revision.' }
}
if ($SourceRevision -notmatch '^[0-9a-fA-F]{40}$') {
    throw 'SourceRevision must be the full commit hash containing this firmware source.'
}
$descriptionPath = Join-Path $BuildDirectory 'project_description.json'
$description = Get-Content -LiteralPath $descriptionPath -Raw | ConvertFrom-Json
if ($description.project_version -ne $Version -or $description.target -ne 'esp32s3') {
    throw 'Existing build version or target does not match this ESP32 release.'
}
$outputRoot = Join-Path $PSScriptRoot 'release'
$packageName = "JohnnyCastaway-ESP32-S3-Touch-LCD-7-$Version"
$stagingRoot = Join-Path $outputRoot $packageName
$archivePath = Join-Path $outputRoot "$packageName.zip"
$resolvedOutputRoot = [IO.Path]::GetFullPath($outputRoot)
$resolvedStagingRoot = [IO.Path]::GetFullPath($stagingRoot)
if (!$resolvedStagingRoot.StartsWith($resolvedOutputRoot + [IO.Path]::DirectorySeparatorChar,
                                    [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe staging path: $resolvedStagingRoot"
}

$inputs = [ordered]@{
    'firmware\bootloader.bin' = Join-Path $BuildDirectory 'bootloader\bootloader.bin'
    'firmware\partition-table.bin' = Join-Path $BuildDirectory 'partition_table\partition-table.bin'
    'firmware\johnny_esp32.bin' = Join-Path $BuildDirectory 'johnny_esp32.bin'
    'FLASHING.md' = Join-Path $projectRoot 'docs\FLASH_ESP32_7_TOUCH.md'
    'LICENSE' = Join-Path $projectRoot 'LICENSE'
    'NOTICE.md' = Join-Path $projectRoot 'NOTICE.md'
    'FLASH_ESP32.bat' = Join-Path $projectRoot 'esp32\release\FLASH_ESP32.bat'
    'Flash-JohnnyEsp32.ps1' = Join-Path $projectRoot 'esp32\release\Flash-JohnnyEsp32.ps1'
    'data\PUT_RESOURCE_FILES_HERE.txt' = Join-Path $projectRoot 'esp32\release\data\PUT_RESOURCE_FILES_HERE.txt'
}
foreach ($source in $inputs.Values) {
    if (!(Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required ESP32 release input is missing: $source"
    }
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
if (Test-Path -LiteralPath $stagingRoot) {
    Remove-Item -LiteralPath $resolvedStagingRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
foreach ($entry in $inputs.GetEnumerator()) {
    $destination = Join-Path $stagingRoot $entry.Key
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $entry.Value -Destination $destination
}

$firmwareRoot = Join-Path $stagingRoot 'firmware'
$checksumLines = foreach ($name in @('bootloader.bin', 'partition-table.bin', 'johnny_esp32.bin')) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $firmwareRoot $name) -Algorithm SHA256).Hash
    "$hash *$name"
}
Set-Content -LiteralPath (Join-Path $firmwareRoot 'SHA256SUMS.txt') -Value $checksumLines -Encoding ascii

$sourceUrl = "https://github.com/DrWize/Castaway-Lookout/tree/$SourceRevision"
$sourceArchive = "https://github.com/DrWize/Castaway-Lookout/archive/$SourceRevision.zip"
$sourceNotice = @"
# Castaway Lookout ESP32 source

Firmware version: $Version. This package reuses an existing normal ESP32 build;
packaging does not compile or flash firmware.

Corresponding project source: [$SourceRevision]($sourceUrl).
[Download that source revision]($sourceArchive).
Build instructions and pinned component requirements are under esp32 in the
source archive. Original Sierra/Dynamix data must be supplied separately.

See LICENSE and NOTICE.md for source licensing and attribution.
Firmware SHA-256 checksums are in firmware/SHA256SUMS.txt.
"@
Set-Content -LiteralPath (Join-Path $stagingRoot 'SOURCE.md') -Value $sourceNotice -Encoding utf8

if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -Path (Join-Path $stagingRoot '*') -DestinationPath $archivePath -CompressionLevel Optimal
$archive = Get-Item -LiteralPath $archivePath
$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
Set-Content -LiteralPath "$archivePath.sha256" -Value "$archiveHash *$packageName.zip" -Encoding ascii
Write-Host "Created: $($archive.FullName)"
Write-Host "Size:    $($archive.Length) bytes"
Write-Host "SHA-256: $archiveHash"
