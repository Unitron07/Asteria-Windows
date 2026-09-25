# Asteria

**Asteria** is an early-preview native Windows game-streaming client derived from [Moonlight PC](https://github.com/moonlight-stream/moonlight-qt). Enhanced integration with [Apollo](https://github.com/ClassicOldSong/Apollo) and selected desktop/workflow ideas inspired by [Artemis Android](https://github.com/MobinYengejehi/Artemis) are planned.

> **Preview status:** expect bugs and incomplete Asteria-specific functionality. The Asteria identity/rebrand is implemented. x64 and native ARM64 builds and portable packaging pass CI; an owner-reported real-device ARM64 AV1 streaming comparison is recorded below; the remaining ARM64 qualification checks are required before the first public preview. Report reproducible problems in [GitHub issues](https://github.com/Unitron07/Asteria-Windows/issues).

## Available now

- Moonlight PC's existing discovery, pairing, streaming, resolution/frame-rate controls, keyboard/mouse input, gamepad support, audio, and performance statistics.
- Inherited hardware decoding, HDR, and AV1 paths, subject to the client GPU, driver, host, and stream configuration; these are not blanket hardware qualification claims.
- Implemented Asteria application, settings, pairing, log, and Windows package identity, designed for side-by-side use with Moonlight. On-device coexistence checks remain part of release qualification.
- Separate x64 and native ARM64 CI builds and portable ZIPs. The owner reported a successful x64 baseline test; detailed hardware/host coverage remains recorded as incomplete in the [baseline report](docs/BASELINE.md).
- **Owner-reported ARM64 observation:** on a Windows-on-ARM device, native ARM64 Asteria felt noticeably smoother and snappier in menus/settings than stock x64 Moonlight running under emulation. This is subjective and was not timed. In repeated same-game, 2560×1440, approximately 60 FPS AV1 streams against an Apollo host, the owner observed effective stream parity: no meaningful decode, render, or frame-queue difference and no stream regression. The Apollo host version, device/driver, process and decoder details, and run durations were not recorded; this is not a general hardware or host compatibility claim.

Sunshine compatibility is inherited from Moonlight. Standard streaming against Apollo must be tested separately; Apollo-specific capability handling, clipboard transfer, virtual-display controls, and server commands are roadmap work, not included preview features.

## First public preview

The first preview targets **Windows 11 x64 and native ARM64**, using portable ZIPs. It covers the inherited streaming baseline and implemented Asteria identity. Installer distribution and signing follow later release validation.

Before publishing, complete the remaining ARM64 qualification on a real Windows 11 ARM64 device, smoke-test x64 from the same release commit, and record tested host versions, hardware, codecs, and known issues using the [validation checklist](docs/VALIDATION.md). CI packaging alone does not establish native execution, hardware decoding, or clean-machine compatibility.

## Roadmap

1. **M0 — baseline import:** merged.
2. **M0A — native Windows ARM64:** build/CI/package architecture work passes CI; an owner-reported real-device AV1 comparison shows observed stream parity, while full device qualification remains open.
3. **M1 — Asteria identity and desktop workflow:** identity/rebrand implemented; profiles, configurable shortcuts, and additional session workflows remain planned.
4. **M1A — Windows performance/frame pacing:** planned measurement and benchmarking before changing defaults.
5. **M1B — experimental PyroWave streaming:** planned as an optional codec path after M1A measurements and a Vibepollo protocol spike.
6. **M2 — pointer/scaling correctness and Apollo capability parsing:** planned.
7. **M3 — Apollo text clipboard and virtual-display requests:** planned. 
9. **M4 — Apollo server commands:** planned.
10. **M5 — release qualification:** apply checks to each release's advertised scope.
11. **M6 — Asteria VR (remote PCVR):** planned after core streaming and performance work, with separate proof-of-concept and headset qualification gates.

Profiles, performance changes, PyroWave, Apollo extensions, and Asteria VR are not prerequisites for this initial preview. PyroWave and Asteria VR are planned, not implemented or qualified. Touch-overlay parity, simultaneous multi-stream viewing, and file transfer are later work.

### Planned Asteria VR

Asteria VR is a PC-to-PC remote PCVR feature: the host PC runs SteamVR and renders the game, while the VR headset is physically connected to a Windows client PC. The client handles the local headset/runtime and sends tracking and input back to the host. The design is headset-agnostic; a PSVR2 with its PC adapter may eventually be one locally attached configuration, alongside other PCVR headsets.

This is separate from the PSVR2 wireless-adapter project, which uses a phone and wearable bridge. Asteria VR does not use that bridge. It also does not depend on the optional M1B PyroWave codec experiment, although later codec or transport work may be reusable. See [M6 in the porting plan](docs/PORTING_PLAN.md) for staged implementation and qualification.

Asteria retains Moonlight's Qt/QML UI, SDL input/session stack, hardware decoding paths, build structure, and upstream history. New behavior is added at narrow integration boundaries so upstream security and correctness fixes remain practical to merge.

See the detailed [porting plan](docs/PORTING_PLAN.md), [architecture](docs/ARCHITECTURE.md), [feature audit](docs/FEATURE_AUDIT.md), and [Windows build guide](docs/BUILD_WINDOWS.md).

## Naming and provenance

**Asteria** is the application/product name; **Asteria-Windows** is this repository.

The source remains a derivative of Moonlight PC and retains upstream history, licenses, source notices, submodules, and attribution. `docs/MOONLIGHT_README.md` preserves the upstream README. References to Artemis in the audit and roadmap refer to the separate Android project used as a behavioral reference; Asteria is an independent Windows client.

## Licensing and maintenance

The reviewed Moonlight and Artemis sources include GPLv3 licensing. Preserve all applicable notices and corresponding-source obligations when publishing builds. Keep Asteria changes small and isolated so upstream Moonlight security and correctness fixes can be integrated regularly.
