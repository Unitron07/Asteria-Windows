[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$DependencyRoot
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$build = Join-Path $sourceRoot "build/qmake-pyrowave-$Architecture"
New-Item -ItemType Directory -Force $build | Out-Null
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$DependencyRoot = (Resolve-Path -LiteralPath $DependencyRoot).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -property installationPath
if (!$vs) { throw 'Visual Studio is required' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$hostArch = if ($env:PROCESSOR_ARCHITECTURE -ieq 'ARM64') { 'arm64' } else { 'amd64' }
$targetArch = if ($Architecture -eq 'arm64') { 'arm64' } else { 'amd64' }
$vcArch = if ($hostArch -eq $targetArch) { $targetArch } else { "${hostArch}_$targetArch" }
$kit = if ($Architecture -eq 'arm64') { 'msvc2022_arm64' } else { 'msvc2022_64' }
$qmake = Join-Path $QtRoot "$kit/bin/qmake.exe"
if ($Architecture -eq 'arm64') {
    $qmake = Join-Path $QtRoot "$kit/bin/qmake.bat"
    if (!(Test-Path $qmake)) { $qmake = Join-Path $QtRoot "$kit/bin/host-qmake.bat" }
}
if (!(Test-Path $qmake)) { throw 'Selected target qmake is missing' }
# Validate before putting explicit paths in a generated, quoted batch program.
foreach ($value in @($sourceRoot,$build,$qmake,$vcvars,$DependencyRoot)) {
    if ($value -match '[\r\n"!%&|<>^]') { throw 'Unsupported shell metacharacter in qmake build path' }
}
@"
@echo off
call "$vcvars" $vcArch
if errorlevel 1 exit /b 1
cd /d "$build"
call "$qmake" "$sourceRoot\tests\pyrowave\offline.pro" "CONFIG+=release" "PYROWAVE_ROOT=$DependencyRoot/install" "VULKAN_HEADERS=$DependencyRoot/source/Granite/third_party/khronos/vulkan-headers/include"
if errorlevel 1 exit /b 1
nmake /nologo
if errorlevel 1 exit /b 1
"@ | Set-Content (Join-Path $build 'compile.cmd') -Encoding ascii
& cmd /d /c (Join-Path $build 'compile.cmd')
if ($LASTEXITCODE) { throw 'Experimental qmake compile/link failed' }
. (Join-Path $PSScriptRoot 'pyrowave/pe-machine.ps1')
Assert-PyroWavePe (Join-Path $build 'release/pyrowave-offline-proof.exe') $Architecture |
    ConvertTo-Json | Set-Content (Join-Path $DependencyRoot 'evidence/qmake-probe-pe.json')
