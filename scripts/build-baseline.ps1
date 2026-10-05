[CmdletBinding()]
param(
    [string]$SourceRoot = (Split-Path $PSScriptRoot -Parent),
    [ValidateSet('x64', 'arm64')][string]$Architecture = 'x64',
    [string]$QtBin
)
$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
# Upstream build-arch.bat uses unquoted source paths.
if ($SourceRoot -match '[\s!&()%\^]') { throw 'Use a checkout path without spaces or shell metacharacters, for example C:\src\Asteria.' }
$Architecture = $Architecture.ToLowerInvariant()
. (Join-Path $PSScriptRoot 'baseline-preflight.ps1')
Assert-BaselineDependencies -SourceRoot $SourceRoot -Architecture $Architecture
$QtBin = Resolve-BaselineQt -Architecture $Architecture -QtBin $QtBin
$savedPath = $env:PATH
# Upstream selects the last `where qmake` match and prefers any qmake.bat.
# Remove other Qt tool directories before adding the selected target kit.
$env:PATH = $QtBin + ';' + (($env:PATH -split ';' | Where-Object {
    $entry = $_.Trim('"')
    $entry -and !(Test-Path (Join-Path $entry 'qmake.exe')) -and
        !(Test-Path (Join-Path $entry 'qmake.bat')) -and
        !(Test-Path (Join-Path $entry 'host-qmake.bat'))
}) -join ';')
$evidenceRelative = "build/evidence/$Architecture"
$evidence = Join-Path $SourceRoot $evidenceRelative
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
Start-Transcript -Path (Join-Path $evidence 'build.log') -Force | Out-Null
Push-Location $SourceRoot
try {
    $sha = git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve source revision' }
    $submodules = @(git submodule status --recursive)
    if ($LASTEXITCODE -ne 0 -or ($submodules | Where-Object { $_ -match '^[-+U]' })) {
        throw 'Initialize recursive submodules at their pinned revisions first.'
    }
    $submodules | Set-Content (Join-Path $evidence 'submodules.txt') -Encoding utf8
    [pscustomobject]@{
        source = $sha
        architecture = $Architecture
        configuration = "Release $Architecture; unsigned"
        harness = (git -C $PSScriptRoot rev-parse HEAD)
        workflowRun = $env:GITHUB_RUN_ID
        workflowAttempt = $env:GITHUB_RUN_ATTEMPT
        qtBin = $QtBin
        command = 'scripts\build-arch.bat Release'
        os = [System.Environment]::OSVersion.VersionString
        runnerImage = $env:ImageVersion
        runnerImageOS = $env:ImageOS
    } | ConvertTo-Json | Set-Content (Join-Path $evidence 'build.json') -Encoding utf8
    foreach ($tool in @('qmake', '7z')) { Get-Command $tool -ErrorAction Stop | Out-Host }
    $qmakeCommand = Resolve-BaselineQmake -QtBin $QtBin
    & $qmakeCommand -query
    if ($LASTEXITCODE -ne 0) { throw 'qmake failed' }
    $qtVersion = & $qmakeCommand -query QT_VERSION
    if ($LASTEXITCODE -ne 0 -or $qtVersion.Trim() -ne '6.11.2') { throw 'Qt 6.11.2 is required' }
    if ($Architecture -eq 'arm64') {
        $hostBin = Join-Path (Split-Path (Split-Path $QtBin -Parent) -Parent) 'msvc2022_64/bin'
        $hostVersion = & (Join-Path $hostBin 'qmake.exe') -query QT_VERSION
        if ($LASTEXITCODE -ne 0 -or $hostVersion.Trim() -ne $qtVersion.Trim()) {
            throw 'ARM64 target and x64 host Qt versions must match'
        }
        "target=$QtBin; version=$qtVersion", "host=$hostBin; version=$hostVersion" |
            Set-Content (Join-Path $evidence 'qt-kits.txt') -Encoding utf8
    }
    # Match upstream's vswhere selection and record the actual compiler and SDK.
    $vswhere = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    $vswherePath = if ($vswhere) { $vswhere.Source } else { Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe' }
    if (!(Test-Path -LiteralPath $vswherePath)) { throw 'Visual Studio Installer/vswhere is required' }
    $vsPath = & $vswherePath -latest -property installationPath
    if (!$vsPath) { throw 'No Visual Studio installation found' }
    $vcvars = Join-Path $vsPath 'VC/Auxiliary/Build/vcvarsall.bat'
    $vcTarget = if ($Architecture -eq 'x64') { 'AMD64' } else { 'arm64' }
    $vcArch = if ($env:PROCESSOR_ARCHITECTURE -ieq $vcTarget) { $vcTarget } else { "$($env:PROCESSOR_ARCHITECTURE)_$vcTarget" }
    $evidenceCmd = $evidenceRelative.Replace('/', '\')
    & $vswherePath -latest -format json | Set-Content (Join-Path $evidence 'visual-studio.json') -Encoding utf8
    # A batch file avoids PowerShell/cmd double-quoting of Program Files paths.
    @"
@echo off
call "$vcvars" $vcArch
if errorlevel 1 exit /b 1
where cl
if errorlevel 1 exit /b 1
where dumpbin > $evidenceCmd\dumpbin-path.txt
if errorlevel 1 exit /b 1
cl /Bv > $evidenceCmd\compiler.txt 2>&1
rem cl /Bv without a source file reports a usage error after its version banner.
if not defined WindowsSDKVersion exit /b 1
if not defined VCToolsVersion exit /b 1
set WindowsSDK > $evidenceCmd\sdk.txt
set VCToolsVersion >> $evidenceCmd\sdk.txt
exit /b 0
"@ | Set-Content (Join-Path $evidence 'capture-toolchain.cmd') -Encoding ascii
    cmd /d /c "$evidenceCmd\capture-toolchain.cmd"
    if ($LASTEXITCODE -ne 0) { throw 'Unable to capture compiler and SDK versions' }
    Get-Content (Join-Path $evidence 'compiler.txt'), (Join-Path $evidence 'sdk.txt') | Out-Host
    # With CI_VERSION unset upstream creates portable.dat, keeping this test
    # build's settings separate from an installed Asteria user profile.
    $savedVersion = $env:CI_VERSION
    $env:CI_VERSION = $null
    try {
        cmd /d /c scripts\build-arch.bat Release
        if ($LASTEXITCODE -ne 0) { throw "Upstream build failed ($LASTEXITCODE)" }
    } finally { $env:CI_VERSION = $savedVersion }
    $deploy = Join-Path $SourceRoot "build/deploy-$Architecture-release"
    $notices = Join-Path $deploy 'source-notices'
    foreach ($license in @('LICENSE', 'h264bitstream/LICENSE',
        'qmdnsengine/qmdnsengine/LICENSE.txt', 'app/SDL_GameControllerDB/LICENSE',
        'moonlight-common-c/moonlight-common-c/LICENSE.txt',
        'moonlight-common-c/moonlight-common-c/enet/LICENSE',
        'moonlight-common-c/moonlight-common-c/nanors/LICENSE')) {
        $destination = Join-Path $notices $license
        New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
        Copy-Item -LiteralPath (Join-Path $SourceRoot $license) -Destination $destination
    }
    @"
Development baseline from https://github.com/moonlight-stream/moonlight-qt at $sha.
Source snapshot including pinned submodules is in the accompanying evidence artifact.
Dependency source/build recipes: https://github.com/moonlight-stream/moonlight-qt-deps/tree/2ab26b8cd5c42899ffd97c573ff2c678738f41b1
Qt source archives: https://download.qt.io/archive/qt/6.11/6.11.2/submodules/
This unsigned development baseline is not a qualified Asteria release.
"@ | Set-Content (Join-Path $notices 'PROVENANCE.txt') -Encoding utf8
    Copy-Item (Join-Path $PSScriptRoot '../docs/DEPENDENCIES_WINDOWS.md') $notices
    $package = @(Get-ChildItem "build/installer-$Architecture-release/*.zip")
    if ($package.Count -ne 1) { throw 'Expected exactly one portable ZIP' }
    7z a $package[0].FullName "$deploy\source-notices"
    if ($LASTEXITCODE -ne 0) { throw 'Unable to include source notices' }
    $clientExecutable = if ($sha -eq 'e3fd29e4d7dc5723d8d0da7d19e2698daec74456') { 'Moonlight.exe' } else { 'Asteria.exe' }
    if ($Architecture -eq 'arm64') {
        $dumpbinPath = Get-Content (Join-Path $evidence 'dumpbin-path.txt') | Select-Object -First 1
        & (Join-Path $PSScriptRoot 'repair-arm64-package.ps1') `
            -PackagePath $package[0].FullName -DumpbinPath $dumpbinPath `
            -ReportPath (Join-Path $evidence 'arm64-runtime-cleanup.json') `
            -ClientExecutable $clientExecutable
    }
    # Inspect the final ZIP, including nested Qt plugins, before any artifact upload.
    & (Join-Path $PSScriptRoot 'test-package-architecture.ps1') `
        -PackagePath $package[0].FullName -Architecture $Architecture `
        -ReportPath (Join-Path $evidence 'package-architecture.json') `
        -ClientExecutable $clientExecutable
    if ($clientExecutable -eq 'Asteria.exe') {
        & (Join-Path $PSScriptRoot 'test-pyrowave-package.ps1') `
            -PackagePath $package[0].FullName -Architecture $Architecture `
            -ReportPath (Join-Path $evidence 'pyrowave-package.json')
    }
    tar -czf (Join-Path $evidence 'source.tar.gz') --exclude=.git --exclude=./build --exclude=./libs -C $SourceRoot .
    if ($LASTEXITCODE -ne 0) { throw 'Unable to archive source and submodules' }
    Get-ChildItem "build/installer-$Architecture-release/*.zip", "build/symbols-$Architecture-release/*.zip", (Join-Path $evidence 'source.tar.gz') |
        Get-FileHash -Algorithm SHA256 | Format-Table -AutoSize | Out-String -Width 300 |
        Set-Content (Join-Path $evidence 'artifact-sha256.txt') -Encoding utf8
} finally {
    Pop-Location
    Stop-Transcript | Out-Null
    $env:PATH = $savedPath
}
