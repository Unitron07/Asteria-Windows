[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$DependencyRoot,
    [Parameter(Mandatory)][string]$QtBin
)
$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path (Split-Path $PSScriptRoot -Parent)).Path
$DependencyRoot = (Resolve-Path -LiteralPath $DependencyRoot).Path
foreach ($value in @($sourceRoot,$DependencyRoot,$QtBin)) {
    if ($value -match '[\r\n"!%&|<>^]') { throw 'Unsupported shell metacharacter in experimental build path' }
}
. (Join-Path $PSScriptRoot 'pyrowave/pe-machine.ps1')
$runtime = Join-Path $DependencyRoot 'install/bin/libpyrowave-shared-0.dll'
Assert-PyroWavePe $runtime $Architecture | Out-Null
$metadata = Get-Content (Join-Path $DependencyRoot 'install/bin/pyrowave-runtime.json') -Raw | ConvertFrom-Json
if ($metadata.architecture -cne $Architecture -or $metadata.codecCommit -cne '186f0393b77f7755953b5ecde994bb1cec2e4155' -or
    $metadata.bitstreamId -cne '186f0393' -or $metadata.apiVersion -cne '0.6.0' -or
    $metadata.sha256 -cne (Get-FileHash -LiteralPath $runtime -Algorithm SHA256).Hash.ToLowerInvariant()) {
    throw 'Experimental runtime provenance mismatch'
}
./scripts/apply-common-c-p1a.ps1 -SourceRoot $sourceRoot
$savedArgs = $env:ASTERIA_EXPERIMENTAL_QMAKE_ARGS
try {
    # This variable is used only by this explicit experimental packaging script.
    $env:ASTERIA_EXPERIMENTAL_QMAKE_ARGS = '"CONFIG+=pyrowave_experimental" "PYROWAVE_ROOT=' +
        $DependencyRoot.Replace('\','/') + '/install" "VULKAN_HEADERS=' +
        $DependencyRoot.Replace('\','/') + '/source/Granite/third_party/khronos/vulkan-headers/include"'
    ./scripts/build-baseline.ps1 -SourceRoot $sourceRoot -Architecture $Architecture -QtBin $QtBin
} finally { $env:ASTERIA_EXPERIMENTAL_QMAKE_ARGS = $savedArgs }
$deploy = Join-Path $sourceRoot "build/deploy-$Architecture-release"
$evidence = Join-Path $sourceRoot "build/evidence/$Architecture"
$stage = Join-Path $sourceRoot "build/live-p1a-$Architecture"
if (Test-Path -LiteralPath $stage) { throw 'Use a fresh live staging directory to avoid stale binaries' }
New-Item -ItemType Directory -Path $stage | Out-Null
# Use the final baseline ZIP, which has already received the ARM64 CRT repair.
# The uncorrected deployment directory can still contain Qt's surplus x64 CRT.
$baselinePackage = @(Get-ChildItem (Join-Path $sourceRoot "build/installer-$Architecture-release") -Filter '*.zip')
if ($baselinePackage.Count -ne 1) { throw 'Expected one verified baseline ZIP' }
[IO.Compression.ZipFile]::ExtractToDirectory($baselinePackage[0].FullName,$stage)
New-Item -ItemType Directory -Path (Join-Path $stage 'pyrowave') | Out-Null
Copy-Item -Path (Join-Path $DependencyRoot 'install/bin/*') -Destination (Join-Path $stage 'pyrowave')
Copy-Item -LiteralPath (Join-Path $DependencyRoot 'install/source-notices') -Destination (Join-Path $stage 'pyrowave/source-notices') -Recurse
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/pyrowave/common-c-p1a.patch') -Destination (Join-Path $stage 'source-notices')
Copy-Item -Path (Join-Path $sourceRoot "build/presentation-$Architecture/source-notices/SDL*-LICENSE.txt") -Destination (Join-Path $stage 'source-notices')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'tests/pyrowave/LIVE-OWNER-TEST.md') -Destination (Join-Path $stage 'RUN-ME.md')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/collect-pyrowave-logs.ps1') -Destination $stage
$dumpbin = Get-Content (Join-Path $evidence 'dumpbin-path.txt') | Select-Object -First 1
$client = Join-Path $stage 'Asteria.exe'
$imports = & $dumpbin /imports $client
if ($LASTEXITCODE) { throw 'Cannot inspect live client imports' }
$imports | Set-Content (Join-Path $evidence 'live-client-imports.txt')
if ($imports -match 'libpyrowave-shared|vulkan-1\.dll') { throw 'Client has forbidden startup codec/Vulkan import' }
& $dumpbin /dependents $runtime | Set-Content (Join-Path $evidence 'live-runtime-dependents.txt')
if ($LASTEXITCODE) { throw 'Cannot inspect runtime dependency closure' }
# Existing dependency helper archives imports; verify every non-system runtime
# dependency resolves to the target package or Windows system32.
$systemDlls = @('KERNEL32.dll','USER32.dll','ADVAPI32.dll','SHELL32.dll','OLE32.dll','WS2_32.dll','BCRYPT.dll','NTDLL.dll','GDI32.dll','WINMM.dll')
$dependencies = & $dumpbin /dependents $runtime
foreach ($line in $dependencies) {
    $name = $line.Trim()
    if ($name -notmatch '^[A-Za-z0-9_.-]+\.dll$') { continue }
    if ($systemDlls -icontains $name -or $name -imatch '^(api-ms-|ext-ms-)') { continue }
    $candidate = Join-Path $stage $name
    if (!(Test-Path -LiteralPath $candidate)) {
        # A runtime built with the current compiler can need a CRT component
        # unused by the ordinary client. Obtain only the selected native target.
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $crt = @(& $vswhere -latest -find "VC/Redist/MSVC/*/$Architecture/Microsoft.VC*.CRT/$name")
        if (!$crt.Count) { throw "Missing packaged runtime dependency: $name" }
        Assert-PyroWavePe $crt[-1] $Architecture | Out-Null
        Copy-Item -LiteralPath $crt[-1] -Destination $candidate
    }
    Assert-PyroWavePe $candidate $Architecture | Out-Null
    # Restricted DLL loader searches only this folder and System32.
    Copy-Item -LiteralPath $candidate -Destination (Join-Path $stage "pyrowave/$name")
}
$zip = Join-Path $sourceRoot "build/Asteria-P1a-Experimental-$Architecture.zip"
Compress-Archive -Path "$stage/*" -DestinationPath $zip -Force
./scripts/test-package-architecture.ps1 -PackagePath $zip -Architecture $Architecture `
    -ReportPath (Join-Path $evidence 'live-package-architecture.json')
Get-ChildItem -LiteralPath $stage -File -Recurse | Get-FileHash -Algorithm SHA256 |
    Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $evidence 'live-package-sha256.json')
@{implementation='ready for owner interoperability test'; liveQualification='pending'; architecture=$Architecture;
  commonC='f900dd4767759c7b9d0e93bcea666b55c69ea62f + maintained P1a patch';
  patchSha256=(Get-FileHash (Join-Path $sourceRoot 'scripts/pyrowave/common-c-p1a.patch')).Hash;
  codec=$metadata; packageSha256=(Get-FileHash $zip).Hash} |
    ConvertTo-Json -Depth 6 | Set-Content (Join-Path $evidence 'live-build.json')
