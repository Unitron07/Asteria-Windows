[CmdletBinding()]
param([Parameter(Mandatory)][string]$Compiler,[string]$OutputRoot,[switch]$Verify)
$ErrorActionPreference='Stop'
$source=Join-Path (Split-Path $PSScriptRoot -Parent) 'tests/pyrowave/vulkan/shaders'
if (!$OutputRoot) { $OutputRoot=$source }
New-Item -ItemType Directory -Force $OutputRoot | Out-Null
$Compiler=(Resolve-Path -LiteralPath $Compiler).Path
$version=(& $Compiler --version) -join "`n"
if ($LASTEXITCODE -or $version -notmatch '8\.13\.3559') { throw 'Requires reviewed glslang 8.13.3559' }
$compilerHash=(Get-FileHash -LiteralPath $Compiler).Hash.ToLowerInvariant()
$provenance=@{compilerVersion=$version;compilerSha256=$compilerHash;target='Vulkan 1.0 / SPIR-V 1.0';
    compilerArchiveUrl='https://github.com/KhronosGroup/glslang/releases/download/8.13.3559/glslang-master-windows-x64-Release.zip';
    compilerArchiveSha256='e4bffacd4a75e1e150c0cf19d5020cfa6893386a1e6dd14ef6426789859eab88';files=@()}
$header="#pragma once`n#include <cstdint>`nnamespace Stage4 {`n"
foreach ($entry in @(@('video.vert','VideoVertex'),@('video.frag','VideoFragment'))) {
    $input=Join-Path $source $entry[0]; $output=Join-Path $OutputRoot "$($entry[0]).spv"
    & $Compiler -V --target-env vulkan1.0 -o $output $input
    if ($LASTEXITCODE) { throw 'Shader compilation failed' }
    $bytes=[IO.File]::ReadAllBytes($output)
    if ($bytes.Length%4 -or [BitConverter]::ToUInt32($bytes,0) -ne 0x07230203) { throw 'Invalid SPIR-V' }
    $words=for ($i=0;$i -lt $bytes.Length;$i+=4) { '0x{0:x8}u' -f [BitConverter]::ToUInt32($bytes,$i) }
    $header+="inline constexpr uint32_t $($entry[1])[] = {`n"
    for ($i=0;$i -lt $words.Count;$i+=8) { $header+='    '+($words[$i..([Math]::Min($i+7,$words.Count-1))] -join ',')+",`n" }
    $header+="};`n"
    $provenance.files+=@{source=$entry[0];sourceSha256=(Get-FileHash $input).Hash.ToLowerInvariant();
        spirv="$($entry[0]).spv";spirvSha256=(Get-FileHash $output).Hash.ToLowerInvariant();arguments='-V --target-env vulkan1.0'}
    if ($Verify -and (Get-FileHash $output).Hash -cne (Get-FileHash (Join-Path $source "$($entry[0]).spv")).Hash) { throw "SPIR-V reproduction mismatch: $($entry[0])" }
}
$header+="}`n"
[IO.File]::WriteAllText((Join-Path $OutputRoot 'video_spirv.h'),$header,[Text.UTF8Encoding]::new($false))
if ($Verify) {
    if ((Get-FileHash (Join-Path $OutputRoot 'video_spirv.h')).Hash -cne (Get-FileHash (Join-Path $source 'video_spirv.h')).Hash) { throw 'Embedded shader mismatch' }
    $record=Get-Content (Join-Path $source 'shader-provenance.json') -Raw | ConvertFrom-Json
    if ($record.compilerSha256 -cne $compilerHash) { throw 'Compiler hash mismatch' }
} else {
    $provenance.embeddedHeaderSha256=(Get-FileHash (Join-Path $OutputRoot 'video_spirv.h')).Hash.ToLowerInvariant()
    $provenance | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputRoot 'shader-provenance.json') -Encoding utf8
}
