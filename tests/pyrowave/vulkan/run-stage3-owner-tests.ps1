[CmdletBinding()]
param([switch]$Surface,[string]$EvidenceRoot,[switch]$Diagnostic,[switch]$DiagnosticDeviceIdle,[switch]$DiagnosticPrefill)
$ErrorActionPreference = 'Stop'
Write-Verbose "Stage 3 runner startup: PowerShell $($PSVersionTable.PSVersion)"
$scriptDir = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($scriptDir) -and $MyInvocation.MyCommand.Path) { $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path }
if ([string]::IsNullOrWhiteSpace($scriptDir)) { throw 'Cannot determine runner directory' }
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) { $EvidenceRoot = Join-Path $scriptDir 'stage3-evidence' }
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
Write-Verbose "Stage 3 runner evidence directory resolved"
. (Join-Path $scriptDir 'runtime-provenance.ps1')
$build = Get-Content -LiteralPath (Join-Path $scriptDir 'build.json') -Raw | ConvertFrom-Json
$exe = Join-Path $scriptDir 'pyrowave-vulkan-shared-device.exe'
$policy = Join-Path $scriptDir 'pyrowave-vulkan-shared-device-policy-tests.exe'
Assert-PyroWavePe $exe $build.architecture | Out-Null
Assert-PyroWavePe $policy $build.architecture | Out-Null
if ($Surface -and $build.architecture -cne 'arm64') { throw 'Surface requires the native ARM64 executable/package' }
$runtime = Assert-PyroWaveRuntime $scriptDir $build.architecture
$manifest = Get-Content -LiteralPath (Join-Path $scriptDir 'sha256.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest) {
    $path = Join-Path $scriptDir $entry.file
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry.sha256) { throw "Package hash mismatch: $($entry.file)" }
}
& $policy
if ($LASTEXITCODE) { throw 'GPU-free policy tests failed' }
$diagnosticTests = Join-Path $scriptDir 'pyrowave-vulkan-shared-diagnostic-tests.exe'
Assert-PyroWavePe $diagnosticTests $build.architecture | Out-Null
& $diagnosticTests
if ($LASTEXITCODE) { throw 'GPU-free diagnostic tests failed' }
$extra = if ($Surface) { @('--surface','--expect-device','X1-85') } else { @() }
function Invoke-Stage3([string]$Name,[string[]]$ExtraArguments) {
    $log = Join-Path $EvidenceRoot "$Name.log"
    # Process timeout also bounds driver hangs and the known failed-factory leak.
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $exe
    $arguments = @('--runtime',(Join-Path $scriptDir 'install/bin'),'--log',$log) +
        @($ExtraArguments | Where-Object { ![string]::IsNullOrWhiteSpace($_) })
    $start.Arguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $process = [Diagnostics.Process]::Start($start)
    if (!$process.WaitForExit(180000)) { $process.Kill(); $code = 1 } else { $code = $process.ExitCode }
    $process.Dispose()
    # Plain .NET strings avoid PowerShell 5.1 serializing Get-Content's attached
    # provider/drive metadata recursively at the evidence JSON depth.
    $lines = if (Test-Path -LiteralPath $log) { [IO.File]::ReadAllLines($log) } else { @('reason=process timeout or missing evidence') }
    $fields = @{}
    foreach ($line in $lines) {
        foreach ($match in [regex]::Matches($line,'(?<key>\b[a-zA-Z0-9_]+)=(?<value>.*?)(?= [a-zA-Z0-9_]+=|$)')) {
            $key = $match.Groups['key'].Value
            $fields[$key] = @($fields[$key]) + @($match.Groups['value'].Value) | Where-Object { $null -ne $_ }
        }
    }
    $status = if ($code -eq 77) { 'SKIP' } elseif ($Name -eq 'diagnostic-suite' -and $fields['diagnostic_complete'] -contains 'YES') { 'DIAGNOSTIC_COMPLETE' }
        elseif ($code) { 'FAIL' } elseif ($Name -eq 'decode') { 'API_PASS' } else { 'DIAGNOSTIC_PASS' }
    $record = @{name=$Name;overall=$status;exitCode=$code;fields=$fields;logEntries=$lines;
        sourceRevision=$build.sourceRevision;architecture=$build.architecture;runtime=$runtime;surface=$Surface.IsPresent;
        scope='OFFLINE ONLY';liveStreaming=$false;productionPromotion='BLOCKED_FACTORY_CLEANUP';runtimePatch='NONE_ADDED_FOR_STAGE3'}
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $EvidenceRoot "$Name.json") -Encoding utf8
    return $record
}
$runs = @()
$summary = $null
$diagnose = $Surface -or $Diagnostic
if (($DiagnosticDeviceIdle -or $DiagnosticPrefill) -and !$diagnose) { throw 'Diagnostic experiment requires -Surface or -Diagnostic' }
if ($diagnose) {
    $diagnosticArgs = @('--diagnostic-suite') + $extra
    if ($Surface -or $DiagnosticPrefill) { $diagnosticArgs += '--diagnostic-prefill' }
    if ($DiagnosticDeviceIdle) { $diagnosticArgs += '--diagnostic-device-idle' }
    $started = [DateTime]::UtcNow
    $diagnosticRun = Invoke-Stage3 'diagnostic-suite' $diagnosticArgs
    $runs += $diagnosticRun
    $summaryPath = Join-Path $EvidenceRoot 'diagnostic-summary.json'
    if ($diagnosticRun.overall -eq 'DIAGNOSTIC_COMPLETE') {
        if (!(Test-Path -LiteralPath $summaryPath) -or (Get-Item -LiteralPath $summaryPath).LastWriteTimeUtc -lt $started) { throw 'Missing/current diagnostic summary required' }
        $summary = [IO.File]::ReadAllText($summaryPath) | ConvertFrom-Json
        if ($summary.overall -cne 'DIAGNOSTIC_COMPLETE' -or $summary.sourceRevision -cne $build.sourceRevision) { throw 'Diagnostic summary identity mismatch' }
        if (($diagnosticRun.fields['validation_errors'] | Where-Object { [int]$_ -gt 0 }) -or
            ($diagnosticRun.exitCode -ne 0 -and $summary.stage3Qualification -eq 'TARGETED_EXACT_MATCH_FULL_SUITE_PENDING')) {
            $diagnosticRun.overall='FAIL'; $summary.stage3Qualification='FAIL_VALIDATION_OR_CLEANUP';
        }
    }
}
$decode = $null
if (!$diagnose -or ($summary -and $summary.stage3Qualification -eq 'TARGETED_EXACT_MATCH_FULL_SUITE_PENDING')) {
    $decode = Invoke-Stage3 'decode' $extra
    $runs += $decode
}
if ($decode -and $decode.exitCode -eq 0) {
    foreach ($fault in @('borrowed','partial-images','decoded','reused','rejected')) {
        $runs += Invoke-Stage3 "fault-$fault" (@('--fail-at',$fault) + $extra)
        if ($runs[-1].exitCode) { break }
    }
}
$factory = $null
if (($decode -and $decode.exitCode -eq 0) -or $summary) {
    $factory = Invoke-Stage3 'factory-fault' @('--factory-fault'); $runs += $factory
}
$unexpectedFailure = @($runs | Where-Object { $_.overall -eq 'FAIL' }).Count
$status = if ($unexpectedFailure) { 'FAIL' }
    elseif ($summary -and $summary.stage3Qualification -ne 'TARGETED_EXACT_MATCH_FULL_SUITE_PENDING') { 'DIAGNOSTIC_COMPLETE' }
    elseif (@($runs | Where-Object { $_.exitCode -eq 77 }).Count) { 'SKIP' } else { 'API_PASS' }
$qualification = if ($summary -and $status -eq 'DIAGNOSTIC_COMPLETE') { $summary.stage3Qualification }
    elseif ($status -eq 'API_PASS') { 'API_PASS_EXACT_FULL_SUITE' } else { $status }
@{overall=$status;sourceRevision=$build.sourceRevision;architecture=$build.architecture;runs=$runs;
    stage3Qualification=$qualification;diagnostics=$summary;
    ownerSurfaceQualification=$(if ($Surface -and $status -eq 'API_PASS') { 'API_PASS_OWNER_RUN' } elseif ($Surface -and $summary) { $qualification } else { 'PENDING' });
    validationReview='Inspect all VALIDATION WARNING lines; unavailable validation is SKIP';
    runtimePatch='NONE_ADDED_FOR_STAGE3';productionPromotion='BLOCKED_FACTORY_CLEANUP'} |
    ConvertTo-Json -Depth 14 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'owner-result.json') -Encoding utf8
Write-Host "Stage 3 $status. Evidence: $EvidenceRoot. Live presentation remains future work."
if ($status -eq 'FAIL' -or $status -eq 'DIAGNOSTIC_COMPLETE') { exit 1 }; if ($status -eq 'SKIP') { exit 77 }; exit 0
