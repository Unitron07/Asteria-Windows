[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
      [Parameter(Mandatory)][string]$BuildRoot,[Parameter(Mandatory)][string]$DependencyRoot)
$ErrorActionPreference = 'Stop'
$source = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'pyrowave/runtime-provenance.ps1')
$metadata = Assert-PyroWaveRuntime $DependencyRoot $Architecture
$stage = Join-Path $source "build/stage3-owner-$Architecture"
if (Test-Path -LiteralPath $stage) { throw 'Use a fresh Stage 3 staging directory' }
New-Item -ItemType Directory -Path $stage,(Join-Path $stage 'install/bin'),(Join-Path $stage 'source-notices') | Out-Null
foreach ($file in @('pyrowave-vulkan-shared-device.exe','pyrowave-vulkan-shared-device-policy-tests.exe','pyrowave-vulkan-shared-diagnostic-tests.exe','SDL2.dll','SDL3.dll')) {
    Copy-Item -LiteralPath (Join-Path $BuildRoot "Release/$file") -Destination $stage
}
foreach ($file in @('libpyrowave-shared-0.dll','pyrowave-runtime.json')) {
    Copy-Item -LiteralPath (Join-Path $DependencyRoot "install/bin/$file") -Destination (Join-Path $stage 'install/bin')
}
Copy-Item -LiteralPath (Join-Path $DependencyRoot 'install/source-notices') -Destination (Join-Path $stage 'source-notices/runtime') -Recurse
Copy-Item -LiteralPath (Join-Path $source 'LICENSE'),(Join-Path $source 'scripts/baseline-deps.json') -Destination (Join-Path $stage 'source-notices')
foreach ($notice in @(
    @{name='SDL2-compat-LICENSE.txt';url='https://raw.githubusercontent.com/libsdl-org/sdl2-compat/e4df8a55f20da762290a78c2bbe8f8d89d01486a/LICENSE.txt'},
    @{name='SDL3-LICENSE.txt';url='https://raw.githubusercontent.com/libsdl-org/SDL/829a65d769d935c4852f8159e964312c0957260a/LICENSE.txt'})) {
    Invoke-WebRequest -Uri $notice.url -OutFile (Join-Path $stage "source-notices/$($notice.name)")
}
Copy-Item -LiteralPath (Join-Path $source 'tests/pyrowave/vulkan/run-stage3-owner-tests.ps1'),
    (Join-Path $source 'tests/pyrowave/vulkan/STAGE3.md'),
    (Join-Path $source 'scripts/pyrowave/runtime-provenance.ps1'),
    (Join-Path $source 'scripts/pyrowave/pe-machine.ps1') -Destination $stage
# Close the MSVC runtime dependencies using the target architecture's installed CRT.
$build = Get-Content -LiteralPath (Join-Path $DependencyRoot 'evidence/build.json') -Raw | ConvertFrom-Json
$dumpbin = Join-Path (Split-Path $build.compiler -Parent) 'dumpbin.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
foreach ($binary in @(Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') })) {
    Assert-PyroWavePe $binary.FullName $Architecture | Out-Null
    $imports = & $dumpbin /dependents $binary.FullName
    if ($LASTEXITCODE) { throw "Cannot inspect imports: $($binary.Name)" }
    foreach ($line in $imports) {
        $name = $line.Trim()
        if ($name -notmatch '^(msvcp|vcruntime|concrt)[a-z0-9_]+\.dll$') { continue }
        $crt = @(& $vswhere -latest -find "VC/Redist/MSVC/*/$Architecture/Microsoft.VC*.CRT/$name")
        if (!$crt.Count) { throw "Missing target CRT $name" }
        Assert-PyroWavePe $crt[-1] $Architecture | Out-Null
        Copy-Item -LiteralPath $crt[-1] -Destination (Join-Path $binary.DirectoryName $name) -Force
    }
}
$revision = & git -C $source rev-parse HEAD
if ($LASTEXITCODE) { throw 'Cannot record source revision' }
@{architecture=$Architecture;sourceRevision=$revision;runtime=$metadata;scope='Stage3 OFFLINE ONLY';
    runtimePatch='NONE_ADDED_FOR_STAGE3';productionRendererChanged=$false;ownerSurfaceQualification='FAILED_INITIAL_FRAGMENT_COMPARISON_NEW_DIAGNOSIS_PENDING';
    factoryCleanup='CONFIRMED_LEAK_PRODUCTION_PROMOTION_BLOCKED'} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'build.json') -Encoding utf8
if (Get-ChildItem -LiteralPath $stage -Recurse -Filter vulkan-1.dll) { throw 'Vulkan loader must never be packaged' }
Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
    @{file=$_.FullName.Substring($stage.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'sha256.json') -Encoding utf8
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $source "build/stage3-owner-$Architecture.zip")
