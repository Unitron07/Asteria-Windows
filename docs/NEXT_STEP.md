# Next step: qualify the first portable preview

Asteria identity/rebranding and native x64/ARM64 build and packaging work are implemented. The next gate is completing Windows 11 ARM64 device qualification, followed by x64 smoke testing from the same release commit. Profiles, new session workflows, performance changes, and Apollo extensions remain roadmap work and are not required for this first preview.

Track device evidence in [GitHub issue #3](https://github.com/Unitron07/Asteria-Windows/issues/3).

## Implemented baseline

- M0 preserves Moonlight PC source history, notices, licenses, and submodules.
- PRs #5 and #6 established x64/ARM64 target selection, pinned dependencies, and upstream/candidate CI.
- PR #8 corrected ARM64 portable-package CRT contamination; the final ZIP architecture gate checks all packaged EXE/DLL files, including Qt plugins.
- PR #9 implemented Asteria application, settings, pairing, logs, artwork, and Windows package identity.
- All four upstream/candidate x64/ARM64 jobs passed in [run 34797782854](https://github.com/Unitron07/Asteria-Windows/actions/runs/34797782854) at `ab69dc3f76ff6b163ab91c35c3795d4da478f022`, including final-ZIP architecture validation.

See [BUILD_WINDOWS.md](BUILD_WINDOWS.md) for commands and [BASELINE.md](BASELINE.md) for evidence limits. CI does not establish native device execution or streaming compatibility.

## Owner-reported ARM64 device comparison

On a Windows-on-ARM device, the owner compared native ARM64 Asteria with stock x64 Moonlight running under emulation. **Qualitative UI observation:** Asteria's menus and settings felt noticeably smoother and snappier; no launch/UI timings were collected. **Observed streaming result:** repeated tests of the same game at 2560×1440 and approximately 60 FPS with AV1 appeared effectively identical. The displayed decode, render, and frame-queue statistics showed no meaningful difference, and no stream regression was observed. Two initial screenshots were discussed, but raw screenshots, run lengths, and a controlled benchmark record are not attached here.

This is a comparison with **emulated x64 Moonlight**, not the pinned upstream native ARM64 build. The device model, Windows build, driver, process architecture confirmation, selected decoder, Apollo version, and package hashes were not supplied. Sunshine has not been separately tested or recorded. See [BASELINE.md](BASELINE.md) and [VALIDATION.md](VALIDATION.md) for the evidence and remaining gates.

## Remaining preview qualification

Use the [validation checklist and result template](VALIDATION.md) to record:

- Exact candidate commit, portable ZIP hash, and matching package-architecture report.
- Windows 11 ARM64 device/SoC/GPU, driver, OS build, native process architecture, and selected decoder; launch without development tools or x64 emulation.
- Discovery/manual host, pair/unpair, launch/resume/disconnect/quit, H.264 1080p60 SDR, audio, keyboard, relative/direct mouse, gamepad, focus/capture release, DPI changes, sleep/resume, and a 30-minute soak.
- Separate Sunshine and Apollo host versions and standard-streaming results. This does not qualify Apollo-specific extensions.
- Available HEVC/AV1/HDR paths and fallback behavior; mark unsupported or untested paths explicitly.
- Settings persistence, portable data location, and side-by-side use with Moonlight.
- A same-device pinned upstream native ARM64 comparison and an x64 smoke test from the same release commit. The reported stock x64 Moonlight comparison does not satisfy the native upstream comparison.

Missing hardware or host access remains an open gate. Do not mark M0A complete solely because CI passes.

## Release wording after qualification

When the remaining ARM64 checks pass, tie release claims to the recorded device, Windows build, host versions, and tested paths. Link the hardware report from release notes and preserve untested limitations.

Publish separate x64 and ARM64 portable preview ZIPs with exact versions, hashes, symbols, corresponding source/submodules, notices, and known issues. Installer distribution/signing and new Asteria-specific features follow later qualification.
