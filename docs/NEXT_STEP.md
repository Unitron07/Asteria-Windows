# Next step: M1B P1a live Vibepollo SDR 4:2:0 integration (future)

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

## P1a NEXT / FUTURE: explicit live SDR 4:2:0

P1a is not implemented by this documentation update. Future work would add explicit
experimental codec choice, complete runtime/presentation preflight,
bitstream-ID verification, minimal audited SCM/RTSP/SDP/common-c changes and
Vibepollo LE length-prefixed compatibility framing. Auto remains standard codec
selection. Refuse/warn on missing/mismatched bitstream ID; API 0.6.0 is a separate
compatibility axis. Validate safe standard-codec fallback/reconnect without
replaying host-app actions, setup/encoder failure, absent/partial capabilities,
missing/wrong DLL/export/API/driver, device loss, software policy and HDR exclusion.
Re-run H.264/HEVC/AV1 lifecycle/audio/input tests and named-target measurements.
No live work is implemented in P0-R or P0.5.

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
