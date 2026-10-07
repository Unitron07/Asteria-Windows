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
$factoryTests = Join-Path $scriptDir 'pyrowave-factory-cleanup-tests.exe'
Assert-PyroWavePe $factoryTests $build.architecture | Out-Null
$proofPath = Join-Path $scriptDir 'install/bin/factory-ownership.json'
$proof = Get-Content -LiteralPath $proofPath -Raw | ConvertFrom-Json
$patch = Join-Path $scriptDir 'source-notices/runtime/patches/0004-borrowed-factory-cleanup.patch'
if ($runtime.factoryCleanupSourceTest -cne 'PASSED' -or $runtime.factoryCleanupPatch -cne '0004-borrowed-factory-cleanup.patch' -or
    $runtime.factoryCleanupPatchSha256 -cne (Get-FileHash -LiteralPath $patch).Hash.ToLowerInvariant() -or
    $runtime.factoryOwnershipSha256 -cne (Get-FileHash -LiteralPath $proofPath).Hash.ToLowerInvariant() -or
    $proof.result -cne 'PASSED' -or $proof.liveWrappers -ne 0 -or $proof.allocations -ne $proof.destructions -or
    $proof.testBinarySha256 -cne (Get-FileHash -LiteralPath $factoryTests).Hash.ToLowerInvariant()) { throw 'Factory cleanup provenance/ownership proof mismatch' }
& $factoryTests
if ($LASTEXITCODE) { throw 'Deterministic pinned factory ownership test failed' }
$extra = if ($Surface) { @('--surface','--expect-device','X1-85') } else { @() }
function Invoke-Stage3([string]$Name,[string[]]$ExtraArguments) {
    $log = Join-Path $EvidenceRoot "$Name.log"
    # Each isolated GPU/fault process has a bounded timeout.
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
        elseif ($code) { 'FAIL' } elseif ($Name -in @('decode','forced-compute-decode')) { 'API_PASS' } else { 'DIAGNOSTIC_PASS' }
    $record = @{name=$Name;overall=$status;exitCode=$code;fields=$fields;logEntries=$lines;
        sourceRevision=$build.sourceRevision;architecture=$build.architecture;runtime=$runtime;surface=$Surface.IsPresent;
        scope='OFFLINE ONLY';liveStreaming=$false;productionPromotion='NOT_AUTHORIZED_STAGE4';runtimePatch=$runtime.factoryCleanupPatch}
    $record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $EvidenceRoot "$Name.json") -Encoding utf8
    return $record
}
$runs = @()
$summary = $null
# Both modes are required for every owner qualification, including x64.
$diagnosticArgs = @('--diagnostic-suite') + $extra
if ($Surface -or $DiagnosticPrefill) { $diagnosticArgs += '--diagnostic-prefill' }
if ($DiagnosticDeviceIdle) { $diagnosticArgs += '--diagnostic-device-idle' }
$started = [DateTime]::UtcNow
$diagnosticRun = Invoke-Stage3 'diagnostic-suite' $diagnosticArgs
$runs += $diagnosticRun
$summaryPath = Join-Path $EvidenceRoot 'diagnostic-summary.json'
$targetPass = 'TARGETED_NUMERIC_EQUIVALENCE_FULL_SUITE_PENDING'
if ($diagnosticRun.overall -eq 'DIAGNOSTIC_COMPLETE') {
    if (!(Test-Path -LiteralPath $summaryPath) -or (Get-Item -LiteralPath $summaryPath).LastWriteTimeUtc -lt $started) { throw 'Missing/current diagnostic summary required' }
    $summary = [IO.File]::ReadAllText($summaryPath) | ConvertFrom-Json
    if ($summary.overall -cne 'DIAGNOSTIC_COMPLETE' -or $summary.sourceRevision -cne $build.sourceRevision) { throw 'Diagnostic summary identity mismatch' }
    if ($summary.stage3Qualification -eq $targetPass) {
        $hardGates = $summary.autoNumericallyEquivalent -eq $true -and $summary.forcedComputeNumericallyEquivalent -eq $true -and
            $summary.stage3Tolerance -eq 1 -and $null -ne $summary.autoMaxAbsoluteError -and $summary.autoMaxAbsoluteError -le 1 -and
            $null -ne $summary.forcedComputeMaxAbsoluteError -and $summary.forcedComputeMaxAbsoluteError -le 1 -and
            $summary.repeatHashStable -eq $true -and $summary.deviceIdleChangedBytes -eq $false -and $summary.experimentsStable -eq $true -and
            $summary.sameCallerDevice -eq $true -and $summary.identicalEncodedFixtures -eq $true -and $summary.fixtureCount -eq 7 -and
            $summary.callerOwnedR8Images -eq $true -and $summary.positiveTimelines -eq $true -and
            $null -ne $summary.externalMemoryHandles -and $summary.externalMemoryHandles -eq 0 -and
            $null -ne $summary.externalSemaphoreHandles -and $summary.externalSemaphoreHandles -eq 0 -and
            $null -ne $summary.d3d11Resources -and $summary.d3d11Resources -eq 0 -and $summary.externalHandleApiUsage -ceq 'NONE' -and
            $summary.autoActualPath -in @('fragment','compute') -and $summary.forcedComputeActualPath -ceq 'compute' -and
            (!$Surface -or $summary.autoActualPath -ceq 'fragment') -and (!$DiagnosticDeviceIdle -or $summary.deviceIdleTested -eq $true)
        foreach ($name in @('auto-fragment.json','forced-compute.json','repeatability.json')) {
            $path = Join-Path $EvidenceRoot $name
            if (!(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $started) { $hardGates=$false; continue }
            $data = [IO.File]::ReadAllText($path) | ConvertFrom-Json
            if ($name -eq 'repeatability.json') {
                if ($data.repeat_hash_stable -cne 'YES') { $hardGates=$false }
                $comparisons = @($data.records | Where-Object { $_.comparison } | ForEach-Object { $_.comparison })
                if ($comparisons.Count -ne (168 + $(if ($Surface -or $DiagnosticPrefill) { 84 } else { 0 }) + $(if ($DiagnosticDeviceIdle) { 42 } else { 0 }))) { $hardGates=$false }
            } else {
                $comparisons = @($data.planes)
                if ($comparisons.Count -ne 21) { $hardGates=$false }
            }
            foreach ($comparison in $comparisons) {
                if ($null -eq $comparison.metrics.max_absolute_error -or $comparison.metrics.max_absolute_error -gt 1 -or
                    $comparison.metrics.numerically_equivalent -ne $true -or $comparison.metrics.stage3_tolerance -ne 1) { $hardGates=$false }
            }
        }
        if (!$hardGates) { $summary.stage3Qualification='FAIL_DIAGNOSTIC_HARD_GATE' }
    }
    if (($diagnosticRun.fields['validation_errors'] | Where-Object { [int]$_ -gt 0 }) -or
        ($diagnosticRun.exitCode -ne 0 -and $summary.stage3Qualification -eq $targetPass)) {
        $diagnosticRun.overall='FAIL'; $summary.stage3Qualification='FAIL_VALIDATION_OR_CLEANUP'
    }
}
$decode = $null
function Confirm-FullSuite($run) {
    if ($run.exitCode -eq 0 -and ($run.fields['comparison'] -notcontains 'NUMERIC_EQUIVALENCE' -or
        $run.fields['stage3_tolerance'] -notcontains '1' -or $run.fields['frames_tested'] -notcontains '144' -or
        $run.fields['slots_exercised'] -notcontains '3' -or $run.fields['decoder_lifetimes'] -notcontains '3' -or
        $run.fields['malformed_recovery'] -notcontains 'PASS' -or !$run.fields['max_absolute_error'] -or
        ($run.fields['max_absolute_error'] | Where-Object { [int]$_ -gt 1 }) -or
        $run.fields['numerically_equivalent'] -contains 'false')) { $run.exitCode=1; $run.overall='FAIL' }
}
if ($summary -and $summary.stage3Qualification -eq $targetPass) {
    $decode = Invoke-Stage3 'decode' $extra; $runs += $decode
    Confirm-FullSuite $decode
    if ($decode.exitCode -eq 0) {
        $forcedDecode = Invoke-Stage3 'forced-compute-decode' (@('--force-compute') + $extra); $runs += $forcedDecode
        Confirm-FullSuite $forcedDecode
    }
    if (@($runs | Where-Object { $_.exitCode -ne 0 }).Count -eq 0) {
        foreach ($fault in @('borrowed','partial-images','decoded','reused','rejected')) {
            $runs += Invoke-Stage3 "fault-$fault" (@('--fail-at',$fault) + $extra)
            if ($runs[-1].exitCode) { break }
        }
    }
}
$factory = $null
if ($summary) { $factory = Invoke-Stage3 'factory-fault' @('--factory-fault'); $runs += $factory }
$factoryCleanup = if ($factory -and $factory.exitCode -eq 0 -and $factory.fields['factory_failure_cleanup'] -contains 'PATCHED_AND_VERIFIED') { 'PATCHED_AND_VERIFIED' } else { 'PENDING_OR_FAILED_PUBLIC_FAULT' }
$unexpectedFailure = @($runs | Where-Object { $_.overall -eq 'FAIL' }).Count
$status = if ($unexpectedFailure -or ($summary -and $factoryCleanup -ne 'PATCHED_AND_VERIFIED')) { 'FAIL' }
    elseif ($summary -and $summary.stage3Qualification -ne $targetPass) { 'DIAGNOSTIC_COMPLETE' }
    elseif (@($runs | Where-Object { $_.exitCode -eq 77 }).Count) { 'SKIP' }
    elseif ($runs.Count -ne 9) { 'FAIL' } else { 'API_PASS' }
$qualification = if ($status -eq 'API_PASS') { 'PASS_NUMERIC_EQUIVALENCE' }
    elseif ($summary -and $status -eq 'DIAGNOSTIC_COMPLETE') { $summary.stage3Qualification } else { $status }
$validation = @($runs | ForEach-Object { $_.fields['validation_status'] } | Where-Object { $_ -and $_ -ne 'SKIP' })
$validationStatus = if (!$validation.Count) { 'SKIP' } elseif ($validation -contains 'ERROR') { 'ERROR' } elseif ($validation -contains 'WARNINGS_REVIEW_REQUIRED') { 'WARNINGS_REVIEW_REQUIRED' } else { 'NO_REPORTED_ERRORS' }
$selectedGPU = $diagnosticRun.fields['selected_device'] | Select-Object -First 1
$record = @{overall=$status;sourceRevision=$build.sourceRevision;architecture=$build.architecture;selectedGPU=$selectedGPU;
    runtimeHash=$runtime.sha256;runtimePatch=$runtime.factoryCleanupPatch;runtimePatchSha256=$runtime.factoryCleanupPatchSha256;runtimeProvenance=$runtime;
    runs=$runs;stage3Qualification=$qualification;diagnostics=$summary;factoryCleanup=$factoryCleanup;validation=$validationStatus;
    ownerSurfaceQualification=$(if ($Surface -and $status -eq 'API_PASS') { 'PASS_NUMERIC_EQUIVALENCE_OWNER_RUN' } else { 'PENDING' });
    validationReview='Inspect all VALIDATION WARNING lines; unavailable validation is SKIP';productionPromotion='NOT_AUTHORIZED_STAGE4'}
foreach ($name in @('autoActualPath','forcedComputeActualPath','sameCallerDevice','externalMemoryHandles','externalSemaphoreHandles','d3d11Resources',
    'autoExact','autoNumericallyEquivalent','forcedComputeExact','forcedComputeNumericallyEquivalent','autoMaxAbsoluteError','forcedComputeMaxAbsoluteError',
    'repeatHashStable','deviceIdleTested','deviceIdleChangedBytes','identicalEncodedFixtures','callerOwnedR8Images','positiveTimelines')) {
    $record[$name] = if ($summary) { $summary.$name } else { $null }
}
foreach ($mode in @(@{prefix='auto';name='decode'},@{prefix='forcedCompute';name='forced-compute-decode'})) {
    $full = $runs | Where-Object { $_.name -ceq $mode.name } | Select-Object -First 1
    if ($full) {
        $maximum = @($full.fields['max_absolute_error'] | ForEach-Object { [int]$_ } | Measure-Object -Maximum).Maximum
        $record[$mode.prefix+'MaxAbsoluteError'] = [Math]::Max([int]$record[$mode.prefix+'MaxAbsoluteError'],[int]$maximum)
        $record[$mode.prefix+'Exact'] = $record[$mode.prefix+'Exact'] -and $full.fields['exact_equal'] -and $full.fields['exact_equal'][-1] -ceq 'true'
        $record[$mode.prefix+'NumericallyEquivalent'] = $record[$mode.prefix+'NumericallyEquivalent'] -and $full.exitCode -eq 0
    }
}
$record | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'owner-result.json') -Encoding utf8
Write-Host "Stage 3 $qualification. Evidence: $EvidenceRoot. Final Surface evidence remains subject to owner review."
if ($status -eq 'FAIL' -or $status -eq 'DIAGNOSTIC_COMPLETE') { exit 1 }; if ($status -eq 'SKIP') { exit 77 }; exit 0
