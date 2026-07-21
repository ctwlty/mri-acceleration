param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$BuildRoot = ''
)

$ErrorActionPreference = 'Stop'
$clientRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $defaultBuildName = if ($Configuration -eq 'Release') { 'build-release' } else { 'build' }
    $BuildRoot = Join-Path $clientRoot $defaultBuildName
}
$BuildRoot = [System.IO.Path]::GetFullPath($BuildRoot)
$cmake = 'C:\msys64\ucrt64\bin\cmake.exe'
$ucrtBin = 'C:\msys64\ucrt64\bin'

if (-not (Test-Path -LiteralPath $cmake)) {
    throw "MSYS2 UCRT64 CMake not found: $cmake"
}

$env:PATH = "$ucrtBin;$env:PATH"
$testing = if ($Configuration -eq 'Debug') { 'ON' } else { 'OFF' }
& $cmake -S $clientRoot -B $BuildRoot -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" "-DBUILD_TESTING=$testing"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with $LASTEXITCODE" }
& $cmake --build $BuildRoot
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with $LASTEXITCODE" }

if ($Configuration -eq 'Debug') {
    & 'C:\msys64\ucrt64\bin\ctest.exe' --test-dir $BuildRoot --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "CTest failed with $LASTEXITCODE" }
}

Get-Item -LiteralPath (Join-Path $BuildRoot 'scenario_nmr_client.exe'), (Join-Path $BuildRoot 'mri_sdk_verify.exe') |
    Select-Object FullName, Length, LastWriteTime
