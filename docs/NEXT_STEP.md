# Next step: qualify M1B P1a GPU presentation performance

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

## P1a live ARM64 video succeeded: GPU performance qualification PENDING

The experimental explicit live SDR 8-bit 4:2:0 path is implemented. Auto stays
on standard codecs. Restricted runtime/API/build-metadata preflight and real SDL
IYUV resource/upload checks occur before host launch; paired HTTPS SCM and strict
DESCRIBE/bitstream-ID checks gate negotiation. Compatibility complete frames feed
the existing runtime and live main-thread SDL decoder. See
[the current contract](PYROWAVE_VIBEPOLLO.md) and [owner guide](../tests/pyrowave/LIVE-OWNER-TEST.md).

The owner confirmed live PyroWave video, audio/input and good visual quality on
Surface Pro 11 / Snapdragon X Plus / Adreno X1-85 ARM64 at 2560x1440, target
120 FPS. The CPU-output bring-up baseline was ~93.3 FPS, ~12.45 ms decode
pipeline time, and ~1.24 ms render. Gated shared GPU planes, Qualcomm fragment
preference, bounded frame slots and standard-layout stats are now implemented.
See [presentation/timing definitions](PYROWAVE_GPU_PRESENTATION.md).

Owner retest on RTX 4070 Ti x64 and Surface Pro 11 ARM64 must establish the
actual improvement and verify resize/fullscreen, disconnect/reconnect and
standard codecs afterward. Record each overlay metric and the selected GPU or
CPU timing mode. Final P1a performance qualification remains **PENDING**; CI and
an offline GPU proof do not establish live 120 FPS. Retry failures manually using
a standard codec; no host launch/resume is automatically repeated.

## P1b LATER: transport hardening

Add live record framing, record-start/lost-buffer metadata, critical packet counts,
parity recovery and adaptive FEC; then bounded partial recovery and sideband
readiness with intact coarse data and more than 90% records. Preserve record
straddling and alignment independence for complete frames. Bandwidth calibration
is later usability work: host link metadata, 32 MiB probe, warmup, slowest of three
and 20% reserve. No probe or HDR decoder is added now.

## Baseline and preview qualification

M0A native Windows ARM64 remains complete based on the owner's baseline-device
validation. M1 identity is implemented. For M1A retain Moonlight's existing
statistics and frame pacing; add instrumentation only for a concrete missing
metric. The prior repeated Apollo AV1 2560x1440/~60 FPS runs are initial stream
parity evidence, not a controlled benchmark. See [BASELINE.md](BASELINE.md).

First-preview release gates remain same-commit x64/ARM64 smoke tests, host
lifecycle/input/audio, clean-machine launch, package architecture/hashes and
recorded Windows/driver/decoder/host versions. PyroWave is not release-blocking.
