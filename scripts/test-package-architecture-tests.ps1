# Synthetic PE headers exercise package inspection; these are not runnable binaries.
$ErrorActionPreference = 'Stop'
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('artemis-package-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$validator = Join-Path $PSScriptRoot 'test-package-architecture.ps1'
function New-PeHeader([uint16]$Machine) {
    $bytes = [byte[]]::new(256)
    $bytes[0] = 0x4D; $bytes[1] = 0x5A
    [BitConverter]::GetBytes([uint32]64).CopyTo($bytes, 60)
    [BitConverter]::GetBytes([uint32]0x4550).CopyTo($bytes, 64)
    [BitConverter]::GetBytes($Machine).CopyTo($bytes, 68)
    [BitConverter]::GetBytes([uint16]112).CopyTo($bytes, 84)
    [BitConverter]::GetBytes([uint16]0x020B).CopyTo($bytes, 88)
    return ,$bytes
}
function Test-Package([string]$Name, [hashtable]$Files, [string]$Architecture, [string]$ExpectedError, [string]$ClientExecutable = 'Asteria.exe') {
    $zipPath = Join-Path $scratch "$Name.zip"
    $reportPath = Join-Path $scratch "$Name.json"
    $zip = [IO.Compression.ZipFile]::Open($zipPath, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($path in $Files.Keys) {
            $stream = $zip.CreateEntry($path).Open()
            try { $stream.Write($Files[$path], 0, $Files[$path].Length) } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
    $caught = $null
    try { & $validator -PackagePath $zipPath -Architecture $Architecture -ReportPath $reportPath -ClientExecutable $ClientExecutable }
    catch { $caught = $_.Exception.Message }
    if ($ExpectedError) {
        if (!$caught -or !$caught.Contains($ExpectedError)) { throw "$Name did not reject as expected: $caught" }
    } elseif ($caught) { throw $caught }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if ($report.passed -ne (!$ExpectedError)) { throw "$Name report status is incorrect" }
    if ($report.sha256 -ne (Get-FileHash $zipPath -Algorithm SHA256).Hash) { throw "$Name report hash is incorrect" }
    $binaryCount = @($Files.Keys | Where-Object { $_ -match '(?i)\.(exe|dll)$' }).Count
    if ($report.binaries.Count -ne $binaryCount) { throw "$Name omitted a binary" }
    Write-Host "PASS: $Name"
}
try {
    foreach ($arch in @('arm64', 'x64')) {
        $machine = if ($arch -eq 'arm64') { 0xAA64 } else { 0x8664 }
        $files = @{
            'Asteria.exe' = (New-PeHeader $machine)
            'AntiHooking.dll' = (New-PeHeader $machine)
            'plugins/platforms/qwindows.DLL' = (New-PeHeader $machine)
            'qml/nested/plugin.dll' = (New-PeHeader $machine)
            'source-notices/LICENSE' = [Text.Encoding]::UTF8.GetBytes('notice')
        }
        Test-Package "valid-$arch" $files $arch ''
        $upstreamFiles = @{'Moonlight.exe' = (New-PeHeader $machine); 'SDL2.dll' = (New-PeHeader $machine)}
        Test-Package "upstream-$arch" $upstreamFiles $arch '' 'Moonlight.exe'
        Test-Package "upstream-wrong-client-$arch" $upstreamFiles $arch 'missing Asteria.exe'
        $wrong = if ($arch -eq 'arm64') { 0x8664 } else { 0xAA64 }
        $files['plugins/platforms/contaminant.dll'] = New-PeHeader $wrong
        Test-Package "contaminated-$arch" $files $arch 'contaminant.dll: Machine'
    }
    foreach ($machine in @(0x014C, 0xA641, 0xA64E, 0)) {
        Test-Package "unsupported-$machine" @{'Asteria.exe' = (New-PeHeader 0xAA64); 'bad.dll' = (New-PeHeader $machine)} arm64 'bad.dll: Machine'
    }
    foreach ($kind in @('dos', 'offset', 'signature', 'optional', 'truncated')) {
        $bad = New-PeHeader 0xAA64
        switch ($kind) {
            dos { $bad[0] = 0 }
            offset { [BitConverter]::GetBytes([uint32]::MaxValue).CopyTo($bad, 60) }
            signature { $bad[64] = 0 }
            optional { $bad[88] = 0 }
            truncated { $bad = [byte[]]@(1, 2) }
        }
        Test-Package "malformed-$kind" @{'Asteria.exe' = (New-PeHeader 0xAA64); 'nested/bad.dll' = $bad} arm64 'nested/bad.dll:'
    }
    Test-Package empty @{} arm64 'missing Asteria.exe'
    Test-Package missing-client @{'Qt6Core.dll' = (New-PeHeader 0xAA64)} arm64 'missing Asteria.exe'
    Test-Package missing-runtime @{'Asteria.exe' = (New-PeHeader 0xAA64)} arm64 'no runtime DLLs'
    Write-Host 'All package architecture tests passed.'
} finally {
    $resolvedScratch = [IO.Path]::GetFullPath($scratch)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (!$resolvedScratch.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe scratch cleanup path' }
    Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
}
