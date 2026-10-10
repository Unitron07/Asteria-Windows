[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$DependencyRoot,
    [Parameter(Mandatory)][string]$DeployDirectory,
    [Parameter(Mandatory)][string]$ClientPath
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'pyrowave/runtime-provenance.ps1')
$metadata = Assert-PyroWaveRuntime $DependencyRoot $Architecture
$runtime = Join-Path $DependencyRoot 'install/bin/libpyrowave-shared-0.dll'
$stage = Join-Path $DeployDirectory 'pyrowave'
$evidence = Join-Path $sourceRoot "build/evidence/$Architecture/pyrowave"
New-Item -ItemType Directory -Force -Path $stage,$evidence | Out-Null
Copy-Item -Path (Join-Path $DependencyRoot 'install/bin/*') -Destination $stage
Copy-Item -LiteralPath (Join-Path $DependencyRoot 'install/source-notices') -Destination (Join-Path $stage 'source-notices') -Recurse
Copy-Item -Path (Join-Path $DependencyRoot 'evidence/*') -Destination $evidence -Recurse
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/pyrowave/common-c-p1a.patch') -Destination (Join-Path $stage 'source-notices')
foreach ($notice in @(
    @{name='SDL2-compat-LICENSE.txt'; url='https://raw.githubusercontent.com/libsdl-org/sdl2-compat/e4df8a55f20da762290a78c2bbe8f8d89d01486a/LICENSE.txt'},
    @{name='SDL3-LICENSE.txt'; url='https://raw.githubusercontent.com/libsdl-org/SDL/829a65d769d935c4852f8159e964312c0957260a/LICENSE.txt'}
)) {
    Invoke-WebRequest -Uri $notice.url -OutFile (Join-Path $stage "source-notices/$($notice.name)")
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'tests/pyrowave/LIVE-OWNER-TEST.md') -Destination (Join-Path $DeployDirectory 'PYROWAVE-OWNER-TEST.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/collect-pyrowave-logs.ps1') -Destination $DeployDirectory
$shaderStage=Join-Path $stage 'shaders'
New-Item -ItemType Directory -Force $shaderStage | Out-Null
Copy-Item -Path (Join-Path $sourceRoot 'app/streaming/video/shaders/pyrowave/*') -Destination $shaderStage
Copy-Item -LiteralPath (Join-Path $sourceRoot 'tests/pyrowave/vulkan/STAGE5-OWNER-TEST.md') -Destination (Join-Path $DeployDirectory 'STAGE5-OWNER-TEST.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/verify-stage5-owner-package.ps1') -Destination $DeployDirectory
$dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
$imports = & $dumpbin /imports $ClientPath
if ($LASTEXITCODE) { throw 'Cannot inspect client imports' }
$imports | Set-Content (Join-Path $evidence 'client-imports.txt')
if ($imports -match 'libpyrowave-shared|vulkan-1\.dll') { throw 'Client has forbidden startup codec/Vulkan import' }
$dependencies = & $dumpbin /dependents $runtime
if ($LASTEXITCODE) { throw 'Cannot inspect runtime dependency closure' }
$dependencies | Set-Content (Join-Path $evidence 'runtime-dependents.txt')
$systemDlls = @('KERNEL32.dll','USER32.dll','ADVAPI32.dll','SHELL32.dll','OLE32.dll','WS2_32.dll','BCRYPT.dll','NTDLL.dll','GDI32.dll','WINMM.dll')
foreach ($line in $dependencies) {
    $name = $line.Trim()
    if ($name -notmatch '^[A-Za-z0-9_.-]+\.dll$') { continue }
    if ($systemDlls -icontains $name -or $name -imatch '^(api-ms-|ext-ms-)') { continue }
    $candidate = Join-Path $DeployDirectory $name
    if (!(Test-Path -LiteralPath $candidate)) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $crt = @(& $vswhere -latest -find "VC/Redist/MSVC/*/$Architecture/Microsoft.VC*.CRT/$name")
        if (!$crt.Count) { throw "Missing packaged runtime dependency: $name" }
        $candidate = $crt[-1]
    }
    Assert-PyroWavePe $candidate $Architecture | Out-Null
    # The restricted loader searches this directory and System32, never PATH.
    Copy-Item -LiteralPath $candidate -Destination (Join-Path $stage $name)
}
Get-ChildItem -LiteralPath $stage -File -Recurse | Get-FileHash -Algorithm SHA256 |
    Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $evidence 'packaged-runtime-sha256.json')
$metadata | ConvertTo-Json | Set-Content (Join-Path $evidence 'packaged-runtime.json')
Write-Output "Staged pinned PyroWave $Architecture runtime in the normal deployment"
