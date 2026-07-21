param(
    [string]$BuildRoot = '',
    [string]$MriSdkRoot = '',
    [string]$ParameterFile = '',
    [switch]$QtOnly
)

$ErrorActionPreference = 'Stop'
if ($QtOnly) {
    if (-not [string]::IsNullOrWhiteSpace($MriSdkRoot) -or -not [string]::IsNullOrWhiteSpace($ParameterFile)) {
        throw 'QtOnly cannot be combined with MRI runtime source parameters.'
    }
} elseif ([string]::IsNullOrWhiteSpace($MriSdkRoot) -or [string]::IsNullOrWhiteSpace($ParameterFile)) {
    throw 'Provide both -MriSdkRoot and -ParameterFile, or explicitly use -QtOnly.'
}
$clientRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $clientRoot 'build-release'
}
$BuildRoot = [System.IO.Path]::GetFullPath($BuildRoot)
$distRoot = [System.IO.Path]::GetFullPath((Join-Path $clientRoot 'dist'))
$clientPrefix = $clientRoot + [System.IO.Path]::DirectorySeparatorChar
if (-not $distRoot.StartsWith($clientPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe dist target: $distRoot"
}

$guiSource = Join-Path $BuildRoot 'scenario_nmr_client.exe'
$verifySource = Join-Path $BuildRoot 'mri_sdk_verify.exe'
foreach ($source in @($guiSource, $verifySource)) {
    if (-not (Test-Path -LiteralPath $source)) { throw "Release executable not found: $source" }
}

if (Test-Path -LiteralPath $distRoot) {
    Remove-Item -LiteralPath $distRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $distRoot | Out-Null
Copy-Item -LiteralPath $guiSource, $verifySource -Destination $distRoot

$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
$guiTarget = Join-Path $distRoot 'scenario_nmr_client.exe'
& 'C:\msys64\ucrt64\bin\windeployqt6.exe' --release --no-translations --compiler-runtime $guiTarget
if ($LASTEXITCODE -ne 0) { throw "windeployqt6 failed with $LASTEXITCODE" }

if (-not $QtOnly) {
    & (Join-Path $PSScriptRoot 'stage-mri-runtime.ps1') -MriSdkRoot $MriSdkRoot -ParameterFile $ParameterFile -Destination (Join-Path $distRoot 'mri-runtime')
    if ($LASTEXITCODE -ne 0) { throw "MRI runtime staging failed with $LASTEXITCODE" }
}

$objdump = 'C:\msys64\ucrt64\bin\objdump.exe'
$ucrtBin = 'C:\msys64\ucrt64\bin'
$queue = [System.Collections.Generic.Queue[string]]::new()
$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
Get-ChildItem -LiteralPath $distRoot -Recurse -File |
    Where-Object { $_.Extension -in '.exe', '.dll' } |
    ForEach-Object { $queue.Enqueue($_.FullName) }
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $seen.Add($binary)) { continue }
    $imports = & $objdump -p $binary |
        Select-String 'DLL Name:\s*(.+)$' |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
    foreach ($import in $imports) {
        if ($import -match '^(vcruntime|msvcp|concrt)\d.*\.dll$') { continue }
        $source = Join-Path $ucrtBin $import
        $destination = Join-Path $distRoot $import
        if ((Test-Path -LiteralPath $source) -and -not (Test-Path -LiteralPath $destination)) {
            Copy-Item -LiteralPath $source -Destination $destination
            $queue.Enqueue($destination)
        }
    }
}

foreach ($required in @('Qt6Core.dll', 'Qt6Widgets.dll', 'platforms\qwindows.dll')) {
    $requiredPath = Join-Path $distRoot $required
    if (-not (Test-Path -LiteralPath $requiredPath)) { throw "Deployed runtime missing: $requiredPath" }
}

$originalPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    & (Join-Path $distRoot 'mri_sdk_verify.exe') --help | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Deployed verifier failed to start with code $LASTEXITCODE" }
    $smoke = Start-Process -FilePath $guiTarget -WorkingDirectory $distRoot -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds 3
    if ($smoke.HasExited) {
        throw "Deployed Qt client exited during smoke test with code $($smoke.ExitCode)"
    }
    Stop-Process -Id $smoke.Id
} finally {
    $env:PATH = $originalPath
}

Get-ChildItem -LiteralPath $distRoot -File -Recurse |
    Measure-Object -Property Length -Sum |
    Select-Object Count, Sum
Write-Output "DEPLOY_OK $guiTarget"
