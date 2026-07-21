param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Before', 'After')]
    [string]$Phase,
    [string]$OutputPath = 'D:\mri_data\par0423-3',
    [string]$StateFile = ''
)

$ErrorActionPreference = 'Stop'
$clientRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testOutput = Join-Path $clientRoot 'test-output'
if ([string]::IsNullOrWhiteSpace($StateFile)) {
    $StateFile = Join-Path $testOutput 'real-sdk-raw-before.json'
}
New-Item -ItemType Directory -Path $testOutput -Force | Out-Null

if (-not (Test-Path -LiteralPath $OutputPath -PathType Container)) {
    throw "MRI output directory does not exist: $OutputPath"
}

$snapshot = @(Get-ChildItem -LiteralPath $OutputPath -Filter '*.raw' -File |
    Select-Object FullName, Length, LastWriteTimeUtc,
        @{Name = 'LastWriteTimeUtcIso'; Expression = { $_.LastWriteTimeUtc.ToString('o') }})

if ($Phase -eq 'Before') {
    $snapshot | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $StateFile -Encoding UTF8
    Write-Output "RAW_BEFORE count=$($snapshot.Count) state=$StateFile"
    exit 0
}

if (-not (Test-Path -LiteralPath $StateFile -PathType Leaf)) {
    throw "Before snapshot is missing: $StateFile"
}
$before = Get-Content -Raw -LiteralPath $StateFile -Encoding UTF8 | ConvertFrom-Json
$beforeSignatures = @{}
foreach ($item in $before) {
    $beforeSignatures[$item.FullName] = "$($item.Length):$($item.LastWriteTimeUtcIso)"
}
$newRaw = @($snapshot | Where-Object {
    $_.Length -gt 0 -and (
        -not $beforeSignatures.ContainsKey($_.FullName) -or
        $beforeSignatures[$_.FullName] -ne "$($_.Length):$($_.LastWriteTimeUtcIso)")
})
$resultFile = Join-Path $testOutput 'real-sdk-new-raw.json'
$newRaw | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $resultFile -Encoding UTF8
if ($newRaw.Count -eq 0) {
    throw "No new or updated non-empty RAW file was produced in $OutputPath"
}
$newRaw | Format-Table FullName, Length, LastWriteTimeUtc -AutoSize
Write-Output "RAW_AFTER_OK count=$($newRaw.Count) result=$resultFile"
