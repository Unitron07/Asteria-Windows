# Validation and release evidence

This is the qualification checklist. The [baseline report](BASELINE.md) records the merged M0 import, successful x64 CI, local upstream CLI startup, and the owner's successful manual test confirmation. M0A native ARM64 is complete from the owner's recorded Surface Pro 11th Edition test below. Exact release artifact/host details and some individual hardware cases remain undocumented. Attach new results to the relevant milestone PR and link them from release notes; distinguish source inspection, builds, general user confirmation, and measured hardware tests.

## Recorded manual test

- Tester: project owner.
- Result: owner confirmed the tested client works, then merged PR #1.
- Test date, exact artifact, Windows build, process architecture, GPU/driver, host product/version, stream settings, and individual input/audio cases: not supplied.
- Scope: successful user-reported baseline test. Do not infer independent Sunshine and Apollo coverage, native ARM64 execution, clean-machine status, hardware decoding, or measured performance from this statement.

Use the result template below to fill applicable gaps during release qualification.

## Owner-reported Windows ARM64 comparison (2026-09-25)

- **Clients and workload:** native ARM64 Asteria versus stock x64 Moonlight under emulation on a Surface Pro 11th Edition with Snapdragon X Plus and 16 GB RAM and an Apollo host; same game, 2560×1440, approximately 60 FPS, AV1. The owner repeated the comparison.
- **Observed streaming result:** effectively identical streaming in these runs, with no meaningful decode, render, or frame-queue difference and no observed stream regression. The initial on-screen samples were near 60 FPS with zero displayed network/jitter drops. This is an observed result for this setup, not a controlled median/p95 benchmark or a universal performance claim.
- **Qualitative UI observation:** Asteria menus/settings felt noticeably smoother and snappier. Subjective, with no timing measurement.
- **Native execution:** the owner verified the Asteria process as native ARM64. The comparison used the official x64 Moonlight release under Windows ARM64 emulation on the same device. No official native ARM64 upstream Moonlight release exists; CI-built unmodified upstream ARM64 artifacts are internal reference builds only.
- **Evidence limits:** Windows build, GPU driver, exact artifact/commit/hash, selected decoder, Apollo version, run duration, and raw logs were not recorded. M0A is complete from the owner's validation; these details remain useful for release qualification. Sunshine is not a required target for this Apollo-based project/preview.

The remaining release checks below are separate from the completed M0A milestone; details also appear in [BASELINE.md](BASELINE.md).

## Automated package architecture checks

Both upstream and candidate CI jobs run the offline package guard tests, then validate the final portable ZIP during the build wrapper. Review `package-architecture.json` in the architecture-specific evidence artifact: `passed` must be true, its SHA-256 must match the tested ZIP, and every EXE/DLL must have the target's machine type. The scanner includes nested Qt plugins and rejects foreign architectures and malformed headers. Its regression suite deliberately adds an x64 DLL to an ARM64 package and verifies rejection, with the reverse case for x64.

The gate has local synthetic test coverage and passes for upstream/candidate x64 and ARM64 packages in [run 34797782854](https://github.com/Unitron07/Asteria-Windows/actions/runs/34797782854). Retain the hash-bound reports for the exact release ZIPs. Passing PE inspection does not prove native process execution, clean-machine launch, dependency completeness, or hardware decoding. Those remain real-device checks below.

## Functional checks

Apply this checklist to the advertised release scope. Clipboard, virtual-display controls, server commands, profiles, and new performance modes are future work; mark their checks not applicable until implemented and included. Installer lifecycle checks apply when installers are offered. The first portable preview covers inherited streaming and Asteria identity.

| Area | Required cases | Pass condition |
| --- | --- | --- |
| Standard host | Pinned Apollo build; discovery/manual host, pair/unpair, app list, launch/resume/disconnect/quit | Baseline functions work; disconnect and quit have distinct effects |
| Session lifecycle | 20 connect/disconnect cycles; timeout, network interruption, sleep/resume, client crash/restart | No stuck input, stale requests, unintended host actions, or persistent new resource leak |
| Host changes | Switch between two paired hosts; reconnect after permissions change | No stale capability state or cross-host clipboard response |
| Keyboard/mouse | Relative/direct modes, wheel, Alt+Tab/capture release, non-US layout, dead keys; IME if claimed | Correct host input and reliable local escape; no stuck modifiers |
| Display | 100/150/200% DPI, two monitors of different scale, fullscreen/windowed, implemented scaling modes | Correct direct-pointer corner/center mapping, usable UI, safe monitor removal |
| Audio/gamepad | Output-device change, stereo baseline, controller hotplug/rumble | No crash or stuck controller state; negotiated features work |
| Clipboard | Both directions, Unicode/emoji/newlines/empty text, unsupported formats, size limit, denial, slow response, malformed/HTTP-200 error reply | Only authorized text applied; errors preserve clipboard; no echo loop or sensitive log content |
| Virtual display | Ready/missing driver, denied request, launch/resume, disconnect/crash/reconnect, existing host session | Host state and client status match the documented host behavior; no unrelated display reset |
| Server commands | Harmless command, deny permission, changed list, invalid index, disconnect during send | Correct indexed action, no unsolicited replay, no false execution-success claim |
| Identity/package | Moonlight side by side, portable directory, clean-machine launch, installer update/uninstall | No shared credentials/settings collisions or unintended data removal |
| Accessibility | Keyboard-only settings/actions, focus visibility, readable scale, accessible names | Included workflows remain operable without a mouse |

Use fixtures/unit tests for permission parsing, profiles/migrations, URL encoding, coordinate transforms, stale-session cancellation, and native command encoding. Use a bounded mock HTTP service for clipboard/error contracts. Run these in CI once implemented. Real Apollo sessions and GPU/input/display checks remain manual or hardware-lab tests; do not label hosted-runner builds as full compatibility coverage.

## Performance method

Moonlight PC's existing performance statistics are the default M1A measurement source. The owner's repeated native ARM64 Asteria versus emulated x64 Moonlight Apollo AV1 runs above are initial evidence of stream parity, with the listed evidence limits. Add instrumentation only after naming a concrete missing metric and the decision it would support. Keep current frame pacing unless repeatable comparisons show a real problem or benefit.

1. Build Asteria in Release configuration and record its SHA. Compare with the official x64 Moonlight release under Windows ARM64 emulation for the practical user comparison; a CI-built unmodified upstream ARM64 artifact may be used as an optional internal engineering reference. Record the exact comparison versions/SHAs.
2. Use the same client GPU/driver, host build/GPU/encoder, resolution, refresh rate, codec, bitrate, display, power mode, and network path.
3. Warm up for two minutes, then collect at least three five-minute runs of each build. Alternate their order and use the same reproducible workload. Start with wired LAN 1080p60 H.264 SDR, then test enabled extensions.
4. Capture Moonlight's built-in performance overlay/log readings: decode time, rendering time, frame-queue delay, network latency/variance, dropped frames, codec, resolution, and observed FPS where available. Record median and p95 only if the source supplies enough samples or a trustworthy aggregate; do not infer percentiles from a screenshot. Note unavailable metrics, plus frame pacing observations, CPU/GPU load, memory trend, audio glitches, and connection failures where measured. Keep screenshots or raw logs and the workload description.
5. Measure input-to-photon latency with a high-speed camera or suitable hardware if reporting that metric. Internal decode/network stats are not an input-to-photon measurement.
6. Perform a 30-minute session soak and the connect/disconnect lifecycle test with clipboard/overlays enabled, if included.

**Proposed initial regression gate:** on baseline hardware, flag an increase in p95 decode-plus-render time greater than the larger of 1 ms or 10% of baseline, or a dropped-frame-rate increase greater than 0.5 percentage points. Investigate repeated failures before release; compare measurement variance and record any deliberate, feature-specific exception. These thresholds are engineering targets to calibrate from M0 results, not measured performance promises. Crashes, stuck input, cross-host data transfer, and broken baseline streaming are release blockers regardless of timing averages.

## Hardware coverage

For the first preview, record the baseline H.264 1080p60 SDR path on Windows 11 x64 and ARM64 clients against Apollo, plus the advertised AV1 path where supported. M0A native ARM64 qualification is complete; these are release-specific coverage checks. Sunshine is not required for this project/preview. Retain upstream codec functionality and smoke-test each additional path available on each machine.

Before claiming broad stable support, test representative Intel, AMD, and NVIDIA clients; hybrid-GPU selection; supported HEVC/AV1/HDR paths; high refresh rate; and office-text quality with YUV 4:4:4 where both ends support it. List each tested GPU, driver, OS build, codec/chroma/HDR mode, and host version. Mark unavailable combinations untested; unsupported codec hardware should produce a clear fallback or error.

For ARM64, record the device model, SoC/GPU, driver, Windows build, actual process architecture, and decoder in use. Verify PE machine type `ARM64` (`0xAA64`) for the client and shipped native runtime DLLs, including Qt plugins and AntiHooking. Confirm on-device native execution and runtime startup with the deployed dependencies. A successful x64-emulated launch or cross-build is insufficient. For a practical same-device comparison, use the official x64 Moonlight release under Windows ARM64 emulation. CI-built unmodified upstream ARM64 artifacts may be used for internal same-architecture analysis, but are not official upstream releases.

Test clean-machine portable launch, discovery/pairing, launch/resume/disconnect, stereo audio, keyboard, direct/relative mouse, gamepad, focus/capture release, DPI changes, sleep/resume, and a 30-minute streaming soak on ARM64. Record unavailable devices or Apollo host access as release limitations. Test HEVC/AV1/HDR only when supported by the actual device/host combination; record fallback behavior and avoid blanket codec claims.

Windows 10 x64 still needs its own declared minimum OS/runtime and hardware qualification. Local multi-monitor behavior does not establish support for simultaneous remote-monitor streams.

## Result template

- Date and tester:
- Upstream SHA / candidate SHA / dependency and submodule manifest:
- Client device/SoC, OS build, OS and process architecture (including emulation status), GPU, driver, display/DPI, power mode:
- Runtime PE architecture inventory, Qt/codec versions, and decoder selected:
- Host product/version, OS, GPU/encoder, virtual-display driver:
- Network and stream settings:
- Workload and run duration:
- Functional cases: pass / fail / not tested:
- Performance baseline / candidate / variance:
- Raw evidence:
- Known limitations, blockers, and linked fixes:

## First public preview record (2026-09-25)

[v0.1.0-preview.1](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.1.0-preview.1) is an unsigned, portable Windows x64 and native ARM64 prerelease. Both packages came from main commit [`34dfd937528586babded200fafaee535e21d4a40`](https://github.com/Unitron07/Asteria-Windows/commit/34dfd937528586babded200fafaee535e21d4a40) in successful [CI run 36191650824](https://github.com/Unitron07/Asteria-Windows/actions/runs/36191650824). The tag resolves to that commit. The release job checked each original ZIP against its passing, hash-bound `package-architecture.json` and confirmed 69 x64 and 68 ARM64 EXE/DLL entries, including deployed runtimes and plugins. It also checked `portable.dat`, root license, and provenance notices before attaching the unchanged ZIPs.

| Portable release asset | SHA-256 |
| --- | --- |
| `Asteria-v0.1.0-preview.1-windows-x64-portable.zip` | `1b55c39006ec333716414c167031a71585921f1112cca634ac02e7e897d54a60` |
| `Asteria-v0.1.0-preview.1-windows-arm64-portable.zip` | `0c7de9bf6a34ca1aef7fe7ef66b2fa15a3f01ef1d26eec615fcb7b7cf8efb05a` |

The release includes `SHA256SUMS.txt`, symbols, recursive corresponding-source snapshots, and architecture/build evidence for each target. The owner confirmed the completed x64 and ARM64 CI builds work. The previously recorded Surface Pro 11th Edition native-process and repeated Apollo AV1 comparison remains evidence for that setup; exact artifact hashes and several host/client details for that earlier comparison were not recorded. No clean-machine or full functional-matrix result was supplied, so this preview does not claim broad qualification. The executable's inherited 6.1.0 metadata and bundled development-baseline provenance notice refer to the Moonlight-based build; `v0.1.0-preview.1` is Asteria's public preview version. No installer was shipped.

## Release checklist

- [x] M0 x64 upstream and candidate CI builds pass; see run 34736992552.
- [x] Upstream and candidate x64/ARM64 CI builds and final-ZIP architecture checks pass; see run 34797782854.
- [ ] Documented release builds reproduced locally.
- [x] M0A: ARM64 package architecture gate passed and the owner verified native Asteria process execution and Apollo AV1 streaming on the recorded Surface Pro 11th Edition.
- [ ] Record the exact release ARM64 artifact/hash, Windows build, driver, decoder, and Apollo version.
- [ ] Functional and regression gates pass for the advertised scope; the reported AV1 stream comparison covers only one workload.
- [ ] Separate x64 and ARM64 portable builds run with deployed runtimes on clean machines; data-location behavior is documented for each.
- [x] Exact preview commit, hashes, corresponding source with submodules, licenses/notices, and symbols are available in the release assets.
- [ ] Stable executable/installer signing and installer lifecycle checks pass when those artifacts are offered.
- [x] Preview release notes distinguish the owner-reported validation, inherited-but-untested paths, deferred features, and evidence limits.
