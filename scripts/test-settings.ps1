[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$SourceRoot = (Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$build = Join-Path $SourceRoot "build/settings-tests-$Architecture"
New-Item -ItemType Directory -Force -Path $build | Out-Null
$kit = if ($Architecture -eq 'arm64') { 'msvc2022_arm64' } else { 'msvc2022_64' }
$qtBin = Join-Path $QtRoot "$kit/bin"
. (Join-Path $PSScriptRoot 'baseline-preflight.ps1')
$qmake = Resolve-BaselineQmake -QtBin $qtBin
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -property installationPath
if (!$vs) { throw 'Visual Studio is required' }
$vcvars = Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
$hostArch = if ($env:PROCESSOR_ARCHITECTURE -ieq 'ARM64') { 'arm64' } else { 'amd64' }
$targetArch = if ($Architecture -eq 'arm64') { 'arm64' } else { 'amd64' }
$vcArch = if ($hostArch -eq $targetArch) { $targetArch } else { "${hostArch}_$targetArch" }
$deployTool = Join-Path $qtBin 'windeployqt.exe'
$deployArgs = ''
if ($Architecture -eq 'arm64') {
    $hostTool = Join-Path $QtRoot 'msvc2022_64/bin/windeployqt.exe'
    $qtPaths = Join-Path $qtBin 'host-qtpaths.bat'
    if (Test-Path -LiteralPath $qtPaths) { $deployTool=$hostTool; $deployArgs="--qtpaths `"$qtPaths`"" }
    elseif (!(Test-Path -LiteralPath $deployTool)) { $deployTool=$hostTool; $deployArgs="--qtpaths `"$qtBin/qtpaths.bat`"" }
}
foreach ($value in @($SourceRoot,$build,$qmake,$vcvars,$QtRoot)) {
    if ($value -match '[\r\n"!%&|<>^]') { throw 'Unsupported shell metacharacter in test build path' }
}
@"
@echo off
call "$vcvars" $vcArch
if errorlevel 1 exit /b 1
cd /d "$build"
call "$qmake" "$SourceRoot\tests\settings\settings.pro" "CONFIG+=release"
if errorlevel 1 exit /b 1
nmake /nologo
if errorlevel 1 exit /b 1
"$deployTool" $deployArgs --dir "$build\release" --release --qmldir "$SourceRoot\app\gui" --no-opengl-sw "$build\release\asteria-settings-tests.exe"
if errorlevel 1 exit /b 1
set QT_QPA_PLATFORM=offscreen
set QT_QUICK_BACKEND=software
set QT_QUICK_CONTROLS_STYLE=Basic
release\asteria-settings-tests.exe -o settings-results.txt,txt -o settings-results.xml,junitxml
if errorlevel 1 exit /b 1
"@ | Set-Content (Join-Path $build 'test.cmd') -Encoding ascii
& cmd /d /c (Join-Path $build 'test.cmd')
$testExit = $LASTEXITCODE
if (Test-Path (Join-Path $build 'settings-results.txt')) { Get-Content (Join-Path $build 'settings-results.txt') | Out-Host }
if ($testExit) { throw 'Settings/preferences/UI regression failed' }
