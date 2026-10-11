[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$DependencyRoot
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$DependencyRoot = (Resolve-Path -LiteralPath $DependencyRoot).Path
$buildRoot = Join-Path $sourceRoot "build/perf-teardown-$Architecture"
$evidence = Join-Path $DependencyRoot "evidence/perf-teardown-$([guid]::NewGuid())"
New-Item -ItemType Directory -Force -Path $buildRoot,$evidence | Out-Null
$qualifiedHead = '0e39de03454e9bc8e2a4d1ff0d09e6a7e667b117'
$oldDecoder = Join-Path $buildRoot 'qualified-decoder.cpp'
git -C $sourceRoot cat-file -e "${qualifiedHead}^{commit}" 2>$null
if ($LASTEXITCODE) {
    # Later PR heads may be shallow clones whose two parents exclude this head.
    git -C $sourceRoot fetch --no-tags origin $qualifiedHead
    if ($LASTEXITCODE) { throw 'Cannot fetch the immutable qualified regression source' }
}
git -C $sourceRoot show "${qualifiedHead}:app/streaming/video/pyrowave_decoder.cpp" | Set-Content -LiteralPath $oldDecoder -Encoding utf8
if ($LASTEXITCODE) { throw 'Qualified regression source is missing' }
. (Join-Path $PSScriptRoot 'baseline-preflight.ps1')
$kit = if ($Architecture -eq 'arm64') { 'msvc2022_arm64' } else { 'msvc2022_64' }
$qtBin = Join-Path $QtRoot "$kit/bin"
$qmake = Resolve-BaselineQmake -QtBin $qtBin
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -property installationPath
if (!$vs) { throw 'Visual Studio is required' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$hostArch = if ($env:PROCESSOR_ARCHITECTURE -ieq 'ARM64') { 'arm64' } else { 'amd64' }
$targetArch = if ($Architecture -eq 'arm64') { 'arm64' } else { 'amd64' }
$vcArch = if ($hostArch -eq $targetArch) { $targetArch } else { "${hostArch}_$targetArch" }
foreach ($value in @($sourceRoot,$buildRoot,$qmake,$vcvars,$DependencyRoot,$QtRoot)) {
    if ($value -match '[\r\n"!%&|<>^]') { throw 'Unsupported shell metacharacter in test build path' }
}
$previousPath=$env:PATH
try {
    $env:PATH="$qtBin;$previousPath"
    foreach ($mode in @('qualified-bug','current')) {
        $build=Join-Path $buildRoot $mode
        $reports=Join-Path $evidence $mode
        New-Item -ItemType Directory -Force -Path $build,$reports | Out-Null
        $decoderSource=if ($mode -eq 'qualified-bug') { $oldDecoder } else { Join-Path $sourceRoot 'app/streaming/video/pyrowave_decoder.cpp' }
        @"
@echo off
call "$vcvars" $vcArch
if errorlevel 1 exit /b 1
cd /d "$build"
call "$qmake" "$sourceRoot\tests\performance\teardown_export.pro" "CONFIG+=release" "DECODER_SOURCE=$decoderSource" "SDL_ROOT=$sourceRoot/libs/windows" "SDL_ARCH=$Architecture" "PYROWAVE_ROOT=$DependencyRoot/install" "VULKAN_HEADERS=$DependencyRoot/source/Granite/third_party/khronos/vulkan-headers/include"
if errorlevel 1 exit /b 1
nmake /nologo
if errorlevel 1 exit /b 1
"@ | Set-Content -LiteralPath (Join-Path $build 'compile.cmd') -Encoding ascii
        & cmd /d /c (Join-Path $build 'compile.cmd')
        if ($LASTEXITCODE) { throw "Production teardown test build failed: $mode" }
        foreach ($dll in @('SDL2.dll','SDL3.dll')) {
            Copy-Item -LiteralPath (Join-Path $sourceRoot "libs/windows/lib/$Architecture/$dll") -Destination (Join-Path $build 'release')
        }
        $exe=Join-Path $build 'release/pyrowave-perf-teardown-tests.exe'
        . (Join-Path $PSScriptRoot 'pyrowave/pe-machine.ps1')
        Assert-PyroWavePe $exe $Architecture | ConvertTo-Json | Set-Content (Join-Path $reports 'test-pe.json')
        & $exe $reports 2>&1 | Tee-Object -FilePath (Join-Path $reports 'test-console.txt')
        $code=$LASTEXITCODE
        if ($mode -eq 'qualified-bug') {
            if ($code -ne 1 -or !(Select-String -LiteralPath (Join-Path $reports 'test-console.txt') -SimpleMatch 'failures=2')) {
                throw 'Regression did not reproduce exactly the two qualified-head metadata errors'
            }
            Write-Output 'PASS negative control: qualified production destructor/export reproduces both metadata errors'
        } else {
            if ($code) { throw 'Current production destructor/export regression failed' }
            $captures=@(Get-ChildItem -LiteralPath $reports -Recurse -Filter 'asteria-perf-*.json' -File | ForEach-Object { $_.FullName })
            if ($captures.Count -ne 3) { throw 'Expected one production JSON report per backend' }
            & python (Join-Path $PSScriptRoot 'performance.py') validate @captures
            if ($LASTEXITCODE) { throw 'Production teardown JSON validation failed' }
            & python (Join-Path $PSScriptRoot 'performance.py') report @captures --output (Join-Path $reports 'analysis.md')
            if ($LASTEXITCODE) { throw 'Production teardown JSON analyzer failed' }
            Write-Output 'PASS current production teardown/export; native, legacy and CPU JSON validated and analyzed'
        }
    }
} finally { $env:PATH=$previousPath }
