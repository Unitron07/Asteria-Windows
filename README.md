# Asteria

**Asteria** is an early-preview native Windows game-streaming client derived from [Moonlight PC](https://github.com/moonlight-stream/moonlight-qt). Enhanced integration with [Apollo](https://github.com/ClassicOldSong/Apollo) and selected desktop/workflow ideas inspired by [Artemis Android](https://github.com/MobinYengejehi/Artemis) are planned.

> **Preview status:** expect bugs and incomplete Asteria-specific functionality. The Asteria identity/rebrand is implemented. x64 and native ARM64 builds and portable packaging pass CI; real Windows 11 ARM64 device qualification remains pending and is required before the first public preview. Report reproducible problems in [GitHub issues](https://github.com/Unitron07/Asteria-Windows/issues).

## Available now

- Moonlight PC's existing discovery, pairing, streaming, resolution/frame-rate controls, keyboard/mouse input, gamepad support, audio, and performance statistics.
- Inherited hardware decoding, HDR, and AV1 paths, subject to the client GPU, driver, host, and stream configuration; these are not blanket hardware qualification claims.
- Implemented Asteria application, settings, pairing, log, and Windows package identity, designed for side-by-side use with Moonlight. On-device coexistence checks remain part of release qualification.
- Separate x64 and native ARM64 CI builds and portable ZIPs. The owner reported a successful x64 baseline test; detailed hardware/host coverage remains recorded as incomplete in the [baseline report](docs/BASELINE.md).

Sunshine compatibility is inherited from Moonlight. Standard streaming against Apollo must be tested separately; Apollo-specific capability handling, clipboard transfer, virtual-display controls, and server commands are roadmap work, not included preview features.

## First public preview

The first preview targets **Windows 11 x64 and native ARM64**, using portable ZIPs. It covers the inherited streaming baseline and implemented Asteria identity. Installer distribution and signing follow later release validation.

Before publishing, qualify the ARM64 ZIP on a real Windows 11 ARM64 device, smoke-test x64 from the same release commit, and record tested host versions, hardware, codecs, and known issues using the [validation checklist](docs/VALIDATION.md). CI packaging alone does not establish native execution, hardware decoding, or clean-machine compatibility.

## Roadmap

1. **M0 — baseline import:** merged.
2. **M0A — native Windows ARM64:** build/CI/package architecture work is implemented and passes CI; real-device qualification remains.
3. **M1 — Asteria identity and desktop workflow:** identity/rebrand implemented; profiles, configurable shortcuts, and additional session workflows remain planned.
4. **M1A — Windows performance/frame pacing:** planned measurement and benchmarking before changing defaults.
5. **M2 — pointer/scaling correctness and Apollo capability parsing:** planned.
6. **M3 — Apollo text clipboard and virtual-display requests:** planned.
7. **M1B — experimental PyroWave streaming:** planned as an optional codec path after M1A measurements and a Vibepollo protocol spike.
8. **M4 — Apollo server commands:** planned.
9. **M5 — release qualification:** apply checks to each release's advertised scope.

Profiles, performance changes, PyroWave, and Apollo extensions are not prerequisites for this initial preview. PyroWave is planned, not implemented or qualified. Touch-overlay parity, simultaneous multi-stream viewing, and file transfer are later work.

Asteria retains Moonlight's Qt/QML UI, SDL input/session stack, hardware decoding paths, build structure, and upstream history. New behavior is added at narrow integration boundaries so upstream security and correctness fixes remain practical to merge.

See the detailed [porting plan](docs/PORTING_PLAN.md), [architecture](docs/ARCHITECTURE.md), [feature audit](docs/FEATURE_AUDIT.md), and [Windows build guide](docs/BUILD_WINDOWS.md).

## Naming and provenance

**Asteria** is the application/product name; **Asteria-Windows** is this repository.

The source remains a derivative of Moonlight PC and retains upstream history, licenses, source notices, submodules, and attribution. `docs/MOONLIGHT_README.md` preserves the upstream README. References to Artemis in the audit and roadmap refer to the separate Android project used as a behavioral reference; Asteria is an independent Windows client.

## Licensing and maintenance

The reviewed Moonlight and Artemis sources include GPLv3 licensing. Preserve all applicable notices and corresponding-source obligations when publishing builds. Keep Asteria changes small and isolated so upstream Moonlight security and correctness fixes can be integrated regularly.
