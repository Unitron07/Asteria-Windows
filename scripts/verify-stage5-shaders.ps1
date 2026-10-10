[CmdletBinding()]
param([Parameter(Mandatory)][string]$Compiler,[Parameter(Mandatory)][string]$OutputRoot)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$production=Join-Path $root 'app/streaming/video/shaders/pyrowave'
$qualified=Join-Path $root 'tests/pyrowave/vulkan/shaders'
& (Join-Path $PSScriptRoot 'generate-stage4-shaders.ps1') -Compiler $Compiler -OutputRoot (Join-Path $OutputRoot 'video') -Verify
foreach($name in @('video.vert','video.frag','video.vert.spv','video.frag.spv','video_spirv.h','shader-provenance.json')) {
    if((Get-FileHash (Join-Path $production $name)).Hash -cne (Get-FileHash (Join-Path $qualified $name)).Hash) { throw "Qualified Stage 4 bytes changed: $name" }
}
$record=Get-Content (Join-Path $production 'overlay-provenance.json') -Raw | ConvertFrom-Json
if((Get-FileHash $Compiler).Hash.ToLowerInvariant() -cne $record.compilerSha256) { throw 'Overlay compiler provenance mismatch' }
$output=Join-Path $OutputRoot 'overlay.frag.spv'
& $Compiler -V --target-env vulkan1.0 -o $output (Join-Path $production 'overlay.frag')
if($LASTEXITCODE) { throw 'Overlay shader reproduction failed' }
if((Get-FileHash $output).Hash.ToLowerInvariant() -cne $record.files[0].spirvSha256 -or
   (Get-FileHash (Join-Path $production 'overlay.frag')).Hash.ToLowerInvariant() -cne $record.files[0].sourceSha256 -or
   (Get-FileHash (Join-Path $production 'overlay_spirv.h')).Hash.ToLowerInvariant() -cne $record.embeddedHeaderSha256) { throw 'Overlay shader hash mismatch' }
$bytes=[IO.File]::ReadAllBytes($output)
$header=[IO.File]::ReadAllText((Join-Path $production 'overlay_spirv.h'))
$words=@([regex]::Matches($header,'0x([0-9a-f]{8})u') | ForEach-Object { [Convert]::ToUInt32($_.Groups[1].Value,16) })
if($words.Count*4 -ne $bytes.Length) { throw 'Overlay embedded SPIR-V size mismatch' }
for($i=0;$i -lt $words.Count;++$i) { if($words[$i] -ne [BitConverter]::ToUInt32($bytes,$i*4)) { throw 'Overlay embedded SPIR-V word mismatch' } }
Write-Output 'PASS: qualified Stage 4 bytes unchanged; both shaders reproduced; separate overlay source/SPIR-V/header provenance verified'
