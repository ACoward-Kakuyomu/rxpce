[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Tag,
    [string]$SourceDirectory = (Split-Path -Parent $PSScriptRoot),
    [string]$BuildDirectory,
    [string]$OutputDirectory,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Tag -cnotmatch '^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw 'Use a stable release tag in the form vX.Y.Z (for example v1.0.0).'
}
$sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
$projectText = Get-Content -LiteralPath (Join-Path $sourceRoot 'CMakeLists.txt') -Raw
$versionMatch = [regex]::Match($projectText, 'project\(rxpce\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)\s')
if (-not $versionMatch.Success -or $Tag.Substring(1) -cne $versionMatch.Groups[1].Value) {
    throw 'The release tag must match the rxpce project VERSION in CMakeLists.txt.'
}
if ($ValidateOnly) {
    Write-Output "Validated $Tag"
    return
}
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $sourceRoot 'build/Release' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $sourceRoot 'build/packages' }
$buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
$documents = @(
    'LICENSE.md', 'README.md', 'MANUAL.md', 'BUILDING.md', 'CUE_MATERIALIZE.md',
    'THIRD_PARTY_NOTICES.md', 'LICENSES/fpce-xpce.txt',
    'LICENSES/PolyForm-Noncommercial-1.0.0.txt', 'third-party/MCS6502/LICENSE.TXT',
    'third-party/libchdr/LICENSES.rxpce.txt', 'third-party/libchdr/README.rxpce.md',
    'third-party/wtl/MS-PL.txt', 'third-party/picojson/LICENSE'
)
$binaries = @('rxpce.exe', 'cue_materialize.exe')
# Validate every required input before producing any distribution artifacts.
foreach ($document in $documents) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $document) -PathType Leaf)) {
        throw "Missing release document: $document"
    }
}
foreach ($binary in $binaries) {
    if (-not (Test-Path -LiteralPath (Join-Path $buildRoot $binary) -PathType Leaf)) {
        throw "Missing Release binary: $binary"
    }
}
$executableVersion = [System.Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $buildRoot 'rxpce.exe'))
$binaryVersion = '{0}.{1}.{2}' -f $executableVersion.FileMajorPart, $executableVersion.FileMinorPart, $executableVersion.FileBuildPart
if ($binaryVersion -cne $Tag.Substring(1)) {
    throw 'The rxpce.exe version does not match the release tag.'
}

$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
$name = "rxpce-$Tag-windows-x64"
$archive = Join-Path $outputRoot "$name.zip"
$checksum = "$archive.sha256"
if ((Test-Path -LiteralPath $archive) -or (Test-Path -LiteralPath $checksum)) {
    throw 'Release package already exists. Choose a new output folder.'
}
# An isolated stage and an explicit allowlist keep build caches, personal
# settings, media images and unrelated files out of the release ZIP.
$stageParent = Join-Path $outputRoot ('.stage-' + [guid]::NewGuid().ToString('N'))
$stage = Join-Path $stageParent $name
$null = New-Item -ItemType Directory -Path $stage
foreach ($document in $documents) {
    $destination = Join-Path $stage $document
    $null = New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force
    Copy-Item -LiteralPath (Join-Path $sourceRoot $document) -Destination $destination
}
foreach ($binary in $binaries) {
    Copy-Item -LiteralPath (Join-Path $buildRoot $binary) -Destination (Join-Path $stage $binary)
}
Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
$digest = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText($checksum, "$digest  $name.zip`n", [System.Text.UTF8Encoding]::new($false))
Write-Output $archive
Write-Output $checksum
