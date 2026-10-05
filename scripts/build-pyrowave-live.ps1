# Compatibility entry point. PyroWave now ships in the normal Windows package;
# there is no separate experimental client or P1a end-user ZIP.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$DependencyRoot,
    [Parameter(Mandatory)][string]$QtBin
)
$savedRoot = $env:ASTERIA_PYROWAVE_DEPENDENCY_ROOT
try {
    $env:ASTERIA_PYROWAVE_DEPENDENCY_ROOT = (Resolve-Path -LiteralPath $DependencyRoot).Path
    & (Join-Path $PSScriptRoot 'build-baseline.ps1') -Architecture $Architecture -QtBin $QtBin
} finally { $env:ASTERIA_PYROWAVE_DEPENDENCY_ROOT = $savedRoot }
