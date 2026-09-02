param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = "Stop"
$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$probeScript = Join-Path $scriptDirectory "probe_toolchain.py"

if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
    throw "Config file not found: $ConfigPath"
}
if (Test-Path -LiteralPath $OutputPath) {
    throw "Refusing to overwrite existing evidence: $OutputPath"
}

$pythonLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
if ($null -ne $pythonLauncher) {
    & $pythonLauncher.Source -3.11 $probeScript $ConfigPath $OutputPath
} else {
    $python = Get-Command python.exe -ErrorAction Stop
    & $python.Source $probeScript $ConfigPath $OutputPath
}

if ($LASTEXITCODE -ne 0) {
    throw "Read-only toolchain probe failed with exit code $LASTEXITCODE"
}

Write-Host "Read-only probe written to: $OutputPath"
Write-Host "No vendor DLL was loaded and no vendor program was started by this script."
