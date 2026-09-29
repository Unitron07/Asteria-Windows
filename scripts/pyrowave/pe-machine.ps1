function Get-PyroWavePeMachine([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) { throw "Invalid DOS header: $Path" }
        $stream.Position = 0x3c
        $offset = $reader.ReadUInt32()
        if ($offset -gt $stream.Length - 6) { throw "Invalid PE offset: $Path" }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550) { throw "Invalid PE signature: $Path" }
        return $reader.ReadUInt16()
    } finally { $reader.Dispose(); $stream.Dispose() }
}
function Assert-PyroWavePe([string]$Path, [string]$Architecture) {
    $expected = if ($Architecture -eq 'arm64') { 0xaa64 } else { 0x8664 }
    $actual = Get-PyroWavePeMachine $Path
    if ($actual -ne $expected) { throw "PE architecture mismatch in ${Path}: expected $Architecture; machine=$actual" }
    return [pscustomobject]@{ file=$Path; machine=('0x{0:X4}' -f $actual); architecture=$Architecture;
        sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
}
