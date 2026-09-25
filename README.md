# Asteria

**Asteria** is an early-preview native Windows game-streaming client derived from [Moonlight PC](https://github.com/moonlight-stream/moonlight-qt). Enhanced integration with [Apollo](https://github.com/ClassicOldSong/Apollo) and selected desktop/workflow ideas inspired by [Artemis Android](https://github.com/MobinYengejehi/Artemis) are planned.

> **Preview status:** [v0.1.0-preview.1](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.1.0-preview.1) is released for Windows x64 and native ARM64 as unsigned portable ZIPs. Expect bugs and incomplete Asteria-specific functionality. The owner verified both CI builds work and previously validated native ARM64 execution on a Surface Pro 11th Edition; see the [release notes](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.1.0-preview.1) and [validation record](docs/VALIDATION.md) for the tested scope and evidence limits. Report reproducible problems in [GitHub issues](https://github.com/Unitron07/Asteria-Windows/issues).

## Available now

- Moonlight PC's existing discovery, pairing, streaming, resolution/frame-rate controls, keyboard/mouse input, gamepad support, audio, and performance statistics.
- Inherited hardware decoding, HDR, and AV1 paths, subject to the client GPU, driver, host, and stream configuration; these are not blanket hardware qualification claims.
- Implemented Asteria application, settings, pairing, log, and Windows package identity, designed for side-by-side use with Moonlight. On-device coexistence checks remain part of release qualification.
- Separate x64 and native ARM64 CI builds and portable ZIPs. The owner reported a successful x64 baseline test; detailed hardware/host coverage remains recorded as incomplete in the [baseline report](docs/BASELINE.md).
- **M0A owner validation:** Surface Pro 11th Edition, Snapdragon X Plus, 16 GB RAM; the Asteria process was verified as native ARM64. Against the official x64 Moonlight release running under Windows ARM64 emulation on the same device, repeated same-game 2560×1440, approximately 60 FPS AV1 streams on Apollo were effectively identical, with no meaningful decode, render, or frame-queue regression. Asteria's menus/settings felt noticeably smoother and snappier (qualitative, not timed). The Windows build, GPU driver, decoder, Apollo version, artifact hash, and run durations were not recorded; this is not a broad compatibility or measured latency claim. There is no official native ARM64 upstream Moonlight release; CI-built unmodified upstream ARM64 artifacts are internal reference builds.

The owner uses Apollo for this project and preview; Sunshine is not a required qualification target. Apollo-specific capability handling, clipboard transfer, virtual-display controls, and server commands are roadmap work, not included preview features.

## First public preview

**[v0.1.0-preview.1 is available now](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.1.0-preview.1)** for Windows x64 and native ARM64 as portable ZIPs. Both packages were built from commit `34dfd937528586babded200fafaee535e21d4a40` in the same successful [CI run](https://github.com/Unitron07/Asteria-Windows/actions/runs/36191650824). The release includes SHA-256 checksums, symbols, corresponding source, and architecture/build evidence. Extract the ZIP into a writable folder and keep `portable.dat` next to `Asteria.exe`. No installer is offered in this preview.

The owner confirmed the completed x64 and ARM64 builds work. Earlier native ARM64 Apollo AV1 testing is described above; exact Windows, driver, decoder, host-version, and artifact-hash details were not recorded for that comparison. Broad hardware and functional qualification remains open in the [validation checklist](docs/VALIDATION.md).

## Roadmap

1. **M0 — baseline import:** merged.
2. **M0A — native Windows ARM64:** complete; CI/package architecture checks and owner-verified native ARM64 execution with Apollo AV1 streaming on a Surface Pro 11th Edition are recorded.
3. **M1 — Asteria identity and desktop workflow:** identity/rebrand implemented; profiles, configurable shortcuts, and additional session workflows remain planned.
4. **M1A — Windows performance baseline:** use Moonlight's built-in statistics for initial Asteria ARM64 versus emulated x64 Moonlight comparisons. The owner's repeated AV1 runs are initial evidence of stream parity, not a controlled benchmark. Add instrumentation only for a concrete missing metric; change frame pacing only when measurements show a real problem or benefit.
5. **M1B — experimental PyroWave streaming:** next development task is a focused Vibepollo/PyroWave protocol spike. The codec remains optional and unimplemented in Asteria; see [the spike notes](docs/M1B_PYROWAVE_SPIKE.md).
6. **M2 — pointer/scaling correctness and Apollo capability parsing:** planned.
7. **M3 — Apollo text clipboard and virtual-display requests:** planned. 
8. **M4 — Apollo server commands:** planned.
9. **M5 — release qualification:** apply checks to each release's advertised scope.
10. **M6 — Asteria VR (remote PCVR):** planned after core streaming and performance work, with separate proof-of-concept and headset qualification gates.

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
