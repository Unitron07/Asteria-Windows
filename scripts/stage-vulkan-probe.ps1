[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
      [Parameter(Mandatory)][string]$BuildRoot, [Parameter(Mandatory)][string]$VulkanHeadersRoot)
$ErrorActionPreference = 'Stop'
$source = Split-Path $PSScriptRoot -Parent
$stage = Join-Path $source "build/vulkan-owner-$Architecture"
New-Item -ItemType Directory -Force -Path $stage | Out-Null
foreach ($file in @('pyrowave-vulkan-probe.exe','pyrowave-vulkan-policy-tests.exe','SDL2.dll','SDL3.dll')) {
    Copy-Item -LiteralPath (Join-Path $BuildRoot "Release/$file") -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $source 'tests/pyrowave/vulkan/run-owner-tests.ps1') -Destination $stage
Copy-Item -LiteralPath (Join-Path $source 'tests/pyrowave/vulkan/README.md') -Destination (Join-Path $stage 'README.md')
Copy-Item -LiteralPath (Join-Path $source 'tests/pyrowave/vulkan/QUALIFICATION.md') -Destination $stage
$notices = Join-Path $stage 'source-notices'
New-Item -ItemType Directory -Force -Path $notices | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'LICENSE'),(Join-Path $source 'scripts/baseline-deps.json') -Destination $notices
foreach ($entry in @(
    @{file='SDL2-compat-LICENSE.txt';url='https://raw.githubusercontent.com/libsdl-org/sdl2-compat/e4df8a55f20da762290a78c2bbe8f8d89d01486a/LICENSE.txt'},
    @{file='SDL3-LICENSE.txt';url='https://raw.githubusercontent.com/libsdl-org/SDL/829a65d769d935c4852f8159e964312c0957260a/LICENSE.txt'})) {
    Invoke-WebRequest -Uri $entry.url -OutFile (Join-Path $notices $entry.file)
}
Copy-Item -LiteralPath (Join-Path $VulkanHeadersRoot 'LICENSE.md') -Destination (Join-Path $notices 'Vulkan-Headers-LICENSE.md')
$revision = & git -C $source rev-parse HEAD
if ($LASTEXITCODE) { throw 'Cannot record source revision' }
@{architecture=$Architecture;sourceRevision=$revision;scope='Stage2 synthetic only';productionRendererChanged=$false;
    codecRuntimeIncluded=$false;vulkanHeaderPin='6802bb4733b63ed5efd3adb308a6c885ef180ea1';ownerQualification='OWNER_CONFIRMED';
    ownerQualifiedProbeRevision='0008248ef75d2404225d9d2eed35c5a030c27fe9';validationStatus='SKIP';stage3Authorized=$false} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'build.json')
Get-ChildItem -LiteralPath $stage -File -Recurse | ForEach-Object {
    @{file=$_.FullName.Substring($stage.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'sha256.json')
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $source "build/vulkan-owner-$Architecture.zip") -Force
