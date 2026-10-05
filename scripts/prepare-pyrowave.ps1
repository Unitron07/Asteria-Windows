[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$EnvironmentFile,
    [string]$DependencyRoot = $env:ASTERIA_PYROWAVE_DEPENDENCY_ROOT
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
if (!$DependencyRoot) {
    $DependencyRoot = Join-Path $sourceRoot "build/pyrowave/$Architecture"
    # Reuse the dedicated validation job's documented ARM64 patched build too.
    $patched = Join-Path $sourceRoot "build/pyrowave-patched/$Architecture"
    if (Test-Path (Join-Path $patched 'install/bin/libpyrowave-shared-0.dll')) { $DependencyRoot = $patched }
    if (!(Test-Path (Join-Path $DependencyRoot 'install/bin/libpyrowave-shared-0.dll'))) {
        # The helper retains exact pins, patch hashes and architecture evidence.
        & (Join-Path $PSScriptRoot 'build-pyrowave-deps.ps1') -Architecture $Architecture
    }
}
$DependencyRoot = (Resolve-Path -LiteralPath $DependencyRoot).Path
. (Join-Path $PSScriptRoot 'pyrowave/runtime-provenance.ps1')
Assert-PyroWaveRuntime $DependencyRoot $Architecture | Out-Null
$headers = Join-Path $DependencyRoot 'source/Granite/third_party/khronos/vulkan-headers/include'
foreach ($file in @((Join-Path $DependencyRoot 'install/include/pyrowave/pyrowave.h'), (Join-Path $headers 'vulkan/vulkan_core.h'))) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing pinned build header: $file" }
}
# A quoted batch environment file passes verified paths back to build-arch.bat.
foreach ($value in @($DependencyRoot,$headers)) {
    if ($value -match '[\r\n"!%&|<>^]') { throw 'Unsupported shell metacharacter in runtime path' }
}
@"
@set "ASTERIA_PYROWAVE_DEPS=$DependencyRoot"
@set "PYROWAVE_ROOT=$($DependencyRoot.Replace('\','/'))/install"
@set "VULKAN_HEADERS=$($headers.Replace('\','/'))"
"@ | Set-Content -LiteralPath $EnvironmentFile -Encoding ascii
