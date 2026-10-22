# No Qt, network, MSVC or GPU required. Work only in a disposable local clone.
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$checkout = Join-Path $sourceRoot 'moonlight-common-c/moonlight-common-c'
$scratch = Join-Path ([IO.Path]::GetTempPath()) ('asteria-p1a-' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $scratch 'moonlight-common-c/moonlight-common-c'
$patch = Join-Path $scratch 'scripts/pyrowave/common-c-p1a.patch'
New-Item -ItemType Directory -Force (Split-Path $fixture), (Split-Path $patch) | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceRoot 'scripts/pyrowave/common-c-p1a.patch') -Destination $patch
git clone --shared --no-checkout --quiet $checkout $fixture
if ($LASTEXITCODE) { throw 'Cannot create local common-c fixture' }
function Assert-Rejected([scriptblock]$Action, [string]$Message) {
    try { & $Action } catch {
        if ($_.Exception.Message -notlike "*$Message*") { throw }
        return
    }
    throw "Expected rejection: $Message"
}

# A synthetic revision with the exact same tree proves the SHA check is active.
# This also works with Actions' shallow submodule checkout, without fetching history.
$tree = git -C $fixture rev-parse 'aeba988e0a302145eff2e70fa3e11cd98ec01f5d^{tree}'
if ($LASTEXITCODE) { throw 'Reviewed tree unavailable' }
$unexpected = git -C $fixture -c user.name=Fixture -c user.email=fixture@example.invalid commit-tree $tree -m 'Unexpected revision fixture'
if ($LASTEXITCODE) { throw 'Cannot create unexpected revision fixture' }
git -C $fixture checkout --quiet $unexpected
if ($LASTEXITCODE) { throw 'Unexpected revision fixture unavailable' }
Assert-Rejected { & "$PSScriptRoot/apply-common-c-p1a.ps1" -SourceRoot $scratch } 'Unexpected common-c gitlink'
git -C $fixture checkout --quiet aeba988e0a302145eff2e70fa3e11cd98ec01f5d
if ($LASTEXITCODE) { throw 'New revision fixture unavailable' }
& "$PSScriptRoot/apply-common-c-p1a.ps1" -SourceRoot $scratch
$before = git -C $fixture diff
if ($LASTEXITCODE -or !$before) { throw 'Patch did not change reviewed sources' }
& "$PSScriptRoot/apply-common-c-p1a.ps1" -SourceRoot $scratch
$after = git -C $fixture diff
if ($LASTEXITCODE -or (Compare-Object $before $after)) { throw 'Second application changed sources' }
git -C $fixture apply --reverse --check $patch
if ($LASTEXITCODE) { throw 'Patched source no longer matches reviewed delta' }

# Same validation used for corresponding-source archives (no Git metadata).
$archive = Join-Path $scratch 'archive'
New-Item -ItemType Directory -Force $archive | Out-Null
Copy-Item -LiteralPath (Join-Path $fixture 'src') -Destination $archive -Recurse
git -C $archive apply --reverse --check $patch
if ($LASTEXITCODE) { throw 'Patched archive rejected' }
$header = Join-Path $archive 'src/PyroWave.h'
(Get-Content -LiteralPath $header -Raw).Replace('186f0393', 'deadbeef') | Set-Content -LiteralPath $header
git -C $archive apply --reverse --check $patch 2>$null
if (!$LASTEXITCODE) { throw 'Modified archive incorrectly accepted' }

# An unexpected local edit must fail rather than be overwritten or ignored.
$header = Join-Path $fixture 'src/PyroWave.h'
(Get-Content -LiteralPath $header -Raw).Replace('186f0393', 'deadbeef') | Set-Content -LiteralPath $header
Assert-Rejected { & "$PSScriptRoot/apply-common-c-p1a.ps1" -SourceRoot $scratch } 'cannot apply cleanly'
Write-Host 'PASS: wrong pin rejected, reviewed pin accepted, idempotence, modified source/archive rejected'
exit 0
