$ErrorActionPreference = 'Stop'
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('asteria-pyrowave-package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
function Add-Bytes($Archive,[string]$Name,[byte[]]$Bytes) {
    $stream = $Archive.CreateEntry($Name).Open()
    try { $stream.Write($Bytes,0,$Bytes.Length) } finally { $stream.Dispose() }
}
foreach ($case in @('x64','arm64','missing-runtime','missing-metadata','missing-notice','wrong-machine','wrong-architecture',
                    'wrong-codec','wrong-bitstream','wrong-api','wrong-hash','duplicate-runtime','duplicate-metadata','oversized-metadata')) {
    $arch = if ($case -eq 'arm64') { 'arm64' } else { 'x64' }
    $machine = if ($arch -eq 'arm64' -or $case -eq 'wrong-machine') { 0xaa64 } else { 0x8664 }
    $bytes = [byte[]]::new(128)
    $bytes[0]=0x4d; $bytes[1]=0x5a; [BitConverter]::GetBytes([uint32]64).CopyTo($bytes,60)
    [BitConverter]::GetBytes([uint32]0x4550).CopyTo($bytes,64)
    [BitConverter]::GetBytes([uint16]$machine).CopyTo($bytes,68)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
    $metadata = @{codecCommit='186f0393b77f7755953b5ecde994bb1cec2e4155'; bitstreamId='186f0393'; apiVersion='0.6.0'; architecture=$arch; sha256=$hash}
    switch ($case) {
        'wrong-architecture' { $metadata.architecture='arm64' }
        'wrong-codec' { $metadata.codecCommit='wrong' }
        'wrong-bitstream' { $metadata.bitstreamId='00000000' }
        'wrong-api' { $metadata.apiVersion='0.7.0' }
        'wrong-hash' { $metadata.sha256='0' * 64 }
    }
    $metadataText = $metadata | ConvertTo-Json
    if ($case -eq 'oversized-metadata') { $metadataText += ' ' * 4096 }
    $zip = Join-Path $fixtureRoot "$case.zip"
    $archive = [IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
    try {
        Add-Bytes $archive 'Asteria.exe' $bytes
        if ($case -ne 'missing-runtime') { Add-Bytes $archive 'pyrowave/libpyrowave-shared-0.dll' $bytes }
        if ($case -ne 'missing-metadata') { Add-Bytes $archive 'pyrowave/pyrowave-runtime.json' ([Text.Encoding]::UTF8.GetBytes($metadataText)) }
        if ($case -ne 'missing-notice') { Add-Bytes $archive 'pyrowave/source-notices/dependencies.json' ([Text.Encoding]::UTF8.GetBytes('{}')) }
        if ($case -eq 'duplicate-runtime') { Add-Bytes $archive 'PYROWAVE/LIBPYROWAVE-SHARED-0.DLL' $bytes }
        if ($case -eq 'duplicate-metadata') { Add-Bytes $archive 'PYROWAVE/PYROWAVE-RUNTIME.JSON' ([Text.Encoding]::UTF8.GetBytes($metadataText)) }
    } finally { $archive.Dispose() }
    $passed = $true
    try { & (Join-Path $PSScriptRoot 'test-pyrowave-package.ps1') -PackagePath $zip -Architecture $arch -ReportPath (Join-Path $fixtureRoot "$case.json") }
    catch { $passed = $false }
    $expected = $case -in @('x64','arm64')
    if ($passed -ne $expected) { throw "Unexpected package verification result: $case (accepted=$passed)" }
    Write-Output "PASS: $case"
}
Write-Output 'All pinned PyroWave package guard tests passed.'
