# Phase 1B: controlled benchmarking and bottleneck analysis

Status: **Step A preparation; Phase 1B INCOMPLETE**. No new Surface measurements
exist in this change. Step B begins only after the owner provides repeated real
captures/logs. A valid conclusion can be that no optimization is justified.
This PR changes local tooling, tests and documentation, not the streaming runtime.

## Verified identities and evidence boundaries

PR #45 merged at `a3e6d4604e1a02e5b86689f032a88cc08c155e03`.
GitHub and fetched `main` agreed on that SHA at preparation; it is this branch's base.
Phase 1A owner-qualified implementation: `00b805e19866155483e6bd27dcd775355087a0ed`.
Its corrected native metadata is owner-verified on Surface Pro 11 / Snapdragon X
Plus / Qualcomm Adreno X1-85 / native ARM64, 2560x1440@120 target:

| Field | Verified exported value |
| --- | --- |
| backend | NATIVE_VULKAN |
| newFrameSubmissionInterval.scope | CPU:successive_new_video_vkQueueSubmit_success_callbacks |
| newFrameSubmissionInterval.clock | steady_clock |
| frameAgeAtPickup.clock | steady_clock |

Owner-provided evidence names: `asteria-perf-56cc4f59-0f7e-4c97-b964-8d36f60baac6.json`
and `Asteria-1791760716.log`. The owner verified sourceRevision `00b805e...`.
The supplied native summary records decoded=10454, rendered=10380,
presentationDrops=nativeRetiredDrops=74, cpuYuvReadbackFrames=0, timelineErrors=0,
fatalPresenterErrors=0, cleanupOkay=true. Raw files were not independently reread
during this preparation. Functional overlays, window transitions, reconnect,
audio/input and standard-codec smoke are separately **OWNER-CONFIRMED**; no exact
reconnect count or additional counters are asserted. Vulkan validation remains SKIP.
See [PR #45](https://github.com/Unitron07/Asteria-Windows/pull/45).

Keep these evidence groups separate: historical Stage 5 qualification; official
v0.2.0; official v0.3.0; initial Phase 1A; metadata-fixed Phase 1A; controlled
Phase 1B. Never overwrite originals or equate old screenshots with a new JSON window.
Historical Stage 2–5 and release records remain in [VALIDATION.md](VALIDATION.md),
[GPU presentation](PYROWAVE_GPU_PRESENTATION.md) and [STAGE5.md](../tests/pyrowave/vulkan/STAGE5.md).

## Matrix and identities

| Condition | Exact source / binary | Expected path, requiring actual verification | Evidence |
| --- | --- | --- | --- |
| A | Official v0.2.0 `42f756e465288157608fe894e3a3dfe800b05344` | CPU-I420 fallback, often compute on Surface | Common logs/overlay or external method; no new capture JSON |
| B | Official v0.3.0 `2f29a8963078dadbe4e5031791a6390098e4635d` | Native Vulkan, often fragment | Common logs/overlay or external method; no new capture JSON |
| C | Qualified development `00b805e19866155483e6bd27dcd775355087a0ed` | Actual backend/path from logs and capture | Profiling ON |
| D | Exact same C package, verified SHA-256 | Actual backend/path from logs | Profiling OFF; no histogram percentiles |

Historical releases stay immutable. Never use a modified executable as an official
baseline. This tooling needs no newly instrumented build: reuse the qualified C/D
package. CI may build this tooling PR's source too; if using it, update the config's
source to its exact package SHA and label it instrumented development separately.
Do not combine package revisions in one ON/OFF experiment. Manifest SHA/hashes are
provenance checks, not signatures; download from the verified GitHub Actions run.

Primary Surface profiles: **1080p60** (1920x1080@60) and **1440p120**
(2560x1440@120), PyroWave SDR 8-bit 4:2:0. Optional 1080p120/1440p60 and x64
RTX 4070 Ti follow only after core evidence. Manual target bitrate proposed for
1440p120 is 500000 Kbps; choose/record an explicit fixed 1080p60 bitrate before
its run. Do not interpret the template target as measured payload or capacity.
Actual host version, workload, codec/path, bitrate, V-sync, display refresh,
route, power, brightness and driver must be recorded. Requested backend is no proof.
Compute versus fragment is a codec-path confounder. Synchronous CPU decode/readback
and async GPU submission are not equivalent stage timings.

## Repeatable host workload proposal

The repository has deterministic offline codec/presenter fixtures, but no fixed
live host workload. Proposed first baseline: generate a 10-second lossless moving
test pattern once per profile, then play the same hashed file full-screen in a
loop on the host. This avoids variable gameplay/input. First verify host playback
and encoding sustain the requested rate; otherwise document host limitation and
choose a lighter fixed replay before full repeats. Do not compare different scenes.

On the **host**, using a recorded FFmpeg/ffplay version (not installed by the runner):

```powershell
ffmpeg -version
ffmpeg -n -f lavfi -i 'testsrc2=size=2560x1440:rate=120:duration=10' -an -c:v ffv1 -pix_fmt yuv420p phase1b-1440p120.mkv
Get-FileHash .\phase1b-1440p120.mkv -Algorithm SHA256
ffplay -fs -loop 0 -an .\phase1b-1440p120.mkv
```

For 1080p60 change size/rate and filename. `-n` prevents overwriting. Record
file hash, FFmpeg/player version, command, host CPU/GPU/OS, monitor refresh,
playback/encoder throughput and playback phase at connection. Lossless decoding
can itself load the host; it is a controlled synthetic workload, not representative
of every game. Ten seconds divides the 60-second measurement window, reducing
phase bias; verify repeatability first. Generate before experiments, never during
a measured stream. See official [testsrc2 filters](https://ffmpeg.org/ffmpeg-filters.html#testsrc_002c-testsrc2)
and [ffplay loop/fullscreen options](https://ffmpeg.org/ffplay.html).

## Design, order and predetermined exclusions

Use 10-second warmup and 60-second capture, five independent streams per condition.
Runner's five-pair order: **C D D C C D D C C D**. Use `--start OFF` to reverse it
in subsequent experiments; for manual A/B use A B B A A B B A A B,
then B A A B B A A B B A in a subsequent block. Keep all runs including poor ones.
First pass: one C/D pair at 1080p60, inspect manifests/backend/export, then one
at 1440p120. Correct configuration errors before the complete five-pair sessions.
Pilot runs remain marked pilot in notes and are not pooled into the planned repeats.

Exclusions determined beforehand: wrong package/profile/path, changed workload,
disconnect/crash, decoder recreation/multiple lifetimes, incomplete capture,
counter overflow or missing provenance. Record reason/status, keep raw files and
retry under a new attempt directory. Bad pacing, drops and loss bursts alone are
not exclusions. Runner preserves failed attempts and stops for inspection.
Use at least 60 seconds between attempts; extend uniformly if heat is accumulating.
Record cooldown/thermal availability. No optimization or 120 FPS pass threshold.

## Owner runner: exact commands and interventions

Requires Windows and Python 3.10+ standard library. Run from the checked-out tooling
repository. Use the normal portable ARM64 package, whose extracted executable is
`Asteria.exe` with `portable.dat`, `stage5-package-manifest.json` and
`pyrowave/pyrowave-runtime.json`. Normal CI artifacts are named
`Asteria-Windows-arm64-<run-id>`; extract the artifact's inner portable ZIP too.
The package verifier checks all manifest file hashes and fixed runtime pins.

Close existing Asteria/Moonlight normally. Pair host and configure PyroWave,
SDR/8-bit/4:2:0, resolution/FPS/bitrate/V-sync in Asteria before the experiment.
No pairing credentials or host address go in the public manifest. Prepare the
host workload and verify display/power/network conditions manually.

```powershell
# Do not overwrite an existing configuration or experiment.
New-Item -ItemType Directory C:\Benchmarks\Phase1B
Copy-Item .\docs\benchmark-manifest.template.json C:\Benchmarks\Phase1B\surface-1440p120.json
notepad C:\Benchmarks\Phase1B\surface-1440p120.json
# Fill known null fields; leave unavailable values null. Confirm exact GPU spelling
# from the qualified log. Keep sourceRevision equal to the selected package.
python .\scripts\performance.py benchmark-validate C:\Benchmarks\Phase1B\surface-1440p120.json

# One command begins all FIVE ON/OFF pairs (ten fresh application launches):
python .\scripts\benchmark_runner.py --exe C:\Benchmarks\Packages\phase1a-00b805\Asteria.exe --config C:\Benchmarks\Phase1B\surface-1440p120.json --session C:\Benchmarks\Phase1B\surface-1440p120-five-pairs --pairs 5 --cooldown 60
```

The example package path is an owner-selected extraction location, not an installed
path assumption. For 1080p60 copy the template to a new config, set profile,
width=1920/height=1080/targetFps=60 and the chosen bitrate, then use a new session.
Use `--pairs 1` and a separate pilot session for the first-pass check.

**Owner interventions per attempt:** confirm settings/workload; Enter to launch;
connect exactly one stream; when video is stable Enter in the runner; leave the
workload running during the timer; at the prompt disconnect normally, wait for
cleanup, close Asteria normally, then Enter. No coordinates/input automation.
The runner does not kill Asteria. Terminal focus changes are a scheduling/input
confounder: use the same sequence in ON and OFF and preserve focus-related settings.
Turning the timer confirmation into a stream initialization timestamp would be
incorrect: ON's exact window begins at production decoder initialization. The
runner waits warmup+measurement+2s after owner confirmation to ensure the ON
capture completes, records confirmation/disconnect timestamps, and validates duration.
OFF has no equivalent histograms. Native common counters cover **decoder lifetime**,
including warmup and manual timing delays. Their fractions can be compared
descriptively with the same lifecycle protocol; they are not fixed-window 60s counters.
Fixed-window OFF resource/FPS measurements require a separately documented common
external method; unavailable values remain null.

After interruption close the previous client normally; then rerun the identical
command with `--resume`. Exact config, executable hash, run order and cooldown
must match. Completed manifests/captures are validated and skipped; interrupted
ordinals get new UUID attempt directories. Existing results are never overwritten.
An exclusive `runner.lock` prevents concurrent writers. After an abrupt runner
crash, verify no prior runner/client is running before manually deleting ONLY that
session's stale lock. A resumable failed run is not silently treated as completed.
Copying raw logs may include private addresses; redact a copy before public sharing.

### Native log selection and offline pilot recovery

The runner records its redirected **console.log** as first-class manifest evidence,
as well as changed portable `Asteria-*.log` files. The runner and analyzer share one
selector: ignore explicit `testOnly=true` preflight summaries, then require exactly
one real `testOnly=false` native shutdown summary. Identical parsed JSON copied
across different log files counts once, regardless of prefix/key order. Two real
records within one file still count twice even if identical; distinct real summaries
in different files also fail. Missing/malformed testOnly or malformed JSON fails.
Source SHA, GPU, bitstream, native backend, actual decoder path, cleanup=true,
explicit zero timeline/fatal errors and integer counters remain validated. The
original lines, capture values and counter definitions are never rewritten.

An owner Surface pilot exposed two defects in the original tooling head
`b11ed44735a270a907f21a1456a68e785b38bb66`: console.log was omitted and preflight
was counted as another lifetime. The owner reports one preflight and one real
summary in `run-01-ON-5511ce24/console.log`, with one capture named
`asteria-perf-8179fbe6-e203-4bb6-af11-c3f5966f2115.json`. Those raw files were
not available on the development computer; **this is a reported reproduction,
not an independently revalidated hardware run**. Recovery is possible only if
the actual evidence passes validation; no new stream is needed for that check.

Close Asteria normally first. Using the refreshed toolkit on the Surface, replace
the example session path with the actual pilot location:

```powershell
python .\scripts\benchmark_runner.py --revalidate C:\Benchmarks\Phase1B\pilot\run-01-ON-5511ce24 --recovery-output C:\Benchmarks\Phase1B\pilot-revalidated
python .\scripts\performance.py benchmark-validate C:\Benchmarks\Phase1B\pilot-revalidated\manifest.json
```

This offline command launches no application and changes no original file. It is
restricted to an original failed ON manifest with the exact old log-selection
error. It requires original exitCode=0, ordered owner timing records, the same
declared capture inventory, exactly one full-duration non-overflowed valid capture,
correct native scope/clock metadata and one healthy real summary matching capture
and manifest identities/settings. Missing evidence or another failure refuses
automatic recovery; do not edit values or fabricate timing/exit evidence to pass.

Only after validation, it exclusively creates a **new standalone directory**
outside the original session, copies the original log/capture bytes, retains the
failed manifest verbatim as `original-manifest.json`, and writes `manifest.json`,
`revalidation.json` and `summary.md`. It hashes every copied source file, revalidates
the copy, and detects changes during validation/copy. Existing output is refused;
an interrupted copy remains separate and must not be treated as completed.
`revalidation` records the original failure and hashes. No original finishedAt,
measurement values, settings, runId or ordinal is invented or replaced.

The original manifest remains **failed**, and its session plan is not advanced.
The recovered copy is the **same pilot**, not another independent repeat. Analyze
either the original failed attempt or its validated copy, never both as repeats.
Normal `--resume` of the original session retries its failed ON ordinal; it does
not silently adopt a recovered pilot. New valid ON attempts proceed to OFF under
the corrected runner. Retain pilot evidence separately from planned full repeats.

### Unattended interface audit

`app/cli/commandlineparser.cpp` documents `stream <host> <app>` and `quit <host>`.
The former needs an already-paired host; its codec choices exclude PyroWave, which
must come from saved settings. `quit` terminates the running **host application**;
`--quit-after` controls host-app termination after a session, not a timed local
disconnect. No documented timed local stop/readiness protocol exists in this
Windows build. Full unattended control is therefore not implemented. Runner
intentionally launches normal UI and uses owner prompts. It neither changes
system configuration nor sends host input. Historical releases are rejected as
runner targets; run A/B manually with the same protocol, preserving logs separately.

## Analysis and measurement inventory

[PERFORMANCE_BENCHMARKING.md](PERFORMANCE_BENCHMARKING.md) retains the complete
source-audited metric dictionary: networkReassembly, frameAssembly,
parserPreparation, decoderQueueWait, gpuDecodeSubmission, cpuDecodeReadback,
frameInterarrival, decodePublicationInterval, newFrameSubmissionInterval,
renderSchedulingInterval, frameAgeAtPickup, renderLoop, vulkanQueueSubmitCall,
vulkanPresentCall, overlayUploadCpu, swapchainRecreation, decoderInitialization,
decoderCleanup, hostProcessing, estimatedRtt plus streamSetup, connectionStart,
connectionCleanup. Read scope/unit/clock before comparing any stage.

Captures retain schemaVersion 1. Companion manifest version 1 is separate; template
lists every required field and explicit nulls. Existing validator/report/summary
commands remain compatible. New commands reuse the production capture validator
and aggregation; no second profiler, host connection or heavy dependency is added.

```powershell
python -m unittest discover -s tests/performance -p 'test_*.py' -v
$files = @(Get-ChildItem C:\Benchmarks\Phase1B\surface-1440p120-five-pairs -Recurse -Filter manifest.json | Sort-Object FullName | ForEach-Object FullName)
python .\scripts\performance.py benchmark-validate @files
python .\scripts\performance.py benchmark-report @files --output C:\Benchmarks\Phase1B\surface-1440p120-report.md
```

Runner writes a new UUID summary after each completed attempt. Report retains
all supplied attempts and statuses, checks capture SHA/backend/decoder/profile,
rejects reused run IDs, separates evidence/source/path/profile groups and flags
unknown conditions. ON/OFF requires identical executable hash and source.
Across repeated completed runs report sample count, median, min/max, sample standard
deviation and median absolute deviation. Outliers (>3 scaled MAD when MAD>0) are
flagged and retained, never automatically excluded. No significance test or causal
claim is inferred from five samples. Every per-run p50/p95/p99 remains separate;
no pooled p99 is fabricated. Charts are deferred until actual measured data arrive.

Full-window integer-us histograms have exact counts/means/min/max unless overflow;
nearest-rank percentiles round upward to log2/8 bin bounds (up to 12.5% error).
The 512-event trace is a bounded prefix in cross-thread reservation order; sort
relativeUs for diagnostics, never infer full-window gap/burst distributions from it.
Histogram maximum/tails identify gaps; interval histograms lack sequence needed
to reconstruct all bursts. Count received, decoded, new video submissions and
retained/overlay redraws separately. Capture FPS=count/window; no scanout inference.
presentationDrops/decodedFrames uses one common scope/window; never mix a 60s
capture numerator with lifetime denominator. nativeRetiredDrops is retirement
accounting, not another drop source. Legacy/CPU pickup uses common-c monotonic;
native pickup steady_clock. Native post-successful-submit and SDL RenderPresent
return intervals have different boundaries, even though both use steady_clock.

Native codec iDWT fragment/compute and Dequant reports remain original text and
units; cumulative GPU scopes include warmup and can overlap. Compare only same
pin/path/workload/report period; never equate async CPU API duration with GPU
execution or synchronous CPU-I420 readback. No timestamp queries or waits are added.

## Network and external resource evidence

Record transport (Wi-Fi/Ethernet), link speed, direct/Tailscale route, MTU, RTT,
jitter, explicit lost packet/frame bursts, target bitrate, observed payload Mbps,
audio loss episodes and local decoder queue overflows. Use companion notes or
common external measurements in both conditions; unknown fields remain null.
`networkDrops` infers frame-number gaps, which may include local callback queue
discard; it cannot classify all gaps as transport loss. Presentation replacement
is a separate queue phenomenon. At ~500 Mbps payload, FEC/protocol overhead raises
wire demand above payload; Wi-Fi advertised link speed is not sustained throughput.
If route capacity/loss varies, report the network limitation before blaming Vulkan.

Proposed external measurements (separate optional runs; not automatically enabled):
process CPU from paired GetProcessTimes / wall duration / logical CPU count; memory
working-set snapshots with PID and interval, not a single endpoint called peak;
PerfMon GPU Engine with PID/engine attribution, recording available counters and
tool overhead. Microsoft documents that Task Manager GPU data needs WDDM 2.0+
and applies to Vulkan too: [GPU data methodology](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/).
Availability and correct PID attribution on **Adreno X1-85 are UNVERIFIED**;
enumerate counters on that driver before recording values. Do not add engine
percentages into an unexplained utilization number. Shared integrated memory is
not dedicated process VRAM; [Microsoft documents GPU process-memory counter limitations](https://learn.microsoft.com/en-us/troubleshoot/windows-client/performance/gpu-process-memory-counters-report-wrong-value).
WPR/WPA or [GPUView](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/using-gpuview)
can investigate scheduling in separately approved diagnostic runs with overhead
controls. Do not install a sensor driver or privileged component into Asteria.
Battery discharge needs long fixed unplugged runs, matched brightness and capacity
delta; plugged-in measurements cannot establish discharge efficiency. Power,
thermal sensors and GPU clocks are unavailable until reliable device support and
common measurement scope are verified. Detailed power/thermal optimization is Phase 4.

## Preliminary historical clues and remaining limits

Owner observed ON 113/24154 drops (~0.468%) and OFF 423/20346 (~2.079%, ~4.44x).
OFF had more explicit packet loss/larger bursts; smoother ON is **not causal evidence**.
Historical parser ~2.30ms, async API ~0.52ms, submit ~0.046ms and present ~0.213ms
are clues from different conditions, not proven bottlenecks. Do not sum their
averages as total latency. Scheduling, queue waits, publication/submission and
replacement are hypotheses; no priorities or synchronization changes are authorized.

GPU presenter execution, physical scanout, end-to-end latency, system power,
thermal state and GPU utilization remain unavailable. Profiling-on/off causal
overhead is unestablished. Histogram tails are approximate and traces bounded.
Network/host/client/display domains stay separate. No physical latency reduction,
hardware power efficiency improvement or profiling benefit is claimed.

## Results and bottleneck ranking: waiting for owner

| Profile / conditions | Completed controlled repeats | Qualification / finding |
| --- | --- | --- |
| 1080p60 A/B/C/D | unavailable | awaiting owner measurements |
| 1440p120 A/B/C/D | unavailable | awaiting owner measurements |
| Same-binary C/D | unavailable | causal overhead not established |

After owner upload: validate each original, verify source/path/profile/window,
inspect full repeat variation and exclusions, retain common-method resource and
network evidence, then produce tables/plots of measured values. For each candidate
record supporting metric/scope, severity, confidence, likely cause, expected
benefit with uncertainty, risk and next discriminating experiment. Classify CPU
assembly/parser as synchronous VideoDec work, async decode API as potentially
overlapped with GPU, queue wait as scheduling/backpressure, pickup/render/WSI as
main-thread CPU calls, and assembly/RTT/hostProcessing as separate network/host
domains. Stage averages alone do not prove critical-path occupancy; look for
repeatable tails, load dependence and correlated loss/waits, then test a controlled
hypothesis. Rank pacing/display investigations for Phase 2 and PyroWave work for
Phase 3 only when supported. No forced winner, arbitrary speedup threshold,
renderer/codec optimization, MultiSeat, VR, release or automatic merge.

Return each session directory (plan, started/finished manifests, logs, original
JSON and summaries), configuration, host workload hash/version and host/external
notes. Raw logs stay private until redacted. **STOP after Step A CI; owner results
are required for Step B and Phase 1B completion.**
