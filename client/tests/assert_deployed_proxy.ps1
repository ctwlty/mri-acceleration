param(
    [Parameter(Mandatory = $true)]
    [string]$DistRoot
)

$ErrorActionPreference = 'Stop'
$clientRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = Join-Path $clientRoot 'tools\eggcontroller_proxy.py'
$deployed = Join-Path ([System.IO.Path]::GetFullPath($DistRoot)) 'tools\eggcontroller_proxy.py'

if (-not (Test-Path -LiteralPath $source)) { throw "Source proxy missing: $source" }
if (-not (Test-Path -LiteralPath $deployed)) { throw "Deployed proxy missing: $deployed" }

$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$deployedHash = (Get-FileHash -LiteralPath $deployed -Algorithm SHA256).Hash
if ($sourceHash -ne $deployedHash) {
    throw "Deployed proxy hash mismatch: source=$sourceHash deployed=$deployedHash"
}

Write-Output "PROXY_DEPLOY_OK $deployed $deployedHash"
