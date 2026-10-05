[CmdletBinding()]
param([switch]$Surface, [string]$EvidenceRoot = (Join-Path $PSScriptRoot 'owner-evidence'))
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot 'pyrowave-vulkan-probe.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'Run this script from the extracted architecture-matching owner package' }
if ($Surface -and [Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'Arm64') {
    throw 'Surface qualification requires Windows ARM64; use the ARM64 artifact'
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$extra = if ($Surface) { @('--expect-device','X1-85') } else { @() }
$runs = @(
    @{name='flagged'; args=@('--exercise','--seconds','20')},
    @{name='unflagged'; args=@('--without-vulkan-flag','--exercise','--seconds','20')},
    @{name='no-vsync'; args=@('--no-vsync','--exercise','--seconds','20')}
)
$observations = @()
foreach ($run in $runs) {
    Write-Host "`n$($run.name): watch six color bars with black/white/gray below."
    Write-Host 'The window automatically resizes and minimizes/restores. R rebuilds; F toggles fullscreen.'
    Write-Host 'At the end the SAME window must show green with a white rectangle for three seconds.'
    & $exe @($run.args) @extra --log (Join-Path $EvidenceRoot "$($run.name).log")
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        @{status=$(if ($code -eq 77) {'SKIP'} else {'FAIL'});exitCode=$code;run=$run.name;stage3Authorized=$false} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'owner-result.json')
        throw "Owner run did not complete: $($run.name), code $code (77 means unavailable, not PASS)"
    }
    $visual = Read-Host 'Were bars visible, resizing/minimize/restore correct, repeated rebuilds responsive, and green/white SDL fallback visible on the same window? Type YES only if all were observed'
    $notes = Read-Host 'Record artifacts, hangs, flicker, focus issues, or other observations (enter none if none)'
    $observations += @{run=$run.name;apiExitCode=$code;visualConfirmed=($visual -ceq 'YES');notes=$notes}
}
$logs = @($runs | ForEach-Object { Get-Content -LiteralPath (Join-Path $EvidenceRoot "$($_.name).log") -Raw })
$validation = if (@($logs | Where-Object { $_ -match 'validation_status=SKIP' }).Count) { 'SKIP unavailable in one or more runs' } else { 'NO_REPORTED_ERRORS' }
$okay = @($observations | Where-Object { !$_.visualConfirmed }).Count -eq 0
@{status=$(if ($okay) {'OWNER_CONFIRMED'} else {'VISUAL_NOT_CONFIRMED'});surface=$Surface.IsPresent;
    os=[Environment]::OSVersion.VersionString;validation=$validation;observations=$observations;stage3Authorized=$false} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'owner-result.json')
Write-Host "Evidence is in $EvidenceRoot. Return it for review. This does not authorize Stage 3."

