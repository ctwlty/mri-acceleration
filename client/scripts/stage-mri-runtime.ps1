param(
    [Parameter(Mandatory = $true)][string]$MriSdkRoot,
    [Parameter(Mandatory = $true)][string]$ParameterFile,
    [Parameter(Mandatory = $true)][string]$Destination,
    [string]$ManifestPath = (Join-Path $PSScriptRoot '..\runtime\mri-runtime-manifest.json')
)

$ErrorActionPreference = 'Stop'

function Get-Sha256([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToUpperInvariant()
}

function Get-DirectoryManifestSha256([string]$Path) {
    $records = Get-ChildItem -LiteralPath $Path -Recurse -File -Force |
        Sort-Object { $_.FullName.Substring($Path.Length).TrimStart('\', '/') } |
        ForEach-Object {
            $relativePath = $_.FullName.Substring($Path.Length).TrimStart('\', '/').Replace('\', '/')
            "{0}|{1}|{2}" -f $relativePath, $_.Length, (Get-Sha256 $_.FullName)
        }
    $payload = [System.Text.Encoding]::UTF8.GetBytes(($records -join "`n") + "`n")
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($sha256.ComputeHash($payload))).Replace('-', '')
    } finally {
        $sha256.Dispose()
    }
}

function Test-PathWithin([string]$Path, [string]$Container) {
    $normalizedPath = $Path.TrimEnd('\', '/')
    $normalizedContainer = $Container.TrimEnd('\', '/')
    return $normalizedPath.Equals($normalizedContainer, [System.StringComparison]::OrdinalIgnoreCase) -or
        $normalizedPath.StartsWith($normalizedContainer + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-ExpectedHash([string]$Path, [string]$ExpectedHash) {
    $actualHash = Get-Sha256 $Path
    if ($actualHash -ne $ExpectedHash.ToUpperInvariant()) {
        throw "SHA-256 mismatch for '$Path': expected $ExpectedHash, got $actualHash"
    }
}

function Assert-MsvcRuntimeResolvable {
    $searchDirectories = @([Environment]::GetFolderPath('System')) + ($env:PATH -split ';')
    foreach ($runtimeName in @('vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll')) {
        $runtimePath = $searchDirectories |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            ForEach-Object { Join-Path $_ $runtimeName } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
            Select-Object -First 1
        if (-not $runtimePath) { throw "Required x64 MSVC runtime is not resolvable: $runtimeName" }

        $stream = [System.IO.File]::OpenRead($runtimePath)
        $reader = [System.IO.BinaryReader]::new($stream)
        try {
            $stream.Position = 0x3c
            $peOffset = $reader.ReadInt32()
            $stream.Position = $peOffset + 4
            if ($reader.ReadUInt16() -ne 0x8664) {
                throw "Required MSVC runtime is not x64: $runtimePath"
            }
        } finally {
            $reader.Dispose()
            $stream.Dispose()
        }
    }
}

function Assert-VerifiedRuntime([string]$RuntimeRoot, [string]$ParameterPath, $RuntimeManifest) {
    $dllPath = Join-Path $RuntimeRoot $RuntimeManifest.mridll.relativePath
    if (-not (Test-Path -LiteralPath $dllPath -PathType Leaf)) {
        throw "Required runtime file is missing: $dllPath"
    }
    Assert-ExpectedHash $dllPath $RuntimeManifest.mridll.sha256

    $hwCfgPath = Join-Path $RuntimeRoot $RuntimeManifest.hwCfg.relativePath
    if (-not (Test-Path -LiteralPath $hwCfgPath -PathType Container)) {
        throw "Required runtime directory is missing: $hwCfgPath"
    }
    $hwCfgFiles = @(Get-ChildItem -LiteralPath $hwCfgPath -Recurse -File -Force)
    $hwCfgBytes = [int64](($hwCfgFiles | Measure-Object -Property Length -Sum).Sum)
    if ($hwCfgFiles.Count -ne [int]$RuntimeManifest.hwCfg.fileCount -or $hwCfgBytes -ne [int64]$RuntimeManifest.hwCfg.totalBytes) {
        throw "hw_cfg inventory mismatch: expected $($RuntimeManifest.hwCfg.fileCount) files / $($RuntimeManifest.hwCfg.totalBytes) bytes, got $($hwCfgFiles.Count) files / $hwCfgBytes bytes"
    }
    $hwCfgHash = Get-DirectoryManifestSha256 $hwCfgPath
    if ($hwCfgHash -ne $RuntimeManifest.hwCfg.manifestSha256.ToUpperInvariant()) {
        throw "hw_cfg manifest SHA-256 mismatch: expected $($RuntimeManifest.hwCfg.manifestSha256), got $hwCfgHash"
    }

    if (-not (Test-Path -LiteralPath $ParameterPath -PathType Leaf)) {
        throw "Required parameter file is missing: $ParameterPath"
    }
    if ((Split-Path -Leaf $ParameterPath) -ne $RuntimeManifest.parameterFile.fileName) {
        throw "Unexpected parameter file: expected $($RuntimeManifest.parameterFile.fileName), got $(Split-Path -Leaf $ParameterPath)"
    }
    Assert-ExpectedHash $ParameterPath $RuntimeManifest.parameterFile.sha256
}

if (-not (Test-Path -LiteralPath $MriSdkRoot -PathType Container)) { throw "MRI SDK root does not exist: $MriSdkRoot" }
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) { throw "Runtime manifest does not exist: $ManifestPath" }
$MriSdkRoot = [System.IO.Path]::GetFullPath($MriSdkRoot)
$ParameterFile = [System.IO.Path]::GetFullPath($ParameterFile)
$Destination = [System.IO.Path]::GetFullPath($Destination)
if ((Test-PathWithin $Destination $MriSdkRoot) -or (Test-PathWithin $MriSdkRoot $Destination)) {
    throw 'Destination must not overlap the MRI SDK source.'
}
if (Test-PathWithin $ParameterFile $Destination) {
    throw 'Destination must not overlap the parameter file.'
}
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json

Assert-VerifiedRuntime $MriSdkRoot $ParameterFile $manifest
Assert-MsvcRuntimeResolvable

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$stagedDll = Join-Path $Destination $manifest.mridll.relativePath
$stagedHwCfg = Join-Path $Destination $manifest.hwCfg.relativePath
$stagedParameterDirectory = Join-Path $Destination 'profiles'
$stagedParameter = Join-Path $stagedParameterDirectory $manifest.parameterFile.fileName

if (Test-Path -LiteralPath $stagedDll) { Remove-Item -LiteralPath $stagedDll -Force }
if (Test-Path -LiteralPath $stagedHwCfg) { Remove-Item -LiteralPath $stagedHwCfg -Recurse -Force }
if (Test-Path -LiteralPath $stagedParameter) { Remove-Item -LiteralPath $stagedParameter -Force }
Copy-Item -LiteralPath (Join-Path $MriSdkRoot $manifest.mridll.relativePath) -Destination $stagedDll
Copy-Item -LiteralPath (Join-Path $MriSdkRoot $manifest.hwCfg.relativePath) -Destination $stagedHwCfg -Recurse
New-Item -ItemType Directory -Force -Path $stagedParameterDirectory | Out-Null
Copy-Item -LiteralPath $ParameterFile -Destination $stagedParameter

Assert-VerifiedRuntime $Destination $stagedParameter $manifest
Write-Output "MRI_RUNTIME_STAGED $Destination"
