# Phase 1B: exploratory evidence and optional controlled bitrate pilot

Status: **PREPARED; awaiting owner hardware results. Phase 1B incomplete.**
For the default exploratory pilot use the [short owner guide](PERFORMANCE_PHASE1B_QUICKSTART.md)
and small 250/500 Mbps configs; no manual metadata inventory is required.
The detailed setup requirements below apply only to explicitly requested
**controlled** qualification. Earlier ON/OFF commands in
[PERFORMANCE_PHASE1B.md](PERFORMANCE_PHASE1B.md) describe the previous design.
No production streaming, shaders, synchronization, presentation or codec pins change.
Keep PR #46 draft; do not merge or release. Stop after preparation/CI, then after
the four-run pilot for evidence review. Do not start full repeated benchmarks yet.

## Completed ten-run exploratory block

The owner completed ten 2560x1440@120 native ARM64 PyroWave streams: five profiling
ON and five OFF. The following findings are supplied by the independent analysis
in the experiment request. The raw block was not supplied to this preparation
checkout; this record does not claim a new independent raw-file validation.
Preserve its original configs, order, run IDs, manifests, captures, logs and report
as a separate exploratory block; do not backfill missing context or count recovered
copies as new repeats. Do not mix it with the historical single-run clues.

| Measured finding reported by independent analysis | Interpretation limit |
| --- | --- |
| All ten streams completed successfully | Functional success is not a pacing or causal performance result |
| Native Vulkan fragment path and clean shutdown verified | Qualification applies to that block; verify each new stream too |
| ON median presentation-drop rate lower than OFF | Exact rates and underlying counts were not supplied here; do not invent them |
| ON won only 3 of 5 matched pairs on presentation-drop rate | Mixed pairwise direction; the median alone does not establish a profiling benefit |
| Two ON runs had p99 new-frame submission intervals about 41 ms and 37 ms | Individual approximate histogram tails, not end-to-end latency; no OFF histogram comparison is available |
| Workload, network and power context missing | The completed block is exploratory, not a controlled causal comparison |

Hypotheses only: bitrate-dependent route pressure, host encode/playback variation,
client scheduling, display pacing, power/thermal drift, and profiling perturbation
could contribute. None is established by these runs. No profiling benefit, Vulkan
bottleneck or production optimization is justified. Lower bitrate is the next
discriminating test, not a presumed fix. Keep workload/route/power evidence so a
future result can narrow these explanations; bitrate also changes encoder work
and bitstream content, so an effect alone cannot isolate network from decode cost.

## Pilot design and fixed controls

| Order | Target | Profiling | Stream / capture policy |
| --- | --- | --- | --- |
| 1 (A1) | 250000 Kbps = 250 Mbps | ON | 1440p120, 10 s warmup, 60 s capture |
| 2 (B1) | 500000 Kbps = 500 Mbps | ON | identical |
| 3 (B2) | 500000 Kbps = 500 Mbps | ON | identical |
| 4 (A2) | 250000 Kbps = 250 Mbps | ON | identical |

ABBA counterbalances early/late order in this short feasibility pilot. There are
two streams per target, insufficient for a stable tail estimate or significance
claim. Pair A1/B1 and B2/A2 descriptively; keep chronological order. Profiling
stays ON for all four so both targets have the same capture metrics. This pilot
does not test profiling overhead. Targets are requested rates, not measured
payload, link capacity or guaranteed encoder output.

Hold the same qualified portable package/source/executable SHA-256, ARM64 GPU and
driver, PyroWave SDR 8-bit 4:2:0, actual NATIVE_VULKAN/fragment path, 2560x1440@120,
host version/hardware, replay file/player/phase, host encoder settings, client and
host display/refresh/HDR/VRR/V-sync/window mode, overlay/focus procedure, route,
transport/adapter/link/MTU, power/charging/brightness, background load and cooldown
policy. Only the saved target bitrate changes. Do not change codec pins or force
another decoder path. If either target cannot negotiate the expected stream or
path, retain the failure and stop; do not quietly substitute another setting.

## Optional controlled setup gate (only with --qualification controlled)

1. Use Windows and Python 3.10+. Check out this PR's updated tooling. Extract the
   previously qualified exact-source portable ARM64 package with `portable.dat`,
   `stage5-package-manifest.json` and `pyrowave/pyrowave-runtime.json`. The qualified
   package source is `00b805e19866155483e6bd27dcd775355087a0ed`; use its verified
   extraction path below. A new app build is unnecessary. Record the package
   download/artifact identity, executable hash and application version. Runner
   rechecks source, architecture, package hashes and existing runtime pins.
2. Prepare one repeatable host workload before benchmarking. The hashed 10-second
   test-pattern loop and exact FFmpeg/ffplay commands in the parent procedure are
   one option. Use the same file, player version/command, fullscreen display and
   deterministic restart/connection phase for every stream. Verify 1440p120 host
   playback/encoding first; retain throughput evidence. Do not generate the file
   during capture or substitute free gameplay. Record host CPU/GPU/OS, host version,
   encoder settings and host display resolution/refresh/HDR/VRR.
3. Choose and record one fixed client display setup: resolution/120 Hz, V-sync,
   SDR/HDR, VRR, window/fullscreen mode, scaling, overlay and focus behavior. Keep
   the same native fragment path as the completed block. Confirm saved settings
   before every launch; the JSON config does **not** apply Asteria settings.
4. Record direct versus tunnel route with stable private endpoint labels, Wi-Fi
   versus Ethernet, adapter/AP/band/channel (if Wi-Fi), negotiated link speed and
   MTU. Keep client/host locations and network traffic fixed. Retain dated route
   and adapter evidence privately; do not put addresses/credentials in public
   manifests. Record RTT/jitter/loss and payload observation method if available.
5. Fix AC versus battery, charging state, Windows power mode and brightness.
   Record background applications/host-client load, updates/downloads, battery
   percent and thermal starting condition per attempt. Use at least 60 s cooldown
   after normal client close, the same restart procedure, and record actual rest
   time. If thermal/load/route conditions drift, stop and annotate; do not discard
   only a poor result. Sensors/clocks/power watts may be unavailable: write null
   plus reason/method in per-run notes. A concrete manual thermal starting rule
   with sensor unavailability stated is acceptable; invented temperatures are not.
6. Fill all setup fields in the 250 Mbps copy below. `hostWorkload` must include
   file/player/version/command, `hostWorkloadSha256` the file's real hash, and
   `workloadStartProtocol` the reset/playback-phase/connection protocol. Describe
   the other text fields concretely (`hdr`/`vrr` can say `off`, never a guess).
   `displayMode` includes scaling/overlay/focus behavior; `hostHardware` includes
   OS; `networkRoute` includes route/AP identity; `thermalCondition` is the fixed
   starting rule. Leave only runner-assigned timestamp/runId/hash fields unset
   as appropriate; prefer filling executableSha256 from Get-FileHash.
7. Clone the completed 250 config to 500 and change only bitrate. Run
   `benchmark-ready --qualification controlled` on both. It rejects missing setup, placeholder strings,
   mismatched identities/context and wrong targets. Live runner repeats the setup
   gate in controlled mode before package verification/launch. Exploratory mode
   requires essential settings and warns about missing extended context instead.
   Historical manifests with explicit
   nulls remain readable; missing historical information is never fabricated.
   This is an owner-declaration check; actual capture/log evidence must still
   agree. Missing required setup means prepare it before benchmarking.

## Exact setup and pilot commands

Run PowerShell from the tooling repository. Use a fresh experiment directory;
the commands refuse reuse. Substitute the actual verified package extraction
path for `$exe`. Close existing Asteria/Moonlight normally.

```powershell
$pilot = 'C:\Benchmarks\Phase1B\bitrate-pilot-v1'
$exe = 'C:\Benchmarks\Packages\phase1a-00b805\Asteria.exe'
if (Test-Path -LiteralPath $pilot) { throw 'Choose a NEW pilot directory; preserve existing evidence' }
New-Item -ItemType Directory -Path $pilot -ErrorAction Stop | Out-Null
Copy-Item .\docs\benchmark-manifest.template.json "$pilot\250.json"
Get-FileHash -LiteralPath $exe -Algorithm SHA256
notepad "$pilot\250.json"
# CONTROLLED ONLY: fill detailed context, set bitrateKbps=250000,
# experimentStage=pilot, actualBackend=NATIVE_VULKAN, actualDecoderPath=fragment.
$m = Get-Content -LiteralPath "$pilot\250.json" -Raw | ConvertFrom-Json
$m.context.bitrateKbps = 500000
$m.notes = 'PILOT 500 Mbps target; matched to 250.json; actual payload recorded separately.'
if (Test-Path -LiteralPath "$pilot\500.json") { throw 'Preserve existing config' }
$m | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath "$pilot\500.json" -Encoding utf8
python .\scripts\performance.py benchmark-ready "$pilot\250.json" "$pilot\500.json" --qualification controlled
if ($LASTEXITCODE) { throw 'Complete/correct setup before any stream' }
```

Before **each** command, manually open Asteria, set/check the requested target
bitrate and all fixed controls, save a settings screenshot, then close it normally.
Reset the host workload as declared. Wait the uniform cooldown before launch.
Do not paste all four commands unattended: each runner requires manual connection,
stable-video confirmation, timed wait, normal disconnect/cleanup and normal close.
`--single-run --pairs 1 --start ON` means exactly one ON stream, not an ON/OFF pair.
The existing paired mode remains the default for other experiments.

```powershell
# A1: saved target 250000 Kbps; ready configs must still match.
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\250.json" --session "$pilot\01-250" --single-run --pairs 1 --start ON --cooldown 60 --qualification controlled
```

Stop on any nonzero exit, preserve files and inspect before retrying. For a failed
or interrupted attempt, close the previous client normally; resume the **same**
command with `--resume`. Keep the old attempt. Do not advance or cherry-pick.
The single-run command records cooldown policy but has no next launch to delay;
the owner must wait between these separate commands.

```powershell
# B1: change ONLY saved target to 500000 Kbps, confirm controls, close, reset host.
Start-Sleep -Seconds 60
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\500.json" --session "$pilot\02-500" --single-run --pairs 1 --start ON --cooldown 60 --qualification controlled
```

```powershell
# B2: keep 500000 Kbps; confirm controls, close, reset host identically.
Start-Sleep -Seconds 60
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\500.json" --session "$pilot\03-500" --single-run --pairs 1 --start ON --cooldown 60 --qualification controlled
```

```powershell
# A2: change ONLY saved target back to 250000 Kbps, confirm, close, reset host.
Start-Sleep -Seconds 60
python .\scripts\benchmark_runner.py --exe $exe --config "$pilot\250.json" --session "$pilot\04-250" --single-run --pairs 1 --start ON --cooldown 60 --qualification controlled
```

Retain original configs unchanged once the first run starts. If a fixed control
must change, stop this pilot, preserve it and prepare a new matched pilot directory.
Do not relabel the completed exploratory block or change a failed manifest to pass.

## Evidence to return and review gate

Return the complete pilot directory including both configs, each session plan,
started/finished manifests, all attempts (including failures), console/portable
logs, original capture JSON and summaries. Include the prior ten-run block's raw
directory and independent report separately if available. Hash raw evidence before
sharing; keep originals private and redact only copies of logs/screenshots.

For A1, B1, B2, A2, keep a timestamped sidecar note with chronological position,
config/package/workload hashes, settings screenshot references, host replay start
and connection phase, host playback/encode throughput, route/link/RTT/jitter/loss,
observed payload Mbps and audio-loss/decoder-queue events, background load, power
mode/AC/charging/battery percent/brightness, cooldown duration and thermal state.
Name each observation method/window and unavailable field with reason. Preserve
host logs and any existing overlay/external evidence in the same way at both rates;
do not install new profilers or add measurement overhead in just one condition.

```powershell
$files = @(Get-ChildItem -LiteralPath $pilot -Recurse -Filter manifest.json | Sort-Object FullName | ForEach-Object FullName)
python .\scripts\performance.py benchmark-validate @files
if ($LASTEXITCODE) { throw 'Inspect original evidence; do not promote failed runs' }
python .\scripts\performance.py benchmark-report @files --output "$pilot\pilot-report.md"
# Preserve original bytes and names; run once, choose a new inventory name if rerun.
Get-ChildItem -LiteralPath $pilot -Recurse -File | Get-FileHash -Algorithm SHA256 | Export-Csv -LiteralPath "$pilot\evidence-sha256.csv" -NoClobber -NoTypeInformation
```

Inspect each capture's requested bitrate, resolution/FPS/V-sync/source/architecture
and actual backend/path, full 60-second duration/no overflow, plus exactly one real
healthy native shutdown (preflight excluded), clean exit and zero timeline/fatal
errors. The runner validates these. OFF runs have no capture histograms; this
all-ON pilot does. Compare lifetime presentationDrops/decodedFrames separately
from capture-window drops and FPS; never mix their numerators/denominators.

Reuse the existing report's separate target groups and individual p50/p95/p99,
counts and tail maxima. It intentionally labels different bitrates as different
contexts and refuses automatic between-context deltas; no new analyzer is needed.
Review A1/B1 and B2/A2 manually with target labels, p99 submission intervals,
drop fractions and host/network/power notes. Report per-run values/ranges and
retain poor pacing/loss bursts. No pooled p99, fabricated OFF percentiles,
significance test, causal profiling benefit or end-to-end latency claim.

Wrong settings/package/path, reconnect/multiple lifetimes, crash, partial/overflowed
capture or missing required evidence are predeclared qualification failures; keep
them and their reasons. High drops, tail spikes or loss alone are **not** exclusions.
If either rate is unsustainable, record that result and stop for diagnosis.
Proceed to a separately planned repeated block only after review confirms matched
controls and complete evidence. Keep `experimentStage=pilot` separate from future
`repeated` manifests; never pool them. **STOP for owner hardware results.**
