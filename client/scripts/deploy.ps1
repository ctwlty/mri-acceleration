param(
    [string]$BuildRoot = '',
    [string]$DistRoot = ''
)

$ErrorActionPreference = 'Stop'
$clientRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $clientRoot 'build-release'
}
$BuildRoot = [System.IO.Path]::GetFullPath($BuildRoot)
$distRoot = if ([string]::IsNullOrWhiteSpace($DistRoot)) {
    [System.IO.Path]::GetFullPath((Join-Path $clientRoot 'dist'))
} else {
    [System.IO.Path]::GetFullPath($DistRoot)
}
$clientPrefix = $clientRoot + [System.IO.Path]::DirectorySeparatorChar
$buildPrefix = $BuildRoot + [System.IO.Path]::DirectorySeparatorChar
if (-not $distRoot.StartsWith($clientPrefix, [System.StringComparison]::OrdinalIgnoreCase) -and
    -not $distRoot.StartsWith($buildPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
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
$proxySource = Join-Path $clientRoot 'tools\eggcontroller_proxy.py'
if (-not (Test-Path -LiteralPath $proxySource)) { throw "Automation proxy not found: $proxySource" }
$toolsTarget = Join-Path $distRoot 'tools'
New-Item -ItemType Directory -Path $toolsTarget | Out-Null
$proxyTarget = Join-Path $toolsTarget 'eggcontroller_proxy.py'
Copy-Item -LiteralPath $proxySource -Destination $proxyTarget
if ((Get-FileHash -LiteralPath $proxySource -Algorithm SHA256).Hash -ne
    (Get-FileHash -LiteralPath $proxyTarget -Algorithm SHA256).Hash) {
    throw "Automation proxy hash mismatch after deployment: $proxyTarget"
}

$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
$guiTarget = Join-Path $distRoot 'scenario_nmr_client.exe'
& 'C:\msys64\ucrt64\bin\windeployqt6.exe' --release --no-translations --compiler-runtime $guiTarget
if ($LASTEXITCODE -ne 0) { throw "windeployqt6 failed with $LASTEXITCODE" }

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
