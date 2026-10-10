# Phase 1A: streaming performance capture

Status: implementation under validation; Phase 1A is not complete until the
exact implementation passes normal x64/native ARM64 and all regressions.
Phase 1B and hardware performance qualification have not started. No optimization,
renderer architecture change, codec pin/patch, shader change, forced backend,
release, merge or 120 FPS qualification threshold is included.

Audited main base: `0f897c0f591d736371c5a3352b48f3e445810842`.
Immutable reference v0.3.0: `2f29a8963078dadbe4e5031791a6390098e4635d`.
Historical v0.2.0: `42f756e465288157608fe894e3a3dfe800b05344`.
Official releases and historical qualification records remain unchanged.

## Measurement pipeline

```text
first packet (common-c receiveTimeUs)
  -> complete decode unit enqueued (enqueueTimeUs)
  -> VideoDec submit callback -> assembly -> parser/push/output setup
  -> decode API [async GPU submission OR synchronous decode + I420 readback]
  -> publication in replaceable bounded slot
  -> SDL/main renderer pickup -> acquire/record -> vkQueueSubmit
  -> vkQueuePresentKHR returns
  -> [GPU execution / presentation completion / scanout: separate domains]
```

CPU decode submission, native codec GPU execution, WSI call duration and physical
display are distinct. Do not sum overlapping intervals or subtract RTT from an
unknown total. No photon-to-photon claim is possible from this capture.

## Existing measurement inventory and historical audit

Reviewed `pyrowave_decoder.{h,cpp}`, header-only `pyrowave_stats.h` (there is no
stats.cpp), `pyrowave_runtime.{h,cpp}`, `pyrowave_vulkan_live.{h,cpp}`,
`pyrowave_vulkan_presenter.{h,cpp}`, `pyrowave_vulkan_overlays.{h,cpp}`,
`pyrowave_vulkan_probe.{h,cpp}`, `pyrowave_queue.h`, live-slot policy, Session,
FFmpeg and its pacer. Also read NEXT_STEP, VALIDATION, GPU_PRESENTATION,
RELEASE_NOTES_v0.3.0 and Stage 5. Historical source was inspected with
`git show v0.2.0:app/streaming/video/pyrowave_decoder.cpp`; current and historical
stats/runtime definitions were checked against the release source.

| Existing metric | Start -> end | Thread | Clock / unit | Domain and behavior | Both releases? / comparable? | Blind spots |
| --- | --- | --- | --- | --- | --- | --- |
| Reassembly | first packet -> complete DU enqueue | common-c receive, consumed by VideoDec | LiGetMicroseconds / us, overlay ms | network/CPU, elapsed | yes / same scope | excludes lost DUs and packets never delivered |
| Decoder queue wait | enqueue -> callback entry after decoder mutex | VideoDec | common-c monotonic / us | CPU elapsed | yes / same scope | Session lock delay included |
| Assembly | begin transport-fragment assembly -> successful copy | VideoDec | common-c monotonic / us | CPU synchronous | yes / same scope | existing averages divide successful totals by decoded frames |
| Parser/preparation | runtime entry -> narrow decode API start | VideoDec | steady_clock / us | CPU synchronous | yes / conditional | includes clear/parse/push/ready and CPU-output allocation; backend output setup differs |
| Decode API | narrow native call entry -> return | VideoDec | steady_clock / us | CPU; GPU-output call is async, CPU I420 is synchronous | yes / NO across async and readback | may include internal codec context wait; no separate readback duration |
| Queue/age overlay | publication -> dequeue (fallback); publication -> after draw (native) | publication VideoDec, main consumes | common-c (fallback), steady_clock (native) / us | CPU elapsed | yes / NO directly | native existing readyUs is calculated AFTER draw, includes that CPU work |
| Rendering overlay | after stats update -> rendering end, only new frames | main | common-c monotonic / us | CPU, may block in WSI/SDL | yes / conditional | different backend work; redraws excluded, not GPU time |
| Network/presentation drops | frame-number gaps / replaced pending and minimized frames | VideoDec + main | counters / frames | transport inference / presentation queue | yes / conditional | historical label "network jitter" includes presentation drops; cause not known |
| FPS and payload bitrate | counters / previous+active approximately 1-second window | main snapshot | monotonic / FPS, Mbps | client aggregate | yes / same windows only | payload excludes FEC; native redraws excluded |
| RTT/variance | LiGetEstimatedRttInfo snapshot | main | common-c estimate / ms | network estimate | yes / conditional | not one-way latency; unknown omitted |
| Host processing | RTP field supplied on DU | VideoDec | host / 0.1 ms converted to ms | host report | yes / if same host definition | zero is absent; no shared host/client epoch |
| Codec iDWT fragment, compute/iDWT, Dequant | pinned runtime's resolved timestamp intervals | report VideoDec once at 10s, main after drain | codec GPU / native ms per frame text | asynchronous GPU execution | yes / same pin, path and report scope only | cumulative includes warmup; stages may overlap; not total decode/readback |
| Codec memory heap report | native callback snapshot | same report calls | codec device budget units | device/system heap | yes / conditional | NOT Asteria process VRAM or utilization |
| Vulkan submit/present totals | queueOperation entry -> exit | main | steady_clock / ms totals, summary us | CPU/WSI synchronous calls | v0.3 only / no v0.2 scope | existing totals include queue mutex; successful present is not display proof |
| Native lifetime summary | initialization -> teardown counters | main after decode excluded | lifetime counts and us totals | aggregate | v0.3 only | no distributions, no GPU presenter timing |
| Recreate/overlays | resource operation counts | main | counts | CPU/GPU work | v0.3 only | existing counters do not measure CPU duration |
| FFmpeg decoding/pacer | enqueue -> decoded output; pkt_dts -> render; render call | decoder/pacer | common-c / us | CPU-observed standard-codec pipeline | both standard codecs / not PyroWave equivalent | FFmpeg decode includes queueing, unlike narrow PyroWave API time |

Existing overlay, codec reports and lifetime summary remain intact. New captures
reuse existing DecodeTiming durations. GPU-output `decodedFrames` means a
successful submission/publication, not proof of GPU completion.

## Opt-in capture

Local environment configuration avoids normal Settings UI and command parsers.
Launch the development package directly from the configured PowerShell process:

```powershell
New-Item -ItemType Directory -Force C:\Benchmarks\Asteria | Out-Null
$env:ASTERIA_PERF_CAPTURE = 'C:\Benchmarks\Asteria'
$env:ASTERIA_PERF_WARMUP_SECONDS = '10'
$env:ASTERIA_PERF_DURATION_SECONDS = '60'
& .\Asteria.exe
# Connect manually, select PyroWave, stream for >=70 seconds, disconnect.
Remove-Item Env:ASTERIA_PERF_CAPTURE,Env:ASTERIA_PERF_WARMUP_SECONDS,Env:ASTERIA_PERF_DURATION_SECONDS
```

The directory must exist and be absolute. Invalid configuration disables capture
with one warning. Warmup accepts 0..3600 integer seconds; duration 1..86400.
Defaults are 10/60. Measurement starts at successful live decoder initialization;
preflight creates no capture. Warmup excludes frame samples and interval history;
the first admitted interval needs two admitted endpoints. Observation-time
admission may exclude a frame that crosses the duration boundary. Telemetry stops
after duration; streaming continues until normal manual disconnect.

Files are written after decoder teardown and LiStopConnection, by deferred cleanup,
as `asteria-perf-<random UUID>.json` using atomic QSaveFile with direct-write fallback
disabled. Write/allocation failure is nonfatal. Export time is excluded from
cleanup metrics. Do not shut down the process forcibly before cleanup finishes.
Reconnects create new run IDs. A renderer/decoder recreation creates a separate
lifetime capture and resets warmup. At most 16 lifetime reports are retained per
stream; additional reports are omitted and reported by `decoderReportsOmitted`.
Analyze these lifetimes separately; they are not independent repeated runs.

Default mode allocates no histograms and performs only null checks in hooks.
Capture mode uses fixed arrays: 20 histograms of 513 buckets, 512 initial diagnostic
events and 16 codec callback strings of at most 511 bytes. `storageBytes` records
the core allocation. Session's 16 report slots bound deferred export storage.
Atomic 64-bit counters are compile-time required to be lock-free. There is no new
telemetry mutex, background sampler, per-frame disk write or per-frame INFO log.

No credentials, tokens, pairing material, private keys, packets, video, input,
clipboard, hostnames or IPs are exported. Source/codec identity, GPU description,
settings, random run ID and numeric frame IDs are the only diagnostic identity.
Do not paste private raw streaming logs into public benchmark evidence.

## New metric dictionary

All histogram metrics below use **integer microseconds (us)** and exact sample
counts, mean, minimum and maximum. The capture definition strings and clocks are
also exported. `steady_clock` timestamps are never subtracted from common-c or
host timestamps. Fallback pickup uses common-c timestamps on both endpoints;
native pickup uses steady_clock on both endpoints. No absolute clock epochs are
exported; event times are relative to capture start (including warmup).

| JSON name | Measurement start -> end / thread | Comparability |
| --- | --- | --- |
| networkReassembly | DU receiveTimeUs -> enqueueTimeUs / VideoDec observes common-c | matching historical scope |
| frameAssembly | fragment copy start -> success / VideoDec | matching historical scope |
| parserPreparation | reused runtime preparationUs / VideoDec | output setup differs across backends; tool flags it |
| decoderQueueWait | enqueueTimeUs -> submit callback / VideoDec | matching historical scope |
| gpuDecodeSubmission | reused narrow async GPU decodeUs / VideoDec | GPU-output backends only; never equate to readback |
| cpuDecodeReadback | reused narrow synchronous I420 decodeUs / VideoDec | CPU fallback only; readback not separable |
| frameInterarrival | successive delivered DU first-receipt times / VideoDec | aggregate delivered-frame pacing; missing frames not sampled |
| decodePublicationInterval | successive output-ready publications / VideoDec | CPU publication pacing, not GPU completion |
| newFrameSubmissionInterval | native successful queue submission callbacks; fallback after SDL_RenderPresent returns / main | fallback boundary differs, see limitation below |
| renderSchedulingInterval | successive renderFrameOnMainThread entries / main | includes retry and overlay wakes |
| frameAgeAtPickup | publication -> before native draw, or fallback dequeue / main | excludes native draw CPU time; old native overlay differs |
| renderLoop | renderFrameOnMainThread entry -> exit / main | includes stats, overlays, retries and exceptions; old overlay narrower |
| vulkanQueueSubmitCall | actual vkQueueSubmit entry -> return / main | excludes queue mutex; no GPU execution |
| vulkanPresentCall | actual vkQueuePresentKHR entry -> return / main | includes all returned results; no display-completion proof |
| overlayUploadCpu | native overlays.before prepare/staging/upload recording / main | includes checks on unchanged surfaces; not GPU upload execution |
| swapchainRecreation | successful Probe::recreate including existing idle drain / main | rare operation; no new per-frame drain |
| decoderInitialization | initialize entry -> successful end / main | per lifetime; includes provenance and fallback probes; outside warmup |
| decoderCleanup | destructor entry -> resources closed / main | per lifetime; includes existing drain/logs; excludes JSON/export |
| hostProcessing | RTP 0.1 ms * 100 -> us / VideoDec | host scope, absent field omitted |
| estimatedRtt | LiGetEstimatedRttInfo ms * 1000 -> us / main refresh | low-frequency estimator snapshots, not per-frame samples |
| streamSetup | entire Session::initialize / main | one per stream, includes decoder preflight |
| connectionStart | startConnectionAsync, including authenticated launch and LiStartConnection / start QThread | one per stream; host/network+CPU elapsed |
| connectionCleanup | LiStopConnection entry -> return / deferred cleanup QThread | one per stream; excludes decoder resources and optional host quit |

Fallback submission intervals are sampled after SDL_RenderPresent returns and
cannot be directly compared to native post-QueueSubmit intervals. Exported scopes
are specialized by backend so the analysis tool rejects this comparison. New-frame
counts mean new video submissions, independent of retained redraws; they are not
physical display counts. Native draw's accepted submission can include an
OUT_OF_DATE present result; WSI accepted/displayed counts are not inferred.

Counters: receivedFrames, decodedFrames, newVideoSubmissions, networkDrops,
presentationDrops, rejectedFrames, retainedFrameRedraws, overlayOnlyRedraws,
videoPayloadBytes and invalidTimestamps. Overlay-only redraws are a subset of
retained redraws, identified by overlay change/pending work. Malformed rejection
is separate from network or presentation loss. Network gaps use uint32 serial
arithmetic across wrap; backward/repeated IDs do not create huge drop counts.
Transport-specific jitter drops remain null because the existing "jitter"
counter measures presentation replacement, not a reliably attributable cause.

Counts and histograms saturate and flag overflow. Invalid timestamp order is
excluded and counted rather than converted to a fake zero. Unsupported/empty
metrics have zero samples and null statistics. Overflowed summaries are unavailable;
values exceeding signed JSON integer storage become null with overflow status.

Percentiles use nearest rank `ceil(p*n/100)`, estimated by the upper inclusive
bound of eight log2 subdivisions and clamped to observed min/max. Error is upward
and at most 12.5% of integer-us samples; small integers are exact. Storage covers
the full uint64 range. This definition is explicit and versioned. Histograms are
not exported: pooled percentiles across runs are unavailable.

## GPU timing and missing measurements

Pinned codec GPU messages remain unchanged in SDL and are copied into the bounded
`nativeCodecReports.reports` array with their original labels/units. This includes
iDWT fragment, iDWT/compute when actually reported, Dequant and memory heaps.
The reports are cumulative device snapshots, including warmup and possibly frames
after the capture duration. They are not windowed distributions; do not parse
them as per-frame CPU submission measurements or sum overlapping GPU stages.
No messages means unavailable. Truncation is explicit; unknown text is preserved.

Presenter GPU execution is **unavailable** in Phase 1A. Source audit found no
query pool or timestamp dispatch in the presenter. A future optional implementation
could check queue-family timestampValidBits and device timestampPeriod, record
two queries per existing frame resource, and read completed resources after the
existing GetFenceStatus success with availability and without WAIT_BIT. Query reset,
wrap, recreation and teardown need additional mock/device qualification. This
would add dispatch/resources/commands but need not add synchronization. It is
deferred here to preserve the qualified frame resource behavior. See the
[Khronos query specification](https://docs.vulkan.org/spec/latest/chapters/queries.html).

Actual presentation timing may be available via optional
[VK_GOOGLE_display_timing](https://docs.vulkan.org/refpages/latest/refpages/source/VK_GOOGLE_display_timing.html).
[VK_KHR_present_wait](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_present_wait.html)
provides a presentation-completion wait, not a physical scanout or photon timestamp.
Neither is enabled, required or claimed supported on the owner device. No new
display-timing dependency is added. Presenter GPU, displayCompletion,
physicalScanout and endToEndLatency explicitly export unavailable/null.

Driver and systemResources export null until companion-tool measurement.
GetProcessTimes paired snapshots could cheaply estimate process CPU time; normalized
CPU percent needs wall duration and logical processor count. Working set/private
bytes require a clearly defined memory snapshot API; a single endpoint is not a
peak. Windows GPU Engine ETW/performance counters need PID/engine attribution and
care with multiple engines; system-wide GPU percentages are not process usage.
Device heap reports are not process VRAM. Power/thermal/clock support varies by
Qualcomm/NVIDIA drivers; no sensor driver or invasive sampler is installed here.
For Phase 1B use Windows Performance Recorder/Analyzer or a separately approved
PresentMon workflow, recording tool version, PID attribution, sample interval and
overhead. For battery record capacity delta over long, fixed unplugged runs with
matched brightness/power mode; charging makes discharge unsuitable. Mark unavailable
sensor fields unavailable. External tools must receive a separate overhead run.

## Schema and local analysis

Schema version **1**: [performance-capture.schema.json](performance-capture.schema.json).
The Python standard-library validator also checks finite values, integer counts,
null semantics, statistic order, event bounds and version. It rejects NaN/Infinity
and files over 2 MiB. JSON metric keys may be absent for historical imports; unknown
or unavailable measurements never become zeros. Top-level identity distinguishes
`instrumented-development`, stock imported evidence and synthetic fixtures.
Do not label a modified build as stock v0.3.0.

Example: [synthetic.json](../tests/performance/synthetic.json), deterministically
authored and explicitly synthetic, with no GPU/host performance evidence.

```powershell
python scripts/performance.py validate C:\Benchmarks\Asteria\run1.json
python scripts/performance.py summary run1.json run2.json run3.json --output summary.json
python scripts/performance.py report old1.json old2.json new1.json new2.json --output report.md
python -m unittest discover -s tests/performance -p 'test_*.py' -v
cmake -S tests/performance -B build/perf
cmake --build build/perf --config Release
ctest --test-dir build/perf -C Release --output-on-failure
build/perf/Release/perf-overhead.exe > overhead.json
```

Reports include every supplied run, median run means, sample-weighted means,
run-mean min/max and every per-run percentile/count. They never average p99s into
a pooled p99. First input group is the descriptive reference. Unit, clock, scope,
percentile method and profile compatibility are checked. Resolution/FPS/bitrate,
V-sync, architecture, GPU/driver, codec pin and benchmarkContext must match for
qualified comparisons. Missing driver/context makes deltas descriptive only.
Different backend parser setup is flagged. Same runId files are rejected as
independent repeated runs. No host control, input sending or automatic connection.

Optional privacy-safe `benchmarkContext` can be added to copies for analysis:
hostBuild (Vibepollo SHA), workloadId, networkType, displayMode, powerState,
chargingState, tailscale (boolean) and hostWorkload. Include host CPU/GPU/OS and
client OS/driver/brightness/tool versions in the local run manifest. Never include
host address, credentials or pairing files. Wi-Fi interference, Tailscale routing,
host workload and power/thermal state are explicit confounders.

## Validation and overhead

New deterministic core tests cover histogram bounds/percentiles/counts, microsecond
units, ordering, saturation, missing metrics, bounded storage, concurrent writers,
redraw classification and frame-number wrap. Qt's normal x64/ARM64 settings suite
also tests production serializer roundtrip, null/overflow semantics, unique atomic
exports and unwritable/missing directories. Python tests cover schema version,
JSON roundtrip, aggregation, weighted means, missing values and incompatibility.
Existing normal builds/standard-codec regressions, parser/runtime and Stage 2–5
workflows remain required and unchanged in strength. GPU absence is SKIP, not PASS.

`perf-overhead` alternates seven disabled/enabled pairs after warmup, using an
identical CPU-only arithmetic surrogate plus representative hooks. It emits raw
ns/frame pairs. This measures capture-hook overhead, not full live streaming,
driver scheduling, resource sampling or file export. Optimizers may collapse the
arithmetic surrogate; evaluate the paired added cost, not the disabled percentage.
Run the same test on both CI architectures and repeat enabled/disabled full
development streams on Surface before hardware benchmarking. There is no claimed
zero overhead or live performance benefit.

## Surface owner checklist and proposed Phase 1B

Before Phase 1B, verify the exact native ARM64 development package/source identity
and pins; do a functional native PyroWave stream, overlays, resize/minimize/restore,
five reconnects, clean shutdown, H.264/HEVC/AV1 and audio/input checks. Capture
enabled and disabled must both remain stable. Validate JSON and null fields;
verify newVideoSubmissions excludes redraws and bounded event omissions are explicit.
Record any validation/GPU absence as SKIP. Stop on instability, standard-codec
regression, unacceptable overhead or any required pin/shader/synchronization change.

After hardware instrumentation qualification, Phase 1B can use this matrix:

| Client | Required profiles | Optional profiles |
| --- | --- | --- |
| Surface Pro 11 / Snapdragon X Plus / Adreno X1-85 / native Windows ARM64 | 1920x1080@60; 2560x1440@120 | 1920x1080@120; 2560x1440@60 |
| Windows x64 / RTX 4070 Ti | same required profiles | same optional profiles |

For each profile compare unmodified official portable v0.2.0, unmodified official
portable v0.3.0 and explicitly labeled instrumented development. Same pin, host,
Vibepollo build, scripted/replayable host workload, bitrate, display refresh,
V-sync, network route/type, charging, brightness and power mode. Use 10s warmup
and at least 60s measurement, at least five independent runs per condition,
alternate order and record all runs, failures and exclusions with reasons.
Separately repeat profiling off/on with identical development binary/settings.
Stock releases cannot emit new capture schema; transcribe only matching historical
overlay scopes into explicitly labeled local evidence, keep original summaries,
and never manufacture distributions or compare narrow async submission against
synchronous readback. No forced backend switch is implemented.

Record requested/observed FPS and bitrate, drops, RTT and host processing separately.
Physical latency requires a separately designed hardware measurement experiment.
Long thermal/power/soak tests are separate from short timing repeats. Do not cherry
pick best runs, demand 120 FPS or claim matched results from dissimilar workloads.
