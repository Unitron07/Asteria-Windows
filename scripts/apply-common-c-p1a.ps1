[CmdletBinding()]
param([string]$SourceRoot = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$checkout = Join-Path $SourceRoot 'moonlight-common-c/moonlight-common-c'
$patch = Join-Path $SourceRoot 'scripts/pyrowave/common-c-p1a.patch'
$revision = git -C $checkout rev-parse HEAD
if ($LASTEXITCODE -or $revision -cne 'aeba988e0a302145eff2e70fa3e11cd98ec01f5d') {
    throw 'Unexpected common-c gitlink; initialize the pinned submodule before applying P1a'
}
git -C $checkout apply --reverse --check $patch 2>$null
if ($LASTEXITCODE -eq 0) { return }
git -C $checkout apply --check $patch
if ($LASTEXITCODE) { throw 'P1a common-c patch cannot apply cleanly; inspect local changes' }
git -C $checkout apply $patch
if ($LASTEXITCODE) { throw 'P1a common-c patch application failed' }
