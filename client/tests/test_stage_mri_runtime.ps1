$ErrorActionPreference = 'Stop'

$clientRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$stageScript = Join-Path $clientRoot 'scripts\stage-mri-runtime.ps1'
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mri-runtime-stage-test-" + [guid]::NewGuid().ToString('N'))

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

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

function Invoke-StagingExpectFailure([string]$MriSdkRoot, [string]$ParameterFile, [string]$Destination, [string]$ManifestPath, [string]$ExpectedText) {
    $savedErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $stageScript `
            -MriSdkRoot $MriSdkRoot -ParameterFile $ParameterFile -Destination $Destination -ManifestPath $ManifestPath 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    Assert-True ($exitCode -ne 0) 'Expected staging to fail.'
    Assert-True (($output | Out-String) -match [regex]::Escape($ExpectedText)) "Expected error containing '$ExpectedText', got: $output"
}

try {
    $sdkRoot = Join-Path $tempRoot 'sdk'
    $hwCfgRoot = Join-Path $sdkRoot 'hw_cfg'
    $profileRoot = Join-Path $sdkRoot 'profiles'
    New-Item -ItemType Directory -Force -Path $hwCfgRoot, (Join-Path $hwCfgRoot 'nested'), $profileRoot | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $sdkRoot 'mridll.dll'), 'fake runtime dll', [System.Text.Encoding]::ASCII)
    [System.IO.File]::WriteAllText((Join-Path $sdkRoot 'mridll.dll.backup_20250303'), 'backup dll', [System.Text.Encoding]::ASCII)
    [System.IO.File]::WriteAllText((Join-Path $hwCfgRoot 'controller.ini'), 'controller=1', [System.Text.Encoding]::ASCII)
    [System.IO.File]::WriteAllText((Join-Path $hwCfgRoot 'nested\calibration.cfg'), 'gain=42', [System.Text.Encoding]::ASCII)
    $parameterFile = Join-Path $profileRoot 'PTScan.par'
    [System.IO.File]::WriteAllText($parameterFile, 'PTScan', [System.Text.Encoding]::ASCII)
    [System.IO.File]::WriteAllText((Join-Path $profileRoot 'unused.par'), 'unused', [System.Text.Encoding]::ASCII)

    $manifestPath = Join-Path $tempRoot 'fake-manifest.json'
    $hwFiles = @(Get-ChildItem -LiteralPath $hwCfgRoot -Recurse -File -Force)
    $manifest = [ordered]@{
        mridll = [ordered]@{ relativePath = 'mridll.dll'; sha256 = Get-Sha256 (Join-Path $sdkRoot 'mridll.dll') }
        hwCfg = [ordered]@{
            relativePath = 'hw_cfg'
            fileCount = $hwFiles.Count
            totalBytes = [int64](($hwFiles | Measure-Object -Property Length -Sum).Sum)
            manifestSha256 = Get-DirectoryManifestSha256 $hwCfgRoot
            initSha256 = Get-Sha256 (Join-Path $hwCfgRoot 'controller.ini')
        }
        parameterFile = [ordered]@{ fileName = 'PTScan.par'; sha256 = Get-Sha256 $parameterFile }
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

    $destination = Join-Path $tempRoot 'staged'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $stageScript `
        -MriSdkRoot $sdkRoot -ParameterFile $parameterFile -Destination $destination -ManifestPath $manifestPath
    Assert-True ($LASTEXITCODE -eq 0) 'Staging should succeed for verified fake assets.'
    Assert-True (Test-Path -LiteralPath (Join-Path $destination 'mridll.dll')) 'Staged DLL is missing.'
    Assert-True (Test-Path -LiteralPath (Join-Path $destination 'hw_cfg\controller.ini')) 'Staged hw_cfg content is missing.'
    Assert-True (Test-Path -LiteralPath (Join-Path $destination 'hw_cfg\nested\calibration.cfg')) 'Staged nested hw_cfg content is missing.'
    Assert-True (Test-Path -LiteralPath (Join-Path $destination 'profiles\PTScan.par')) 'Staged parameter file is missing.'
    Assert-True (Test-Path -LiteralPath (Join-Path $destination 'mri-runtime-manifest.json')) 'Staged runtime manifest is missing.'
    Assert-True ((Get-Content -LiteralPath (Join-Path $destination 'mri-runtime-manifest.json') -Raw | ConvertFrom-Json).mridll.sha256 -eq $manifest.mridll.sha256) 'Staged runtime manifest must preserve the verified DLL hash.'
    Assert-True ((Get-Content -LiteralPath (Join-Path $destination 'mri-runtime-manifest.json') -Raw | ConvertFrom-Json).hwCfg.initSha256 -eq $manifest.hwCfg.initSha256) 'Staged runtime manifest must preserve the verified init hash.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $destination 'mridll.dll.backup_20250303'))) 'Backup DLL must not be staged.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $destination 'profiles\unused.par'))) 'Unused parameter files must not be staged.'

    Remove-Item -LiteralPath (Join-Path $sdkRoot 'mridll.dll') -Force
    Invoke-StagingExpectFailure $sdkRoot $parameterFile (Join-Path $tempRoot 'missing-output') $manifestPath 'Required runtime file is missing'

    [System.IO.File]::WriteAllText((Join-Path $sdkRoot 'mridll.dll'), 'tampered runtime dll', [System.Text.Encoding]::ASCII)
    Invoke-StagingExpectFailure $sdkRoot $parameterFile (Join-Path $tempRoot 'hash-output') $manifestPath 'SHA-256 mismatch'

    Invoke-StagingExpectFailure $sdkRoot $parameterFile $sdkRoot $manifestPath 'Destination must not overlap the MRI SDK source'

    Write-Output 'STAGE_MRI_RUNTIME_TEST_OK'
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
