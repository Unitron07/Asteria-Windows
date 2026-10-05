[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64','arm64')][string]$Architecture,
    [Parameter(Mandatory)][string]$DependencyRoot
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$stage = Join-Path $sourceRoot "build/presentation-$Architecture"
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceRoot "build/probe-$Architecture/Release/pyrowave-offline-proof.exe") -Destination $stage
foreach ($dll in @('SDL2.dll','SDL3.dll')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot "libs/windows/lib/$Architecture/$dll") -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $DependencyRoot 'install') -Destination (Join-Path $stage 'runtime') -Recurse
$notices = Join-Path $stage 'source-notices'
New-Item -ItemType Directory -Force $notices | Out-Null
foreach ($input in @(
    @{name='SDL2-compat-LICENSE.txt'; url='https://raw.githubusercontent.com/libsdl-org/sdl2-compat/e4df8a55f20da762290a78c2bbe8f8d89d01486a/LICENSE.txt'},
    @{name='SDL3-LICENSE.txt'; url='https://raw.githubusercontent.com/libsdl-org/SDL/829a65d769d935c4852f8159e964312c0957260a/LICENSE.txt'}
)) {
    Invoke-WebRequest -Uri $input.url -OutFile (Join-Path $notices $input.name)
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'LICENSE') -Destination $notices
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/baseline-deps.json') -Destination $notices
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/DEPENDENCIES_WINDOWS.md') -Destination $notices
Copy-Item -LiteralPath (Join-Path $sourceRoot 'tests/pyrowave/README.md') -Destination (Join-Path $stage 'README.md')
@"
Offline P0.5 $Architecture probe. Native target SDL2 API via pinned sdl2-compat/SDL3 v19 archive.
Run from this directory with the matching Windows architecture and MSVC runtime installed:
.\pyrowave-offline-proof.exe --present-raw-i420 - .\evidence-raw
.\pyrowave-offline-proof.exe --present-compatibility .\runtime\bin .\evidence-compat
.\pyrowave-offline-proof.exe --present-records .\runtime\bin .\evidence-records
.\pyrowave-offline-proof.exe --present-recreate-test .\runtime\bin .\evidence-recreate
.\pyrowave-offline-proof.exe --present-loop .\runtime\bin .\evidence-loop --seconds 30
Controls: 1..5 patterns; T raw/compat/records; F fullscreen; R recreate; S nearest/linear;
N native window; W smaller/larger/square window; Esc exit. Maximize/restore manually.
Hosted CI and exit 0 do not qualify visual color, chroma siting or true GPU device loss.
See README.md and source-notices/ for scope, expected images and dependency provenance.
"@ | Set-Content (Join-Path $stage 'RUN-ME.txt') -Encoding utf8
Get-ChildItem $stage -File -Recurse | Get-FileHash -Algorithm SHA256 |
    Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $DependencyRoot 'evidence/presentation-package-sha256.json')
