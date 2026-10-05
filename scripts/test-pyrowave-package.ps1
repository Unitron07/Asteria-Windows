[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$ReportPath
)
$ErrorActionPreference = 'Stop'
$archive = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $PackagePath).Path)
try {
    function Get-Entry([string]$Name) {
        $entries = @($archive.Entries | Where-Object { $_.FullName.Replace('\','/') -ieq $Name })
        if ($entries.Count -ne 1) { throw "Expected exactly one package entry: $Name" }
        return $entries[0]
    }
    Get-Entry 'Asteria.exe' | Out-Null
    Get-Entry 'pyrowave/source-notices/dependencies.json' | Out-Null
    $metadataEntry = Get-Entry 'pyrowave/pyrowave-runtime.json'
    if ($metadataEntry.Length -gt 4096) { throw 'Oversized packaged runtime metadata' }
    $stream = $metadataEntry.Open()
    $reader = [IO.StreamReader]::new($stream)
    try { $metadata = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    $stream = (Get-Entry 'pyrowave/libpyrowave-shared-0.dll').Open()
    $memory = [IO.MemoryStream]::new()
    try { $stream.CopyTo($memory); $bytes = $memory.ToArray() } finally { $stream.Dispose(); $memory.Dispose() }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant() } finally { $sha.Dispose() }
    if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5a4d) { throw 'Invalid runtime DOS header' }
    $offset = [BitConverter]::ToUInt32($bytes,60)
    if ($offset -lt 64 -or [long]$offset + 6 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes,$offset) -ne 0x4550) { throw 'Invalid runtime PE header' }
    $machine = [BitConverter]::ToUInt16($bytes,$offset+4)
    $expected = if ($Architecture -eq 'arm64') { 0xaa64 } else { 0x8664 }
    if ($machine -ne $expected -or $metadata.architecture -cne $Architecture) { throw 'Packaged PyroWave architecture mismatch' }
    if ($metadata.codecCommit -cne '186f0393b77f7755953b5ecde994bb1cec2e4155' -or
        $metadata.bitstreamId -cne '186f0393' -or $metadata.apiVersion -cne '0.6.0' -or $metadata.sha256 -cne $hash) {
        throw 'Packaged PyroWave provenance mismatch'
    }
    $metadata | ConvertTo-Json | Set-Content -LiteralPath $ReportPath -Encoding utf8
    Write-Output "PASS: normal $Architecture package contains pinned, hash-verified PyroWave"
} finally { $archive.Dispose() }
