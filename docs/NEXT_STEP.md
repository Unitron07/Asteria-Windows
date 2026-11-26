# Next step: native PyroWave retry scheduling, then a v0.4.0 preview

**Active phase: controlled performance qualification and v1.0 polish** (updated
2026-11-26). Phase 1A instrumentation merged in #45; Phase 1B benchmarking stays
paused at the owner's request. Phase 2 implementation is active: the native
PyroWave demand gate (#50) and the cross-codec audit (#51) are merged. The next
planned change is native retry scheduling ([Phase 2 step 3](PERFORMANCE_PHASE2.md#implementation-order)):
the fixed 16 ms retry timer exceeds a 120 FPS frame period (8.33 ms).

## Current state since v0.3.0

These changes are on `main` but not yet in a release; v0.3.0 remains the latest
published build and the benchmark reference.

| Change | PR | Status |
| --- | --- | --- |
| common-c `94c65fd` with rebased P1a patch and MSVC C11 atomics | #71 | Merged |
| PyroWave automatic bitrate capped at 800 Mbps, "no video yet" hint, typed bitrate entry, portable log path in the bug template | #72 | Merged (addresses #64) |
| Moonlight upstream `dc99771b` merged; baseline recorded | #73 | Merged |
| Upstream sync: optional `UPSTREAM_SYNC_TOKEN`, rejected pushes reported | #74 | Merged |
| Pacer aggregate surface budget (9), rebased onto upstream's Pacer fixes | #58 | Draft: CI and owner smoke |

### Open items and blockers

- **common-c `b4ee1ce` (#75) collides with PyroWave.** Upstream added MPEG-2 and
  H.263+ using PyroWave's bits: `SCM_MPEG2 = 0x00800000 = SCM_PYROWAVE`,
  `SCM_H263P = 0x02000000 = SCM_PYROWAVE_HDR10` and
  `VIDEO_FORMAT_MPEG2 = 0x00010000 = VIDEO_FORMAT_PYROWAVE`. The P1a patch also no
  longer applies. The SCM bits are host-advertised, so new PyroWave bits must be
  agreed with the Vibepollo host before any further common-c update. Do not merge #75.
- **Prebuilt dependencies:** Asteria builds from the hash-pinned v19 archives in
  `scripts/baseline-deps.json`; upstream's setup scripts now name v21. Moving to v21
  needs a separate change with verified hashes.
- **#64:** the automatic-bitrate part is fixed in #72. A same-bitrate AV1
  comparison (Asteria vs upstream at 120 Mbps) is still to be requested.

### Planned order

1. Native PyroWave retry scheduling, with bounded retry/coalescing tests.
2. One matched Surface measurement using the Phase 1A capture: native PyroWave and
   AV1 at the same resolution, FPS and bitrate, before and after step 1.
3. v0.4.0 unsigned portable preview with the same exact-source x64/ARM64
   qualification used for v0.3.0.
4. v1.0 UI/QoL and regression polish, and a separate PyroWave graduation decision.

P1b transport hardening, v2 MultiSeat and v3 VR follow; see the
[roadmap](PORTING_PLAN.md#host-dependent-roadmap-boundaries).

## Latest release: v0.3.0

**[v0.3.0 — Native Vulkan Release is published](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.3.0)**
from `2f29a8963078dadbe4e5031791a6390098e4635d`, with exact-source x64/native ARM64 packages,
verified public downloads and final ARM64 owner-confirmed functional smoke.
See the [release record](VALIDATION.md#asteria-v030-release-record) and
[release notes](RELEASE_NOTES_v0.3.0.md). Stage 5 is complete; PyroWave remains
Experimental. Controlled performance qualification and v1.0 polish are the next
development phase described below. This documentation update starts no tuning,
v1.0 implementation, MultiSeat or VR work.

[Stage 3 offline shared-device proof](../tests/pyrowave/vulkan/STAGE3.md) now
distinguishes exact identity from approved numerical equivalence: every decoded
Y/U/V byte must be within ±1 of CPU decode; a single ±2 fails. Exact hashes and
all diagnostic metrics remain recorded. At diagnostic head f08ecabb, the owner
found deterministic ±1 variants in BOTH AUTO fragment and forced compute on
Surface X1-85, using identical fixtures and the same caller device. Repeats were
stable, device idle did not change bytes, and external handles/D3D11 stayed zero.
Pinned upstream validation permits bounded reconstruction error; no exact
Qualcomm arithmetic root cause is claimed. The separately recorded same-pin
factory patch deletes the wrapper on either initialization failure, with source
ownership tests and public fault diagnostics required for verification.

Stage 3 is complete. Exact-head Surface Pro 11 / Snapdragon X Plus / Adreno X1-85
ARM64 owner qualification passed with `PASS_NUMERIC_EQUIVALENCE_OWNER_RUN` on
`a3cf6b0c59db9465d01178900cd954390728363f`. [PR #36](https://github.com/Unitron07/Asteria-Windows/pull/36)
merged via `06ea4adb745432842af81b73549f74c696fa5a39`. AUTO/fragment and
FORCE_COMPUTE/compute both passed the per-byte ±1 contract; factory cleanup was
`PATCHED_AND_VERIFIED`. Vulkan validation remained `SKIP` because the layer was
unavailable. The precise arithmetic cause of the ±1 differences remains unproven.

**Stage 4 is COMPLETE and qualified.** The exact implementation head
`7c6bf8e085ffe26fd36d7d72d7405b0ed5aa2e85` was merged through [PR #39](https://github.com/Unitron07/Asteria-Windows/pull/39)
with `PASS_OWNER_CONFIRMED` on Surface Pro 11 / Snapdragon X Plus / Qualcomm
Adreno X1-85 native ARM64. It proved PyroWave Vulkan decode into caller-owned
GPU Y/U/V, explicit BT.709 FULL/LIMITED conversion, CENTER chroma, correct
aspect fit, stable swapchain recreation, bounded slot reuse and clean visible
teardown. CPU YUV readback, D3D11 resources and external handles were absent from
the presentation path. Validation was `SKIP` because the layer was unavailable.
See [STAGE4.md](../tests/pyrowave/vulkan/STAGE4.md) for the final evidence.

## Historical state: v0.2.0 released

Live PyroWave SDR 8-bit 4:2:0 has been validated against Vibepollo on native
Surface Pro 11 / Snapdragon X Plus / Qualcomm Adreno X1-85 ARM64. It remains
explicitly selected and **Experimental**; Automatic chooses standard codecs.
Normal Windows x64 and ARM64 builds include the pinned API 0.6.0 runtime,
bitstream `186f0393`, restricted loading and provenance metadata.
Codec-aware bitrate QoL is integrated: standard codecs have a 500 Mbps UI ceiling,
PyroWave a 3000 Mbps ceiling, and PyroWave automatic bitrate is approximately
`width * height * fps * 1.6` bits/s, capped at 800 Mbps (#64); higher values
remain available manually. Manual overrides survive resolution/FPS changes.
The codec selector remains in Basic Settings.

The historical v0.2.0 Qualcomm path rejects Vulkan/D3D11 shared-fence import with
`PYROWAVE_ERROR_UNSUPPORTED_EXTERNAL_HANDLE`. Fragment decode is preferred for
the GPU interop probe; Asteria safely recreates the decoder for compute-path
decode with CPU I420 readback/presentation. This working fallback does not imply
that Adreno cannot decode PyroWave. See [current owner evidence](VALIDATION.md#current-live-arm64-owner-result).
Broader hardware, GPU interop and performance qualification remain open.

Moonlight PC v6.2.0 is the upstream baseline; weekly upstream/master proposals
preserve history and require human review. See [UPSTREAM_SYNC.md](UPSTREAM_SYNC.md).
The historical release is [Asteria v0.2.0](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.2.0), an unsigned Windows
x64/native ARM64 portable release. Its binaries and corresponding source are
from `42f756e465288157608fe894e3a3dfe800b05344`; subsequent release-status
updates are documentation-only. v0.1.0 evidence below remains historical.


v0.2.0 is published with same-commit Windows x64/native ARM64 qualification and
PyroWave CI. See the [release record](VALIDATION.md#asteria-v020-release-record)
for commit, run IDs, checksums and hardware-validation limits. Future releases
repeat their own qualification; this evidence is specific to v0.2.0.

## Stage 5 COMPLETE / OWNER-CONFIRMED

The qualified implementation `5c7ec593d9c5c27b097d194ea758b48b09a2bba4`
merged through [PR #41](https://github.com/Unitron07/Asteria-Windows/pull/41)
at `beb64659fd9217f95beaa2f92547b4933518ecdf`.
The first Surface live run is log-verified; five reconnects in the same process,
window transitions, functional overlay-only updates, standard-codec streaming
and audio/input across reconnects are separately OWNER-CONFIRMED. The first log
records `overlayRedraws=0`; no additional machine counters are asserted.
Validation remains **SKIP**. See the
[evidence and limits](../tests/pyrowave/vulkan/STAGE5.md#stage-5-owner-qualification-record)
and [retained owner procedure](../tests/pyrowave/vulkan/STAGE5-OWNER-TEST.md).
Stage 2/3/4 historical qualification remains unchanged. Native success did not
exercise legacy GPU/CPU-I420 fallback or prove physical device-loss recovery.

## Performance qualification and v1.0 polish: principles

The concrete order is under "Planned order" above. These rules still apply:

- Repeat matched native-versus-old-fallback benchmarks on Surface Pro 11 with
  the same resolution, frame rate, bitrate and workload; record which fallback
  actually runs and preserve standard codecs/Moonlight as regression baselines.
- Separate CPU decode API/async submission time from native GPU iDWT/Dequant
  execution and final scanout/end-to-end latency. The Stage 5 screenshot and
  shutdown timings are samples, not a matched benchmark or latency proof.
- Measure frame queue delay, present duration, CPU utilization, power, drops,
  p95/p99 stability and long-run resource behavior with repeatable conditions.
- Address only bottlenecks demonstrated by those measurements.
- Complete UI/QoL polish and regression qualification before v1.0.
- Keep PyroWave **Experimental** until a separate graduation decision. No v1.0
  tag or release is authorized by Stage 5 completion.

P1b live records/FEC/partial recovery and bandwidth probing remain later work.
v2 MultiSeat and v3 VR are future milestones, outside this task. Both need a
separate Asteria-oriented Vibepollo fork/host extension; VR does not require
PyroWave. See [the roadmap](PORTING_PLAN.md#host-dependent-roadmap-boundaries).
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
framing passed all five patterns through Direct3D11 SDL IYUV presentation:
1920x1080 SDR 8-bit 4:2:0, BT.709 limited range. Aspect fit, nearest/linear scaling,
resize, fullscreen/maximize/restore and renderer/texture recreation were visually
clean. Ten lifecycle cycles, target/device-reset recovery paths and synthetic
renderer/decoder/runtime recreation passed; 30-second 60 FPS loops passed pacing
sanity checks. See [the owner hardware record](VALIDATION.md#m1b-p05-offline-sdl-qualification)
and [reproduction commands](../tests/pyrowave/README.md#p05-offline-sdl-presentation).

No visible chroma anomaly was observed, but authored CENTER versus SDL3 LEFT
metadata sampling remains formally unqualified. No forced physical GPU loss was
induced; true device-loss qualification remains manual/unproven. Loop timing does
not qualify refresh accuracy, full frame pacing, production or end-to-end latency,
or leak freedom. 4:4:4 and HDR remain unqualified. This offline milestone does not
validate live RTP/UDP, SCM/RTSP/SDP, Vibepollo host interoperability, adaptive FEC,
partial-frame recovery or bandwidth probing. Normal application/release behavior
is unchanged.
