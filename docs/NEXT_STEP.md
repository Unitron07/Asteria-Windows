# Next step: M1B P0.5 offline SDL presentation

**P0-R is COMPLETE** on Windows x64 RTX 4070 Ti and native Windows ARM64
Surface Pro 11th Edition / Snapdragon X Plus / Adreno X1-85. The exact codec is
`186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, API 0.6.0.
Both compatibility and record framing pass expected I420 output through three
decoder lifetimes, load/reload and malformed rejection/recovery. See the
[current hardware record](VALIDATION.md#m1b-p0-r-vibepollo-validation) and
[Vibepollo contract](PYROWAVE_VIBEPOLLO.md). Old `f6fb84...` evidence stays historical.

## P0.5 ACTIVE: offline presentation/color qualification

The standalone SDL harness is implemented; code/CI completion is recorded once
merged. **Owner visual qualification on both targets remains pending.**

1. Download the optional x64/native ARM64 artifacts and run raw I420, compatibility
   and record modes using [exact commands and visual expectations](../tests/pyrowave/README.md#p05-offline-sdl-presentation).
2. Inspect range, BT.709 bars, centered-chroma boundaries and geometry at native
   resolution. Compare raw versus codec; test nearest/linear, smaller/larger/square
   windows, repeated resize, maximize/restore and fullscreen. Save evidence logs.
3. Run ten recreation cycles and a 60 FPS observation. Record backend, adapter,
   dimensions and hashes. Synthetic reset/resource tests exercise recovery code;
   true device-loss hardware behavior remains manual/unproven. Do not infer leak
   freedom or production latency from this probe.

Use SDR 8-bit limited-range 4:2:0 only. Separate 4:4:4 qualification follows;
HDR is excluded. The existing pinned SDL2 API/runtime dependencies are reused.
Normal Session/common-c, codec settings, release packaging and frame pacing stay
unchanged. Local roundtrip and window submission do not prove live interoperability.

## P1a: explicit live SDR 4:2:0

Add explicit experimental codec choice, complete runtime/presentation preflight,
bitstream-ID verification, minimal audited SCM/RTSP/SDP/common-c changes and
Vibepollo LE length-prefixed compatibility framing. Auto remains standard codec
selection. Refuse/warn on missing/mismatched bitstream ID; API 0.6.0 is a separate
compatibility axis. Validate safe standard-codec fallback/reconnect without
replaying host-app actions, setup/encoder failure, absent/partial capabilities,
missing/wrong DLL/export/API/driver, device loss, software policy and HDR exclusion.
Re-run H.264/HEVC/AV1 lifecycle/audio/input tests and named-target measurements.
No live work is implemented in P0-R or P0.5.

## P1b: transport hardening

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
