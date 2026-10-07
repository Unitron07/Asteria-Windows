[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
      [Parameter(Mandatory)][string]$BuildRoot,[Parameter(Mandatory)][string]$DependencyRoot)
$ErrorActionPreference = 'Stop'
$source = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'pyrowave/runtime-provenance.ps1')
$metadata = Assert-PyroWaveRuntime $DependencyRoot $Architecture
$stage = Join-Path $source "build/stage4-owner-$Architecture"
if (Test-Path -LiteralPath $stage) { throw 'Use a fresh Stage 4 staging directory' }
New-Item -ItemType Directory -Path $stage,(Join-Path $stage 'install/bin'),(Join-Path $stage 'source-notices') | Out-Null
foreach ($file in @('pyrowave-vulkan-stage4.exe','pyrowave-vulkan-stage4-policy-tests.exe','pyrowave-vulkan-stage4-resource-tests.exe','SDL2.dll','SDL3.dll')) {
    Copy-Item -LiteralPath (Join-Path $BuildRoot "Release/$file") -Destination $stage
}
foreach ($file in @('libpyrowave-shared-0.dll','pyrowave-runtime.json','factory-ownership.json')) {
    Copy-Item -LiteralPath (Join-Path $DependencyRoot "install/bin/$file") -Destination (Join-Path $stage 'install/bin')
}
Copy-Item -LiteralPath (Join-Path $DependencyRoot 'install/bin/pyrowave-factory-cleanup-tests.exe') -Destination $stage
Copy-Item -LiteralPath (Join-Path $DependencyRoot 'install/source-notices') -Destination (Join-Path $stage 'source-notices/runtime') -Recurse
Copy-Item -LiteralPath (Join-Path $source 'LICENSE'),(Join-Path $source 'scripts/baseline-deps.json') -Destination (Join-Path $stage 'source-notices')
foreach ($notice in @(
    @{name='SDL2-compat-LICENSE.txt';url='https://raw.githubusercontent.com/libsdl-org/sdl2-compat/e4df8a55f20da762290a78c2bbe8f8d89d01486a/LICENSE.txt'},
    @{name='SDL3-LICENSE.txt';url='https://raw.githubusercontent.com/libsdl-org/SDL/829a65d769d935c4852f8159e964312c0957260a/LICENSE.txt'})) {
    Invoke-WebRequest -Uri $notice.url -OutFile (Join-Path $stage "source-notices/$($notice.name)")
}
Copy-Item -LiteralPath (Join-Path $source 'tests/pyrowave/vulkan/run-stage4-owner-tests.ps1'),
    (Join-Path $source 'tests/pyrowave/vulkan/stage4-evidence-policy.ps1'),
    (Join-Path $source 'tests/pyrowave/vulkan/STAGE4.md'),
    (Join-Path $source 'scripts/pyrowave/runtime-provenance.ps1'),
    (Join-Path $source 'scripts/pyrowave/pe-machine.ps1') -Destination $stage
Copy-Item -LiteralPath (Join-Path $source 'tests/pyrowave/vulkan/shaders') -Destination (Join-Path $stage 'shaders') -Recurse
Copy-Item -LiteralPath (Join-Path $source 'scripts/generate-stage4-shaders.ps1') -Destination (Join-Path $stage 'source-notices')
Copy-Item -LiteralPath (Join-Path $source 'tests/pyrowave/vulkan/stage4_policy.h'),(Join-Path $source 'tests/pyrowave/vulkan/stage4_policy_tests.cpp') -Destination (Join-Path $stage 'source-notices')
# Close the MSVC runtime dependencies using the target architecture's installed CRT.
$build = Get-Content -LiteralPath (Join-Path $DependencyRoot 'evidence/build.json') -Raw | ConvertFrom-Json
$dumpbin = Join-Path (Split-Path $build.compiler -Parent) 'dumpbin.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
foreach ($binary in @(Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') })) {
    $pe=Assert-PyroWavePe $binary.FullName $Architecture
    $imports = & $dumpbin /dependents $binary.FullName
    if ($LASTEXITCODE) { throw "Cannot inspect imports: $($binary.Name)" }
    if ($imports -match '(?i)vulkan-1\.dll|libpyrowave-shared|pyrowave-shared|glslang|shaderc|dxcompiler') { throw "Forbidden startup import: $($binary.Name)" }
    foreach ($line in $imports) {
        $name = $line.Trim()
        if ($name -notmatch '^(msvcp|vcruntime|concrt)[a-z0-9_]+\.dll$') { continue }
        $crt = @(& $vswhere -latest -find "VC/Redist/MSVC/*/$Architecture/Microsoft.VC*.CRT/$name")
        if (!$crt.Count) { throw "Missing target CRT $name" }
        Assert-PyroWavePe $crt[-1] $Architecture | Out-Null
        Copy-Item -LiteralPath $crt[-1] -Destination (Join-Path $binary.DirectoryName $name) -Force
    }
}
# Inventory the final package, including CRT DLLs added during dependency closure.
$inventory=@()
foreach ($binary in Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') }) {
    $pe=Assert-PyroWavePe $binary.FullName $Architecture
    $imports = & $dumpbin /dependents $binary.FullName
    if ($LASTEXITCODE) { throw "Cannot inspect final imports: $($binary.Name)" }
    if ($imports -match '(?i)vulkan-1\.dll|libpyrowave-shared|pyrowave-shared|glslang|shaderc|dxcompiler') { throw "Forbidden startup import: $($binary.Name)" }
    $inventory+=@{file=$binary.FullName.Substring($stage.Length+1);pe=$pe;imports=$imports}
}
$inventory | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $stage 'pe-inventory.json') -Encoding utf8
$revision = & git -C $source rev-parse HEAD
if ($LASTEXITCODE) { throw 'Cannot record source revision' }
@{architecture=$Architecture;sourceRevision=$revision;runtime=$metadata;scope='Stage4 OFFLINE ONLY';
    runtimePatch='0004-borrowed-factory-cleanup.patch';productionRendererChanged=$false;ownerSurfaceQualification='OWNER_QUALIFICATION_PENDING';
    factoryCleanup='PATCHED_SOURCE_TEST_PASSED_PUBLIC_FAULT_PENDING'} |
    ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $stage 'build.json') -Encoding utf8
if (Get-ChildItem -LiteralPath $stage -Recurse -Filter vulkan-1.dll) { throw 'Vulkan loader must never be packaged' }
Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
    @{file=$_.FullName.Substring($stage.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'sha256.json') -Encoding utf8
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $source "build/stage4-owner-$Architecture.zip")
