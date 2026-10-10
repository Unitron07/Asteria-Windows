# Next step: controlled native Vulkan performance qualification and v1.0 polish

[Stage 3 offline shared-device proof](../tests/pyrowave/vulkan/STAGE3.md) now
distinguishes exact identity from approved numerical equivalence: every decoded
Y/U/V byte must be within Â±1 of CPU decode; a single Â±2 fails. Exact hashes and
all diagnostic metrics remain recorded. At diagnostic head f08ecabb, the owner
found deterministic Â±1 variants in BOTH AUTO fragment and forced compute on
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
FORCE_COMPUTE/compute both passed the per-byte Â±1 contract; factory cleanup was
`PATCHED_AND_VERIFIED`. Vulkan validation remained `SKIP` because the layer was
unavailable. The precise arithmetic cause of the Â±1 differences remains unproven.

**Stage 4 is COMPLETE and qualified.** The exact implementation head
`7c6bf8e085ffe26fd36d7d72d7405b0ed5aa2e85` was merged through [PR #39](https://github.com/Unitron07/Asteria-Windows/pull/39)
with `PASS_OWNER_CONFIRMED` on Surface Pro 11 / Snapdragon X Plus / Qualcomm
Adreno X1-85 native ARM64. It proved PyroWave Vulkan decode into caller-owned
GPU Y/U/V, explicit BT.709 FULL/LIMITED conversion, CENTER chroma, correct
aspect fit, stable swapchain recreation, bounded slot reuse and clean visible
teardown. CPU YUV readback, D3D11 resources and external handles were absent from
the presentation path. Validation was `SKIP` because the layer was unavailable.
See [STAGE4.md](../tests/pyrowave/vulkan/STAGE4.md) for the final evidence.

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

The historical v0.2.0 Qualcomm path rejects Vulkan/D3D11 shared-fence import with
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

## Next phase: controlled performance qualification and v1.0 polish

Planning only; implementation and optimization require separate work:

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
Old M2â€“M4 Apollo convenience work is deprioritized; its notes are retained as
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
