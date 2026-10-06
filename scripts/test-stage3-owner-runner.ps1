# Next step: post-v0.2.0 native Vulkan presentation

[Stage 3](../tests/pyrowave/vulkan/STAGE3.md) is an isolated offline shared-device
decode proof. Owner Surface qualification is pending. Live Vulkan presentation,
production integration and Stage 4/5 remain future work; the confirmed pinned
borrowed-factory failure cleanup leak blocks production promotion pending review.

## Current state: v0.2.0 released

Live PyroWave SDR 8-bit 4:2:0 has been validated against Vibepollo on native
Surface Pro 11 / Snapdragon X Plus / Qualcomm Adreno X1-85 ARM64. It remains
explicitly selected and **Experimental**; Automatic chooses standard codecs.
Normal Windows x64 and ARM64 builds include the pinned API 0.6.0 runtime,
bitstream `186f0393`, restricted loading and provenance metadata.
Codec-aware bitrate QoL is integrated: standard codecs have a 500 Mbps UI ceiling,
PyroWave a 3000 Mbps ceiling, and PyroWave automatic bitrate is approximately
`width * height * fps * 1.6` bits/s. Manual overrides survive resolution/FPS changes.
The codec selector remains in Basic Settings.

The tested Qualcomm driver rejects the Vulkan/D3D11 shared-fence import with
`PYROWAVE_ERROR_UNSUPPORTED_EXTERNAL_HANDLE`. Fragment decode is preferred for
the GPU interop probe; Asteria safely recreates the decoder for compute-path
decode with CPU I420 readback/presentation. This working fallback does not imply
that Adreno cannot decode PyroWave. See [current owner evidence](VALIDATION.md#current-live-arm64-owner-result).
Broader hardware, GPU interop and performance qualification remain open.

Moonlight PC v6.2.0 is the upstream baseline; weekly upstream/master proposals
preserve history and require human review. See [UPSTREAM_SYNC.md](UPSTREAM_SYNC.md).
The current release is [Asteria v0.2.0](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.2.0), an unsigned Windows
x64/native ARM64 portable release. Its binaries and corresponding source are
from `42f756e465288157608fe894e3a3dfe800b05344`; subsequent release-status
updates are documentation-only. v0.1.0 evidence below remains historical.


v0.2.0 is published with same-commit Windows x64/native ARM64 qualification and
PyroWave CI. See the [release record](VALIDATION.md#asteria-v020-release-record)
for commit, run IDs, checksums and hardware-validation limits. Future releases
repeat their own qualification; this evidence is specific to v0.2.0.

The next major PyroWave performance milestone is **post-v0.2.0**:
`PyroWave Vulkan decode -> GPU-resident Y/U/V -> Vulkan presentation shader -> Vulkan swapchain`.
It aims to avoid Vulkan -> CPU -> D3D11 readback/re-upload and dependence on
Vulkan/D3D11 external-fence sharing, reduce presentation overhead and improve
ARM64/Qualcomm viability while retaining safe fallback paths. It is not implemented.
M1A remains measurement-first; standard codecs and Moonlight behavior are the
baseline. This is architectural work rather than more CPU-fallback micro-optimization.
P1b live records/FEC/partial recovery and bandwidth probing remain later work.
After the performance phase, isolated sessions / MultiSeat are the next major
feature area, followed by Asteria VR. Session-lifecycle work may benefit VR but
is not a hard dependency, and VR does not require PyroWave. Both need a separate
Asteria-oriented Vibepollo fork/host extension;
see [the roadmap](PORTING_PLAN.md#host-dependent-roadmap-boundaries).
Old M2–M4 Apollo convenience work is deprioritized; its notes are retained as
possible supporting integrations rather than standalone active milestones.

## Historical P0/P0.5 evidence

**P0-R is COMPLETE** on Windows x64 RTX 4070 Ti and native Windows ARM64
Surface Pro 11th Edition / Snapdragon X Plus / Adreno X1-85. The exact codec is
`186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, API 0.6.0.
Both compatibility and record framing pass expected I420 output through three
decoder lifetimes, load/reload and malformed rejection/recovery. See the
[current hardware record](VALIDATION.md#m1b-p0-r-vibepollo-validation) and
[Vibepollo contract](PYROWAVE_VIBEPOLLO.md). Old `f6fb84...` evidence stays historical.

## P0.5 COMPLETE: offline presentation/color qualification

The implementation merged in [PR #20](https://github.com/Unitron07/Asteria-Windows/pull/20)
at `2d443c6347fd04489bda17bafceccea0bbfa4b65`; optional x64/native ARM64 CI passed.
Owner visual qualification **passed on RTX 4070 Ti x64 and native Surface Pro 11 /
Snapdragon X Plus / Adreno X1-85 ARM64**. Raw I420 and both compatibility/record
framing passed all …7584 tokens truncated…le.WriteAllText(args[i+1],"validation_status=SKIP\noverall=SKIP\n");
  return 77;
 }
}
'@ | Set-Content -LiteralPath $fixture
$csc = Join-Path ([Environment]::GetFolderPath('Windows')) 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$fixtureExe = Join-Path $EvidenceRoot 'fixture.exe'
& $csc /nologo /platform:x64 /target:exe "/out:$fixtureExe" $fixture
if ($LASTEXITCODE) { throw 'Runner regression fixture compile failed' }
$shells = @((Get-Command powershell.exe -ErrorAction Stop).Source)
$pwsh = Get-Command pwsh.exe -ErrorAction SilentlyContinue
if ($pwsh) { $shells += $pwsh.Source }
foreach ($shell in $shells) {
    foreach ($case in @('default','explicit','whitespace')) {
        $package = Join-Path $EvidenceRoot ((Split-Path $shell -Leaf) + " $case package with spaces")
        New-Item -ItemType Directory -Force -Path $package,(Join-Path $package 'install/bin') | Out-Null
        foreach ($name in @('run-stage3-owner-tests.ps1','runtime-provenance.ps1','pe-machine.ps1')) {
            Copy-Item -LiteralPath (Join-Path $PackageRoot $name) -Destination $package
        }
        foreach ($name in @('pyrowave-vulkan-shared-device.exe','pyrowave-vulkan-shared-device-policy-tests.exe')) {
            Copy-Item -LiteralPath $fixtureExe -Destination (Join-Path $package $name)
        }
        $runtime = Join-Path $package 'install/bin/libpyrowave-shared-0.dll'
        Copy-Item -LiteralPath $fixtureExe -Destination $runtime
        @{architecture='x64';codecCommit='186f0393b77f7755953b5ecde994bb1cec2e4155';bitstreamId='186f0393';apiVersion='0.6.0';
            sha256=(Get-FileHash -LiteralPath $runtime).Hash.ToLowerInvariant()} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'install/bin/pyrowave-runtime.json')
        @{architecture='x64';sourceRevision='GPU_FREE_FIXTURE'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'build.json')
        Get-ChildItem -LiteralPath $package -Recurse -File | ForEach-Object {
            @{file=$_.FullName.Substring($package.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}
        } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'sha256.json')
        $expected = Join-Path $package 'stage3-evidence'
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $package 'run-stage3-owner-tests.ps1'))
        if ($case -eq 'explicit') { $expected = Join-Path $EvidenceRoot ((Split-Path $shell -Leaf) + ' explicit evidence'); $arguments += @('-EvidenceRoot',$expected) }
        if ($case -eq 'whitespace') { $arguments += @('-EvidenceRoot','   ') }
        $start = New-Object Diagnostics.ProcessStartInfo
        $start.FileName = $shell; $start.Arguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $start.WorkingDirectory = $unrelated; $start.UseShellExecute = $false; $start.CreateNoWindow = $true
        if ((Split-Path $shell -Leaf) -ieq 'powershell.exe') {
            # Match native PowerShell 7 -> Windows PowerShell launching, which
            # strips the host kit's incompatible module directories.
            $start.EnvironmentVariables['PSModulePath'] = Join-Path ([Environment]::GetFolderPath('Windows')) 'System32/WindowsPowerShell/v1.0/Modules'
        }
        $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($start)
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Runner regression timeout' }
        $code = $process.ExitCode; $process.Dispose()
        if ($code -ne 77) { throw "Runner SKIP fixture returned ${code}: $($stdout.Result) $($stderr.Result)" }
        $record = Get-Content -LiteralPath (Join-Path $expected 'owner-result.json') -Raw | ConvertFrom-Json
        if ($record.overall -cne 'SKIP' -or $record.sourceRevision -cne 'GPU_FREE_FIXTURE') { throw 'Runner evidence mismatch' }
        if (Test-Path -LiteralPath (Join-Path $unrelated 'stage3-evidence')) { throw 'Runner wrote relative to CWD' }
    }
}
Write-Output 'PASS: actual packaged Stage 3 runner, Windows PowerShell 5.1/pwsh, unrelated CWD, spaces/default/explicit roots, explicit SKIP'
