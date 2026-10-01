# Next step: M1B P0-R / Vibepollo requalification

The immediate gate is compatibility realignment with Nonary/Vibepollo. The active
codec is Themaister/pyrowave `186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream
ID `186f0393`, API 0.6.0. See [the current protocol/patch contract](PYROWAVE_VIBEPOLLO.md),
[historical evidence](M1B_PYROWAVE_SPIKE.md), and [validation](VALIDATION.md).
Old x64 RTX 4070 Ti and native ARM64 Surface Pro 11 / Snapdragon X Plus / Adreno
X1-85 GPU results used `f6fb84...`; the new codec is not requalified by them.

## P0-R: compatibility realignment

1. Build the exact upstream pin on x64 and ARM64 with recorded source/patch hashes.
   Apply Vibepollo's decoder short-block safety patch; retain the encoder-only
   pool/sizing patches with their provenance for future fixture work.
2. Run GPU-free LE compatibility and sequence/block/padding complete-frame parser
   tests, malformed input/mutation cases, loader API/export/load/reload tests,
   qmake experimental compile/link and PE/import checks on both architectures.
3. Rerun the deterministic 1920x1080 8-bit SDR 4:2:0 proof in both formats through
   three decoder lifetimes on RTX 4070 Ti and native Adreno hardware. Record
   runtime/fixture/output hashes, OS/driver, adapter and rejection/recovery logs.
   Unavailable Vulkan in hosted CI is a skip, not hardware qualification.

No live Session negotiation, advertisement, UI setting, normal packaging or
frame-pacing change. `PYRW` is historical offline framing only. Both local
roundtrips prove codec/framing behavior, not network interoperability.

## P0.5: presentation after new-codec hardware qualification

Use the Vibepollo-compatible build for SDL IYUV upload/window presentation; SDR
BT.709 color, limited/full range and chroma-siting/scaling patterns; resize,
decoder/device recreation and device-loss recovery; and runtime deployment/import
closure/clean-machine policy. Qualify 4:4:4 presentation separately after 4:2:0,
including the retained encoder sizing fix for new fixtures. Stay offline and
exclude PyroWave HDR. Existing frame pacing and release contents remain intact.

## P1a: explicit live SDR 4:2:0

Add explicit experimental codec choice, complete runtime/presentation preflight,
bitstream-ID verification, minimal audited SCM/RTSP/SDP/common-c changes and
Vibepollo LE length-prefixed compatibility framing. Auto remains standard codec
selection. Refuse/warn on missing/mismatched bitstream ID; API 0.6.0 is a separate
compatibility axis. Validate safe standard-codec fallback/reconnect without
replaying host-app actions, setup/encoder failure, absent/partial capabilities,
missing/wrong DLL/export/API/driver, device loss, software policy and HDR exclusion.
Re-run H.264/HEVC/AV1 lifecycle/audio/input tests and named-target measurements.
No live work is implemented in P0-R.

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
