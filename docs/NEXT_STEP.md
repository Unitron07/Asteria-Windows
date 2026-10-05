# Next step: post-v0.2.0 native Vulkan presentation

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
