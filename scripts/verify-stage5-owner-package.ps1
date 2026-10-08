[CmdletBinding()]
param([Parameter(Mandatory)][string]$ExpectedSourceRevision,[Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
      [string]$PackageRoot=$PSScriptRoot,[string]$ArchivePath,[string]$ExpectedArchiveSha256)
$ErrorActionPreference='Stop'
if($ArchivePath) {
    $hash=(Get-FileHash -LiteralPath $ArchivePath).Hash.ToLowerInvariant()
    if(!$ExpectedArchiveSha256 -or $hash -cne $ExpectedArchiveSha256.ToLowerInvariant()) { throw 'Owner ZIP SHA-256 mismatch' }
}
$manifest=Get-Content (Join-Path $PackageRoot 'stage5-package-manifest.json') -Raw | ConvertFrom-Json
if($manifest.sourceRevision -cne $ExpectedSourceRevision -or $manifest.architecture -cne $Architecture) { throw 'Owner source/architecture mismatch' }
$resolvedRoot=[IO.Path]::GetFullPath($PackageRoot).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
foreach($file in $manifest.files) {
    $path=[IO.Path]::GetFullPath((Join-Path $resolvedRoot $file.path))
    if(!$path.StartsWith($resolvedRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid manifest path' }
    if(!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $file.sha256) { throw "Owner file hash mismatch: $($file.path)" }
}
$runtime=Get-Content (Join-Path $PackageRoot 'pyrowave/pyrowave-runtime.json') -Raw | ConvertFrom-Json
if($runtime.codecCommit -cne '186f0393b77f7755953b5ecde994bb1cec2e4155' -or $runtime.bitstreamId -cne '186f0393' -or $runtime.apiVersion -cne '0.6.0' -or $runtime.architecture -cne $Architecture -or $runtime.graniteCommit -cne 'b6cffd5ce81f540f0855e6778428483e14763d9b' -or $runtime.factoryCleanupPatchSha256 -cne '8fe5906706bb27814ed7344f8932cd5ccf00c368ed24d253839d13cfeeb78c86') { throw 'Pinned runtime identity mismatch' }
if((Get-FileHash (Join-Path $PackageRoot 'pyrowave/libpyrowave-shared-0.dll')).Hash.ToLowerInvariant() -cne $runtime.sha256) { throw 'Runtime provenance hash mismatch' }
foreach($name in @('shader-provenance.json','overlay-provenance.json')) {
    $record=Get-Content (Join-Path $PackageRoot "pyrowave/shaders/$name") -Raw | ConvertFrom-Json
    foreach($file in $record.files) {
        if((Get-FileHash (Join-Path $PackageRoot "pyrowave/shaders/$($file.source)")).Hash.ToLowerInvariant() -cne $file.sourceSha256 -or
           (Get-FileHash (Join-Path $PackageRoot "pyrowave/shaders/$($file.spirv)")).Hash.ToLowerInvariant() -cne $file.spirvSha256) { throw 'Shader provenance mismatch' }
    }
}
foreach($item in $manifest.peInventory) {
    $stream=[IO.File]::OpenRead((Join-Path $PackageRoot $item.path)); $reader=[IO.BinaryReader]::new($stream)
    try {
        $stream.Position=60; $offset=$reader.ReadUInt32(); $stream.Position=$offset+4;
        if(('0x{0:x4}' -f $reader.ReadUInt16()) -cne $item.machine) { throw 'Owner PE inventory mismatch' }
    } finally { $reader.Dispose() }
}
Write-Output "PASS exact source $ExpectedSourceRevision / $Architecture / all manifest hashes / runtime and shader provenance / PE inventory"
Write-Output 'Launch Asteria.exe normally and connect to Vibepollo with explicit PyroWave. No authentication is automated.'
