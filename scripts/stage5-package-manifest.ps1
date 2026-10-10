[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackagePath,[Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
      [Parameter(Mandatory)][string]$SourceRevision)
$ErrorActionPreference='Stop'
if($SourceRevision -notmatch '^[0-9a-f]{40}$') { throw 'Exact source SHA required' }
$archive=[IO.Compression.ZipFile]::Open((Resolve-Path -LiteralPath $PackagePath).Path,[IO.Compression.ZipArchiveMode]::Update)
try {
    $previous=$archive.GetEntry('stage5-package-manifest.json'); if($previous) { $previous.Delete() }
    $files=@(); $pe=@()
    foreach($entry in $archive.Entries) {
        if(!$entry.Name) { continue }
        $stream=$entry.Open(); $sha=[Security.Cryptography.SHA256]::Create()
        try { $hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() } finally { $stream.Dispose(); $sha.Dispose() }
        $name=$entry.FullName.Replace('\','/')
        $files+=@{path=$name;bytes=$entry.Length;sha256=$hash}
        if($name -imatch '\.(exe|dll)$') {
            $stream=$entry.Open(); $reader=[IO.BinaryReader]::new($stream)
            try {
                if($reader.ReadUInt16() -ne 0x5a4d) { throw "Invalid packaged PE: $name" }
                $stream.Position=60; $offset=$reader.ReadUInt32(); $stream.Position=$offset
                if($reader.ReadUInt32() -ne 0x4550) { throw "Invalid packaged PE signature: $name" }
                $machine=$reader.ReadUInt16(); $expected=if($Architecture -eq 'arm64') { 0xaa64 } else { 0x8664 }
                if($machine -ne $expected) { throw "Stage 5 package architecture mismatch: $name" }
                $pe+=@{path=$name;machine=('0x{0:x4}' -f $machine);architecture=$Architecture;sha256=$hash}
            } finally { $reader.Dispose() }
        }
    }
    foreach($required in @('Asteria.exe','pyrowave/pyrowave-runtime.json','pyrowave/shaders/shader-provenance.json','pyrowave/shaders/overlay-provenance.json','STAGE5-OWNER-TEST.md','verify-stage5-owner-package.ps1')) {
        if($files.path -cnotcontains $required) { throw "Missing owner package requirement: $required" }
    }
    # Package identity is indexed here; hardware approval belongs to release-specific evidence.
    $record=@{sourceRevision=$SourceRevision;architecture=$Architecture;qualification='HARDWARE_QUALIFICATION_RECORDED_SEPARATELY';files=$files;peInventory=$pe}
    $stream=$archive.CreateEntry('stage5-package-manifest.json').Open(); $writer=[IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false))
    try { $writer.Write(($record | ConvertTo-Json -Depth 8)) } finally { $writer.Dispose() }
} finally { $archive.Dispose() }
Write-Output "Stage 5 exact-head $Architecture manifest and PE inventory added"
