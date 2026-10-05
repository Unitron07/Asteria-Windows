<p align="center">
  <img src="assets/branding/asteria-logo-readme.png" alt="Asteria logo" width="280">
</p>

# Asteria

**[Asteria v0.2.0 is released](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.2.0):** Moonlight PC v6.2.0 baseline, native Windows x64/ARM64,
and explicitly selected **PyroWave (Experimental)** in normal builds, with pinned
runtime/provenance and codec-aware bitrate controls. Automatic uses standard codecs.
Live Vibepollo SDR 8-bit 4:2:0 is owner-validated on Surface Pro 11 / Snapdragon X
Plus / Adreno X1-85; this is not production-stable or universal hardware qualification.
See [validation](docs/VALIDATION.md#current-live-arm64-owner-result).

**Asteria** is a native Windows game-streaming client derived from [Moonlight PC](https://github.com/moonlight-stream/moonlight-qt). Its active direction is codec experimentation and low-latency presentation on native Windows x64/ARM64, followed by isolated remote sessions and future remote PCVR.

> **Release status:** [v0.2.0](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.2.0) is the current normal release for Windows x64 and native ARM64, distributed as unsigned portable ZIPs. PyroWave remains Experimental and explicitly selected. See the [release notes](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.2.0) and [validation record](docs/VALIDATION.md#asteria-v020-release-record) for tested scope and limitations. [v0.1.0](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.1.0) and its evidence remain available as history.

## Available now

- Moonlight PC's existing discovery, pairing, streaming, resolution/frame-rate controls, keyboard/mouse input, gamepad support, audio, and performance statistics.
- Inherited hardware decoding, HDR, and AV1 paths, subject to the client GPU, driver, host, and stream configuration; these are not blanket hardware qualification claims.
- Implemented Asteria application, settings, pairing, log, and Windows package identity, designed for side-by-side use with Moonlight. On-device coexistence checks remain part of release qualification.
- Separate x64 and native ARM64 CI builds and portable ZIPs. The owner reported a successful x64 baseline test; detailed hardware/host coverage remains recorded as incomplete in the [baseline report](docs/BASELINE.md).
- **M0A owner validation:** Surface Pro 11th Edition, Snapdragon X Plus, 16 GB RAM; the Asteria process was verified as native ARM64. Against the official x64 Moonlight release running under Windows ARM64 emulation on the same device, repeated same-game 2560×1440, approximately 60 FPS AV1 streams on Apollo were effectively identical, with no meaningful decode, render, or frame-queue regression. Asteria's menus/settings felt noticeably smoother and snappier (qualitative, not timed). The Windows build, GPU driver, decoder, Apollo version, artifact hash, and run durations were not recorded; this is not a broad compatibility or measured latency claim. Moonlight v6.2.0 now provides an official ARM64 portable release. The earlier comparison above used emulated x64; CI-built upstream artifacts remain internal reference builds.

The owner uses Nonary/Vibepollo (the Apollo-derived host service) for this project; Vibepollo is the primary PyroWave protocol target. The original preview baseline used Apollo. Sunshine is not a required qualification target. Apollo-specific capability handling, clipboard transfer, virtual-display controls, and server commands are deprioritized possible future integrations, not included release features.

## Current public release

**[Asteria v0.2.0 is available now](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.2.0)** for Windows x64 and native ARM64 as unsigned portable ZIPs. Both packages were built from commit `42f756e465288157608fe894e3a3dfe800b05344` in the same successful [Windows qualification run](https://github.com/Unitron07/Asteria-Windows/actions/runs/37262907765), with both-target [PyroWave qualification](https://github.com/Unitron07/Asteria-Windows/actions/runs/37263855400). The release includes SHA-256 checksums, symbols, corresponding source and build/architecture evidence. Extract into a writable folder and keep `portable.dat` beside `Asteria.exe`. No installer is offered.

Normal packages include PyroWave (Experimental), still explicit-only; Automatic uses standard codecs. The inherited application version is 6.2.0 and the project release tag is v0.2.0. Broad GPU/hardware qualification remains open; see the [release record](docs/VALIDATION.md#asteria-v020-release-record). Release-status changes after the tag are documentation-only; binaries/source correspond to the tagged commit above.

## Roadmap

1. **Foundation / ARM64 / identity (M0–M1A):** baseline import, native x64/ARM64
   and Asteria identity are established; desktop follow-ups and measured Windows
   performance work remain. Use Moonlight's existing statistics first, and change
   behavior only for a measured problem or benefit.
2. **Experimental PyroWave (M1B):** offline decode/presentation is qualified on the
   named x64/ARM64 targets; live Vibepollo SDR 4:2:0 is owner-validated on the named
   ARM64 device. Broader GPU interop/performance qualification remains open.
   Standard codecs and Moonlight behavior remain the baseline.
3. **Native Vulkan PyroWave presentation:** the next major technical milestone,
   post-v0.2.0. Planned GPU-resident presentation aims to avoid CPU readback/re-upload
   and Vulkan/D3D11 external fence sharing while retaining safe fallback paths.
   It is not implemented.
4. **Isolated sessions / MultiSeat:** the next major feature area after performance;
   one host remains usable locally while a remote user gets an isolated session.
   Most work belongs in a separate Asteria-oriented Vibepollo fork/host extension;
   Asteria provides controls/status, streaming and isolated input transport.
   Native-class performance is a possible target only with hardware headroom.
5. **Asteria VR:** paired client/host remote PCVR after isolated-session work,
   whose host-control/lifecycle infrastructure may be reusable. This is a priority
   order, not a hard technical dependency; PyroWave is not required.

The old M2–M4 Apollo convenience milestones are deprioritized. Capability parsing,
clipboard, virtual-display requests and server commands may return as supporting
integrations. Historical identifiers, technical notes and release-qualification
gates remain in the [porting plan](docs/PORTING_PLAN.md).
v0.2.0 is released; native Vulkan presentation, isolated sessions and VR remain future work.

### Planned Asteria VR

Asteria VR is a PC-to-PC remote PCVR feature: the host PC runs SteamVR and renders the game, while the VR headset is physically connected to a Windows client PC. This requires an Asteria-oriented Vibepollo fork/host extension for SteamVR/OpenXR, virtual HMD integration, pose ingestion, stereo capture and session timing/lifecycle. The client handles the local headset/runtime, tracking/input transport, stereo decoding, presentation and timing/reprojection. The design is headset-agnostic; a PSVR2 with its PC adapter may eventually be one locally attached configuration, alongside other PCVR headsets.

This is separate from the PSVR2 wireless-adapter project, which uses a phone and wearable bridge. Asteria VR does not use that bridge. It also does not depend on the optional M1B PyroWave codec experiment, although later codec or transport work may be reusable. See [the VR phase in the porting plan](docs/PORTING_PLAN.md) for staged implementation and qualification.

Asteria retains Moonlight's Qt/QML UI, SDL input/session stack, hardware decoding paths, build structure, and upstream history. New behavior is added at narrow integration boundaries so upstream security and correctness fixes remain practical to merge.

See the detailed [porting plan](docs/PORTING_PLAN.md), [architecture](docs/ARCHITECTURE.md), [feature audit](docs/FEATURE_AUDIT.md), and [Windows build guide](docs/BUILD_WINDOWS.md).

## Upstream maintenance

Moonlight history is preserved. The explicit baseline is v6.2.0; weekly automation watches upstream `master`, proposes clean merges as PRs and reports conflicts for human integration. Upstream changes are never auto-merged. See [maintainer instructions](docs/UPSTREAM_SYNC.md). v0.2.0 is the current release.

## Naming and provenance

**Asteria** is the application/product name; **Asteria-Windows** is this repository.

The source remains a derivative of Moonlight PC and retains upstream history, licenses, source notices, submodules, and attribution. `docs/MOONLIGHT_README.md` preserves the upstream README. References to Artemis in the audit and roadmap refer to the separate Android project used as a behavioral reference; Asteria is an independent Windows client.

## Licensing and maintenance

The reviewed Moonlight and Artemis sources include GPLv3 licensing. Preserve all applicable notices and corresponding-source obligations when publishing builds. Keep Asteria changes small and isolated so upstream Moonlight security and correctness fixes can be integrated regularly.
