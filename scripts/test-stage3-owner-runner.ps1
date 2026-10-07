[CmdletBinding()]
param([Parameter(Mandatory)][string]$PackageRoot,[Parameter(Mandatory)][string]$EvidenceRoot,[string]$FixtureExe)
$ErrorActionPreference = 'Stop'
$PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$runner = Join-Path $PackageRoot 'run-stage3-owner-tests.ps1'
if ((Get-Content -LiteralPath $runner -Raw) -match '(?i)[a-z]:[\\/]|/users/|/home/') { throw 'Runner contains developer-local paths' }
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$unrelated = Join-Path $EvidenceRoot 'unrelated working directory'
New-Item -ItemType Directory -Force -Path $unrelated | Out-Null
$fixtureArch = 'x64'
if (![string]::IsNullOrWhiteSpace($FixtureExe)) {
    $FixtureExe = (Resolve-Path -LiteralPath $FixtureExe).Path
    $fixtureArch = (Get-Content -LiteralPath (Join-Path $PackageRoot 'build.json') -Raw | ConvertFrom-Json).architecture
    . (Join-Path $PackageRoot 'pe-machine.ps1')
    Assert-PyroWavePe $FixtureExe $fixtureArch | Out-Null
    $fixtureExe = $FixtureExe
} else {
$fixture = Join-Path $EvidenceRoot 'Unavailable.cs'
@'
using System; using System.IO;
public static class Fixture {
 public static int Main(string[] args) {
  if(Environment.GetCommandLineArgs()[0].Contains("-tests")) return 0;
  string log=null; bool diagnostic=false,factory=false;
  for(int i=0;i<args.Length;i++) {
   if((args[i]=="--runtime" || args[i]=="--log" || args[i]=="--fail-at") && i+1<args.Length) {
    if(args[i]=="--log") log=args[i+1]; i++;
   } else if(args[i]=="--diagnostic-suite") diagnostic=true;
   else if(args[i]=="--factory-fault") factory=true;
   else if(args[i]!="--diagnostic-prefill" && args[i]!="--diagnostic-device-idle" && args[i]!="--force-compute") return 2;
  }
  if(log==null) return 2;
  string root=Environment.GetEnvironmentVariable("STAGE3_FIXTURE_DATA");
  if(root==null) { File.WriteAllText(log,"validation_status=SKIP\noverall=SKIP\n"); return 77; }
  if(diagnostic) {
   foreach(string name in new string[]{"diagnostic-summary.json","auto-fragment.json","forced-compute.json","repeatability.json"}) {
    File.Copy(Path.Combine(root,name),Path.Combine(Path.GetDirectoryName(log),name),true);
    File.SetLastWriteTimeUtc(Path.Combine(Path.GetDirectoryName(log),name),DateTime.UtcNow);
   }
   File.WriteAllText(log,"selected_device=GPU_FREE_FIXTURE\ndiagnostic_complete=YES validation_errors=0 validation_status=SKIP overall=DIAGNOSTIC_COMPLETE\n"); return 0;
  }
  if(factory) {
   bool failed=File.Exists(Path.Combine(root,"factory-fail"));
   File.WriteAllText(log,"factory_failure_cleanup="+(failed ? "FAIL" : "PATCHED_AND_VERIFIED")+" validation_status=SKIP\n"); return failed ? 1 : 0;
  }
  bool failedDecode=File.Exists(Path.Combine(root,"decode-fail"));
  File.WriteAllText(log,"comparison=NUMERIC_EQUIVALENCE stage3_tolerance=1 numerically_equivalent=true exact_equal=false max_absolute_error=1 frames_tested=144 slots_exercised=3 decoder_lifetimes=3 malformed_recovery=PASS\nvalidation_status=SKIP\n"); return failedDecode ? 1 : 0;
 }
}
'@ | Set-Content -LiteralPath $fixture
$csc = Join-Path ([Environment]::GetFolderPath('Windows')) 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$fixtureExe = Join-Path $EvidenceRoot 'fixture.exe'
& $csc /nologo /platform:x64 /target:exe "/out:$fixtureExe" $fixture
if ($LASTEXITCODE) { throw 'Runner regression fixture compile failed' }
}
$shells = @((Get-Command powershell.exe -ErrorAction Stop).Source)
$pwsh = Get-Command pwsh.exe -ErrorAction SilentlyContinue
if ($pwsh) { $shells += $pwsh.Source }
foreach ($shell in $shells) {
    foreach ($case in @('default','explicit','whitespace','diagnostic-skip','numeric-pass','tolerance-fail','hidden-plane-fail','unstable-fail','idle-fail','device-fail','path-fail','fixture-fail','ownership-fail','timeline-fail','factory-fail','decode-fail')) {
        $package = Join-Path $EvidenceRoot ((Split-Path $shell -Leaf) + " $case package with spaces")
        New-Item -ItemType Directory -Force -Path $package,(Join-Path $package 'install/bin'),(Join-Path $package 'source-notices/runtime/patches') | Out-Null
        foreach ($name in @('run-stage3-owner-tests.ps1','runtime-provenance.ps1','pe-machine.ps1')) {
            Copy-Item -LiteralPath (Join-Path $PackageRoot $name) -Destination $package
        }
        foreach ($name in @('pyrowave-vulkan-shared-device.exe','pyrowave-vulkan-shared-device-policy-tests.exe','pyrowave-vulkan-shared-diagnostic-tests.exe','pyrowave-factory-cleanup-tests.exe')) {
            Copy-Item -LiteralPath $fixtureExe -Destination (Join-Path $package $name)
        }
        $runtime = Join-Path $package 'install/bin/libpyrowave-shared-0.dll'
        Copy-Item -LiteralPath $fixtureExe -Destination $runtime
        $patch = Join-Path $package 'source-notices/runtime/patches/0004-borrowed-factory-cleanup.patch'
        'GPU_FREE_PATCH_FIXTURE' | Set-Content -LiteralPath $patch
        $proofPath = Join-Path $package 'install/bin/factory-ownership.json'
        @{result='PASSED';liveWrappers=0;allocations=30000;destructions=30000;testBinarySha256=(Get-FileHash -LiteralPath $fixtureExe).Hash.ToLowerInvariant()} |
            ConvertTo-Json | Set-Content -LiteralPath $proofPath
        @{architecture=$fixtureArch;codecCommit='186f0393b77f7755953b5ecde994bb1cec2e4155';bitstreamId='186f0393';apiVersion='0.6.0';
            factoryCleanupSourceTest='PASSED';factoryCleanupPatch='0004-borrowed-factory-cleanup.patch';
            factoryCleanupPatchSha256=(Get-FileHash -LiteralPath $patch).Hash.ToLowerInvariant();
            factoryOwnershipSha256=(Get-FileHash -LiteralPath $proofPath).Hash.ToLowerInvariant();
            sha256=(Get-FileHash -LiteralPath $runtime).Hash.ToLowerInvariant()} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'install/bin/pyrowave-runtime.json')
        @{architecture=$fixtureArch;sourceRevision='GPU_FREE_FIXTURE'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'build.json')
        $fixtureData = $null
        if ($case -notin @('default','explicit','whitespace','diagnostic-skip')) {
            $fixtureData = Join-Path $package 'fixture-data'; New-Item -ItemType Directory -Path $fixtureData | Out-Null
            $summary = @{sourceRevision='GPU_FREE_FIXTURE';overall='DIAGNOSTIC_COMPLETE';stage3Qualification='TARGETED_NUMERIC_EQUIVALENCE_FULL_SUITE_PENDING';
                autoExact=$false;forcedComputeExact=$false;autoNumericallyEquivalent=$true;forcedComputeNumericallyEquivalent=$true;
                autoMaxAbsoluteError=1;forcedComputeMaxAbsoluteError=1;stage3Tolerance=1;autoActualPath='fragment';forcedComputeActualPath='compute';
                repeatHashStable=$true;deviceIdleTested=$true;deviceIdleChangedBytes=$false;experimentsStable=$true;sameCallerDevice=$true;
                identicalEncodedFixtures=$true;fixtureCount=7;callerOwnedR8Images=$true;positiveTimelines=$true;
                externalMemoryHandles=0;externalSemaphoreHandles=0;d3d11Resources=0;externalHandleApiUsage='NONE'}
            switch ($case) {
                'tolerance-fail' { $summary.autoMaxAbsoluteError=2 }
                'unstable-fail' { $summary.repeatHashStable=$false }
                'idle-fail' { $summary.deviceIdleChangedBytes=$true }
                'device-fail' { $summary.sameCallerDevice=$false }
                'path-fail' { $summary.forcedComputeActualPath='fragment' }
                'fixture-fail' { $summary.identicalEncodedFixtures=$false }
                'ownership-fail' { $summary.externalMemoryHandles=1 }
                'timeline-fail' { $summary.positiveTimelines=$false }
                'factory-fail' { 'fail' | Set-Content (Join-Path $fixtureData 'factory-fail') }
                'decode-fail' { 'fail' | Set-Content (Join-Path $fixtureData 'decode-fail') }
            }
            $summary | ConvertTo-Json | Set-Content (Join-Path $fixtureData 'diagnostic-summary.json')
            $comparison = @{exact_equal=$false;metrics=@{max_absolute_error=1;numerically_equivalent=$true;stage3_tolerance=1}}
            foreach ($name in @('auto-fragment.json','forced-compute.json')) {
                $planes = @(1..21 | ForEach-Object { $comparison })
                if ($case -eq 'hidden-plane-fail' -and $name -eq 'auto-fragment.json') { $planes[20]=@{metrics=@{max_absolute_error=2;numerically_equivalent=$false;stage3_tolerance=1}} }
                @{planes=$planes} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $fixtureData $name)
            }
            @{repeat_hash_stable='YES';records=@(1..294 | ForEach-Object { @{comparison=$comparison} })} |
                ConvertTo-Json -Depth 6 | Set-Content (Join-Path $fixtureData 'repeatability.json')
        }
        Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object {
            @{file=$_.FullName.Substring($package.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'sha256.json')
        $expected = Join-Path $package 'stage3-evidence'
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $package 'run-stage3-owner-tests.ps1'),'-Verbose')
        if ($case -eq 'explicit') { $expected = Join-Path $EvidenceRoot ((Split-Path $shell -Leaf) + ' explicit evidence'); $arguments += @('-EvidenceRoot',$expected) }
        if ($case -eq 'whitespace') { $arguments += @('-EvidenceRoot','   ') }
        if ($case -notin @('default','explicit','whitespace')) { $arguments += @('-Diagnostic','-DiagnosticDeviceIdle','-DiagnosticPrefill') }
        # Use PowerShell's native launcher, as in the documented owner command.
        # It handles cross-version module-path hygiene on Windows/ARM64. The CI
        # step bounds shell startup; each actual GPU process has its own timeout.
        $PSNativeCommandUseErrorActionPreference = $false
        Write-Output "Runner case: $(Split-Path $shell -Leaf)/$case fixture_arch=$fixtureArch"
        Push-Location -LiteralPath $unrelated
        try {
            if ($fixtureData) { $env:STAGE3_FIXTURE_DATA=$fixtureData }
            $output = & $shell @arguments 2>&1 | Out-String; $code = $LASTEXITCODE
        } finally { Remove-Item Env:\STAGE3_FIXTURE_DATA -ErrorAction SilentlyContinue; Pop-Location }
        $output | Set-Content -LiteralPath (Join-Path $package 'child-output.txt')
        $record = Get-Content -LiteralPath (Join-Path $expected 'owner-result.json') -Raw | ConvertFrom-Json
        if ($case -eq 'numeric-pass') {
            if ($code -ne 0 -or $record.stage3Qualification -cne 'PASS_NUMERIC_EQUIVALENCE' -or $record.runs.Count -ne 9 -or
                $record.autoExact -ne $false -or $record.forcedComputeExact -ne $false -or $record.validation -cne 'SKIP') { throw "Bounded non-bitexact PASS lost: $output" }
        } elseif ($fixtureData) {
            if ($code -ne 1 -or $record.stage3Qualification -eq 'PASS_NUMERIC_EQUIVALENCE') { throw "Hard gate was hidden: $case $output" }
            if ($case -notin @('factory-fail','decode-fail') -and $record.runs.Count -ne 2) { throw 'Invalid diagnostic ran the full qualification suite' }
        } else {
            if ($code -ne 77 -or $record.overall -cne 'SKIP') { throw "Runner SKIP fixture returned ${code}: $output" }
        }
        if ($record.sourceRevision -cne 'GPU_FREE_FIXTURE') { throw 'Runner evidence identity mismatch' }
        if (Test-Path -LiteralPath (Join-Path $unrelated 'stage3-evidence')) { throw 'Runner wrote relative to CWD' }
    }
}
Write-Output 'PASS: PS5.1/7 roots/CWD/SKIP, numeric non-bitexact PASS, +2 and hard-gate failures retained'
$global:LASTEXITCODE = 0 # All intentional child outcomes verified.
