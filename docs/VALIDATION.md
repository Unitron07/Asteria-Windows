# Validation and release evidence

## M1B P1a live integration validation

Implementation is **COMPLETE / READY FOR HARDWARE INTEROPERABILITY TEST** behind
the explicit optional build flag. Live owner
Vibepollo qualification is **PENDING** on RTX 4070 Ti x64 and native Surface
Pro 11 / Snapdragon X Plus / Adreno X1-85 ARM64. P0-R/P0.5 history below is unchanged.
The live guide is
[LIVE-OWNER-TEST.md](../tests/pyrowave/LIVE-OWNER-TEST.md).

Local native ARM64 portable Zig tests pass: existing parser (150 cases plus
20,000 mutations), new strict SDP/capability/collision tests, and live-frame
assembly/profile/bounds/recovery tests including a valid 3,546,016-byte frame.
Baseline dependency/preflight, package architecture and ARM64 CRT-repair guard
suites pass.

### First owner P1a interoperability failure and range fix

Owner-reported Surface Pro 11 / Snapdragon X Plus / Adreno X1-85 testing against
Vibepollo reached live video transport successfully. PyroWave negotiation,
pinned runtime initialization, Vulkan decoder creation, SDL I420 initialization,
audio and live video packet receipt succeeded. All live frames were rejected with
`live P1a requires BT.709 limited range` because valid host SDR 4:2:0 sequence
metadata indicated full range. Decoded/rendered live video was not confirmed.

The focused interoperability fix supports BT.709 full and limited SDR 8-bit
4:2:0, preserves parsed range through decode to SDL3 texture colorspace properties,
and rejects unexpected range changes before submitting packets. HDR/PQ, BT.2020,
4:4:4, malformed sequence/framing and extent mismatches remain rejected.
First-sequence logging identifies range once; malformed-frame logs stay rate-limited.
P1a owner qualification remains **PENDING** until a new hardware retest confirms
decoded/rendered live video. Earlier passing CI below predates this fix.

Validation for this fix is recorded with the focused PR; existing parser/runtime,
offline compatibility/record, standard-codec policy and package isolation gates
remain required. GPU-free SDL tests verify rendered full/limited endpoints and
BT.709 chromatic values. `--live-range-test` exercises both real compatibility
fixtures through the runtime decoder, metadata propagation and transition recovery;
unavailable hosted Vulkan is an explicit skip, not live interoperability evidence.

### Original P1a implementation build evidence (2026-10-01 UTC)

Tested code commit: `be884ce1564c9651080ff991f82228d859617f80`.
The subsequent documentation commit records these results without changing code.

| Gate | Result and evidence |
| --- | --- |
| Upstream/candidate baseline, x64 and ARM64 | All four jobs PASS in [run 36818828101](https://github.com/Unitron07/Asteria-Windows/actions/runs/36818828101); ordinary H.264/HEVC/AV1 builds and packaging retained |
| Optional x64/native ARM64 | Both jobs PASS in [run 36818827871](https://github.com/Unitron07/Asteria-Windows/actions/runs/36818827871), including full live client builds, qmake probe, loader/API tests, parser/legacy/presentation regressions and package gates |
| Native ARM64 dependency | `Hostarm64/arm64` MSVC; the original unpatched Granite failure is retained, then the existing portable math compatibility patch builds and its tests PASS |
| GPU-free tests | Five parser/policy/presentation tests and nine optional offline tests PASS on each target; existing 150 cases + 20,000 mutations and legacy 34 cases + 10,000 mutations retained |
| P0.5 hosted lifecycle | Raw I420 hidden lifecycle PASS on both targets; codec roundtrip/recreation record exit 77 for unavailable Vulkan device. These skips do not replace prior owner P0-R/P0.5 qualification |
| Independent package inspection | Experimental x64: 73 PE files, ARM64: 71; ordinary x64: 69, ARM64: 68. Every file has its target machine type; client imports contain no startup codec/Vulkan dependency; ordinary packages contain no runtime/manifest |
| Provenance and state | Downloaded artifact digests, inner package hashes, DLL metadata/hashes and CI architecture reports match; notices and native runtime/CRT closure included. `git diff --check` passes; recursive gitlinks remain pinned and clean |

Owner packages are available in the optional run above. Extract the artifact's
inner ZIP and follow `RUN-ME.md`; the package includes the log collector.

| Package | Inner ZIP SHA-256 |
| --- | --- |
| `Asteria-P1a-Experimental-x64-36818827871` | `99e3eec1601a5c7d7cf767adfb0180e5a01ae8a278bc02afd8c8e32d028a83e7` |
| `Asteria-P1a-Experimental-arm64-36818827871` | `fad91b4afeead1c3247c5a4af5e8ad061277386563ee0dcf65aea6ca4d0066bc` |

Runtime negatives retained: missing DLL, relative path, missing exports, wrong
API and repeated cleanup. Vulkan absence/device/decoder failure is handled by
the same runtime wrapper; unavailable hosted Vulkan is a recorded skip, while
failures after device availability fail optional CI. Live SDL preflight and
authenticated host behavior require the owner tests; no hosted GPU test proves
Vibepollo interoperability, audio/input, true device loss or visual correctness.

Manual failure/retry does not relaunch a host app automatically. Deferred scope:
live records/loss metadata/critical counts, adaptive FEC, partial/sideband decode,
bandwidth probing, 444/HDR and advanced pacing. See the
[current implementation contract](PYROWAVE_VIBEPOLLO.md).


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

## M1B P0-R Vibepollo validation

Active codec `186f0393b77f7755953b5ecde994bb1cec2e4155`, built bitstream ID
`186f0393`, C API 0.6.0; see [current contract](PYROWAVE_VIBEPOLLO.md).
The dependency helper checks exact source/API/patched provenance. Optional CI
covers both complete-frame parsers, explicit legacy fixtures, loader, dependency,
qmake and PE/import evidence for x64 and native ARM64.

Local environment has Git/PowerShell but no MSVC/CMake/Qt. Pinned source headers
and all patch applicability/hashes were audited locally; build execution is
assigned to optional CI. The implementation at
`f01d7c34cbb9538405d08f8a8cbf8e5ad0d90a2a` passed
[P0-R run 36795604865](https://github.com/Unitron07/Asteria-Windows/actions/runs/36795604865)
on **x64 and native ARM64**: exact dependency/API/patch verification, both framing
parsers and legacy regressions, six runtime/compatibility CTests, restricted
load/reload, qmake compile/link and PE/startup-import checks. ARM64 reproduced
the known Granite C1189/C2665 failures and passed with the existing portable-math
patch. Both GPU attempts recorded device-unavailable exit 77; neither is hardware
qualification. Local baseline preflight/package-architecture/ARM64 packaging-repair
tests also passed.

| New-codec offline artifact | ZIP SHA-256 |
| --- | --- |
| [x64](https://github.com/Unitron07/Asteria-Windows/actions/runs/36795604865/artifacts/11132973864) | `3f2e0c1f9bfe167f0ff3f28a2495a72d7433c022af7e544c2b54051a1f0689b2` |
| [ARM64](https://github.com/Unitron07/Asteria-Windows/actions/runs/36795604865/artifacts/11133762570) | `2687e7d04a5ab73d9874b86895d33642993c5c59f1bf81bd377c150a661fd2af` |

[Baseline run 36795605261](https://github.com/Unitron07/Asteria-Windows/actions/runs/36795605261)
**passed all four ordinary upstream/Asteria x64/ARM64 builds** at the same implementation.
The following documentation-only evidence update does not alter tested code.

### P0-R real-hardware requalification COMPLETE

Owner-supplied results qualify the exact codec above, bitstream ID `186f0393`
and API 0.6.0 on **both targets**. Restricted `--load`, required exports and
unload/reload pass. Combined `--roundtrip` and both individual framing commands
pass. Both formats decode 1920x1080 I420 with plane bytes
**2,073,600 / 518,400 / 518,400**, matching content through cycles **0, 1, 2**.

| Target | Vulkan adapter | vendorID | deviceID | driverVersion (raw) | apiVersion (raw) | MAE, each format/cycle |
| --- | --- | --- | --- | --- | --- | --- |
| Windows x64 | NVIDIA GeForce RTX 4070 Ti | 4318 | 10114 | 2585198592 | 4211039 | 0.000694444 |
| Surface Pro 11th Edition / Snapdragon X Plus / native Windows ARM64 | Qualcomm(R) Adreno(TM) X1-85 GPU | 20803 | 909329200 | 2151112704 | 4210983 | 0.00104167 |

On each machine, compatibility framing is **60,352 bytes** and record framing
is **60,076 bytes**. Intentional malformed compatibility input reports
`truncated packet data or pathological length`; record input reports
`block runs off frame`. Each is rejected and the following valid decode recovers.
Final result on both: `PASS: known CPU pixel buffer copied; SDL IYUV-compatible
(no SDL window/pacing or network interoperability test)`.

The x64 runtime was `./pyrowave/x64/install/bin`; ARM64 used
`./pyrowave-patched/arm64/install/bin`. These owner results, alongside the pinned
CI artifact provenance above, complete P0-R. Exact Windows build, human-readable
display-driver package version, and new owner runtime/output hashes were not
provided; none are inferred from the raw Vulkan values.

This qualifies codec/framing/API loading, Vulkan device/GPU decode, I420 output,
three decoder lifetimes and malformed rejection/recovery. It does not qualify
SDL display, color/range/chroma, scaling, pacing, resize, true device loss, 4:4:4,
HDR, RTSP/SDP or live Vibepollo interoperability.

## M1B P0.5 offline SDL qualification

The isolated probe implements raw and both-framing I420 presentation, deterministic
SDR BT.709 limited patterns, aspect-fit resize, nearest/linear controls, ten full
lifecycles, reset-event recovery, synthetic resource recreation, SHA-256/log
evidence and optional 60 FPS observation. See [manual commands and expected
visuals](../tests/pyrowave/README.md#p05-offline-sdl-presentation).

### Pre-PR implementation evidence

Tested code commit: `3ef29a3e44d98641dde73d5b36623d263e967ec0`.
[Optional run 36802307090](https://github.com/Unitron07/Asteria-Windows/actions/runs/36802307090)
**passed on x64 and native ARM64**: three parser/pattern CTests, seven full probe
CTests, exact dependency build, CMake and Qt/qmake SDL harness builds, loader
load/reload and PE/import checks. Both runners completed ten raw SDL lifecycles
with Direct3D11, SDL 2.32.70, IYUV support, reset/resource recreation and shutdown.
Both hosted codec roundtrip/presentation attempts explicitly skipped unavailable
Vulkan devices (`-5`, exit 77); these do not qualify GPU decode or visual output.
ARM64 retained the documented portable-math retry after C1189/C2665.

| P0.5 artifact (downloaded and hash-verified) | ZIP SHA-256 |
| --- | --- |
| [x64](https://github.com/Unitron07/Asteria-Windows/actions/runs/36802307090/artifacts/11135874197) | `1610bb6e441d46116e5ce962bf6e7b77c63d0d3db4dc0e103643e46f81caace8` |
| [ARM64](https://github.com/Unitron07/Asteria-Windows/actions/runs/36802307090/artifacts/11136097994) | `999527438faa2b38e5a4ffe8e7954b4367c37d50019c429382c03ef9feb083b0` |

The staged ARM64 executable, SDL2/SDL3 and codec DLLs were independently checked
as PE `0xAA64`. [Baseline run 36802307336](https://github.com/Unitron07/Asteria-Windows/actions/runs/36802307336)
**passed all four unchanged upstream/candidate x64/ARM64 jobs**. Both candidate
portable ZIPs were downloaded, hashes checked and contents inspected: x64 has
335 entries / 69 native PE files; ARM64 has 334 entries / 68 native PE files;
**neither contains PyroWave content**. Local preflight, package-architecture and
ARM64 packaging-repair tests pass. Diff/scope checks confirm no Session/common-c,
normal app/build/packaging or baseline workflow change. The final evidence update
changes documentation only; the tested compiled code above is unchanged.

Additional automatic **hidden API checks** ran locally on native Windows x64
(`Windows NT 10.0.26300.0`, OS/process architecture X64) and the RTX 4070 Ti,
with the same raw Vulkan identifiers recorded for P0-R. All load/roundtrip modes,
raw/compatibility/record presentation entries and ten full codec/SDL lifecycles
pass. Every pattern decodes identically in both containers; saved I420 extents
and SHA-256 match the log. SDL reports Direct3D11, flags 10, IYUV support and
1920x1080 output. A ten-second bars loop records 600 intervals, mean 16.669404 ms,
min/max 9.691900/25.920200 ms, zero counted missed intervals. These are submission
observations, **not visual inspection, refresh/latency or true device-loss evidence**.
Dense geometry checks are localized to preserve the existing packet/frame caps.

### Owner hardware qualification: P0.5 COMPLETE

[PR #20](https://github.com/Unitron07/Asteria-Windows/pull/20) merged at
`2d443c6347fd04489bda17bafceccea0bbfa4b65`. Optional x64/native ARM64 CI passed
as recorded above. The owner subsequently completed **manual visual qualification
on both named targets**. These records are separate from the earlier hidden API
checks and historical P0/P0-R decode results.

| Owner qualification | Windows x64 | Native Windows ARM64 |
| --- | --- | --- |
| Hardware | NVIDIA GeForce RTX 4070 Ti | Surface Pro 11th Edition / Snapdragon X Plus / Qualcomm Adreno X1-85 |
| SDL compiled/runtime | 2.32.70 / 2.32.70 | 2.32.70 / 2.32.70 |
| Presentation | Direct3D11; SDL_PIXELFORMAT_IYUV; 1920x1080 source; aspect-preserving fit; nearest/linear exercised | Direct3D11; SDL_PIXELFORMAT_IYUV; 1920x1080 source; aspect-preserving fit; nearest/linear exercised |
| Modes/patterns | Raw I420 and PyroWave compatibility/record presentation; all five patterns PASS; both framings and framing equality PASS | Raw I420 and PyroWave compatibility/record presentation; all five patterns PASS; both framings/framing equality PASS with identical decoded results |
| Lifecycle/recovery | Ten cycles PASS; SDL_RENDER_TARGETS_RESET and SDL_RENDER_DEVICE_RESET paths exercised; synthetic full renderer/decoder/runtime recreation PASS | Cycles 0–9 PASS; both SDL reset paths exercised; synthetic full renderer/decoder/runtime recreation PASS |
| Owner visual observation | All patterns looked correct; no obvious color/range/chroma issue, corruption or unexpected stretching/cropping; resize/maximize/fullscreen normal; recreation visually clean | All visuals looked good; no weird behavior, visible chroma issue or corruption; resize/fullscreen/scaling clean; recreation visually clean |
| Visual result | **PASS** | **PASS** |

ARM64 Vulkan adapter: `Qualcomm(R) Adreno(TM) X1-85 GPU`, `vendorID=20803`,
`deviceID=909329200`, `driverVersion=2151112704`, `apiVersion=4210983`.
These are raw Vulkan integers, not a display-driver marketing version.

Representative decoded sample errors (MAE / maximum error); record framing
matched compatibility framing:

| Pattern | x64 MAE | x64 maxError | ARM64 MAE | ARM64 maxError |
| --- | --- | --- | --- | --- |
| Range | 0.000000 | 0 | 0.000000 | 0 |
| BT.709 bars | 0.000000 | 0 | 0.000000 | 0 |
| Centered chroma | 0.004776 | 11 | 0.003041 | 11 |
| Geometry | 0.000028 | 1 | 0.000002 | 1 |
| Gradient | 0.000694 | 1 | 0.001042 | 1 |

| 30-second 60 FPS pacing sanity | x64 | Native ARM64 |
| --- | --- | --- |
| Intervals | 1799 | 1800 |
| Average ms | 16.679525 | 16.667838 |
| Minimum ms | 14.439500 | 5.625200 |
| Maximum ms | 36.244300 | 27.692300 |
| Missed intervals | 1 | 0 |
| Total duration seconds | Not supplied | 30.571586 |
| Present-loop seconds | Not supplied | 30.002108 |
| Sanity result | PASS | PASS |

**Qualified scope:** offline SDL IYUV presentation of 1920x1080 SDR 8-bit 4:2:0,
BT.709 limited-range visual patterns; raw I420, compatibility and record framing;
aspect fit, resize, nearest/linear, fullscreen/maximize/restore, renderer/texture
recreation and synthetic reset/recovery paths on these two devices only.

**Limits:** no visible chroma anomaly was observed on either target. Vibepollo
authors CENTER 4:2:0 chroma, while the pinned SDL2-compat BT.709 limited path maps
to SDL3 LEFT chroma-location metadata. SDL2 has no independent siting control;
exact CENTER-versus-LEFT sampling remains **formally unqualified**. Visual
inspection and sample errors do not mathematically prove center-sample preservation.
Recovery logic was exercised successfully, but no forced physical GPU device-loss
event was induced: true physical device-loss qualification remains manual/unproven.
The loops show no catastrophic offline SDL presentation issue; they do not measure
refresh accuracy, full frame pacing, production latency or end-to-end latency.
Leak freedom is not established. No OS build, display-driver marketing version,
exact display refresh, color calibration or chroma-phase measurement was supplied
for these owner visual runs; the prior hidden API OS record is separate.

P0.5 does not validate live RTP/UDP, RTSP/SDP/SCM negotiation, live Vibepollo host
interoperability, adaptive FEC, partial-frame recovery, bandwidth probing, 4:4:4
or HDR. Ordinary codecs, Session/common-c, runtime behavior and release packaging
are unchanged. **P0-R remains COMPLETE; P1a live SDR 4:2:0 integration is the next
future milestone**, and P1b transport hardening is later; neither is implemented
by this documentation update.

## M1B P0 offline PyroWave validation

**Historical: this section records the superseded f6fb84eb0d8538f43f6f54e58d2040d101c8676c codec and private PYRW wrapper. It does not qualify P0-R.**

**Complete: offline dependency/runtime/parser/decode proof.** The source-diff
groundwork from PR #14 and the P0 implementation merged in
[PR #16](https://github.com/Unitron07/Asteria-Windows/pull/16) are complete. Final
P0 head before merge: `a02902fb58ac66ae4820373dd3978d3087de7d24`. This post-release
experiment is separate from v0.1.0-preview.1 at
`34dfd937528586babded200fafaee535e21d4a40` and changes no normal release behavior.

| Final CI evidence at the P0 head | Result |
| --- | --- |
| [Optional PyroWave P0 run 36515183774](https://github.com/Unitron07/Asteria-Windows/actions/runs/36515183774) | Pass: x64 and native ARM64 dependency builds, parser/runtime/compatibility tests, load/reload, PE/import checks, and qmake probes |
| [Baseline push run 36515181039](https://github.com/Unitron07/Asteria-Windows/actions/runs/36515181039) | Pass |
| [Baseline PR run 36515184040](https://github.com/Unitron07/Asteria-Windows/actions/runs/36515184040) | Pass |

Hosted build/test success is distinct from GPU qualification: earlier hosted
roundtrips explicitly recorded unavailable Vulkan (exit 77). The real-hardware
GPU results below supply the offline decode evidence.

| Offline hardware result | Windows x64 | Native Windows ARM64 |
| --- | --- | --- |
| Device / GPU | Windows 11 build 26200 / NVIDIA RTX 4070 Ti | Surface Pro 11th Edition / Snapdragon X Plus / Qualcomm Adreno X1-85 |
| Runtime artifact | Verified x64 artifact from run 36514467221 | Native ARM64 CI artifact, `probe-arm64/Release` and `pyrowave-patched/arm64/install/bin` |
| Restricted runtime load | API 0.6.0; load/reload passed | API 0.6.0; required exports and load/reload passed |
| Vulkan device | NVIDIA adapter/device creation succeeded | Qualcomm Adreno X1-85 adapter/device creation succeeded |
| Generated PYRW frame | 60,312 bytes | 60,312 bytes |
| Decoded output | 1920×1080, 8-bit SDR 4:2:0 I420 | 1920×1080, 8-bit SDR 4:2:0 I420 |
| Plane sizes | Y = 2,073,600; U = 518,400; V = 518,400 bytes | Y = 2,073,600; U = 518,400; V = 518,400 bytes |
| Mean absolute sample error | **0.000694444** | **0.00104167** |
| Lifetimes and malformed input | Three decoder cycles; rejection and recovery passed | Three decoder cycles; rejection and recovery passed |
| Presentation | Known CPU buffer copied; no SDL window/pacing test | Known CPU buffer copied; no SDL window/pacing test |

The ARM64 owner-reported adapter record is `vendorID=20803`,
`deviceID=909329200`, `driverVersion=2151112704`, `apiVersion=4210983` (raw
Vulkan integers). The exact commands from the extracted CI artifact root were:

```powershell
.\probe-arm64\Release\pyrowave-offline-proof.exe --load ".\pyrowave-patched\arm64\install\bin"
.\probe-arm64\Release\pyrowave-offline-proof.exe --roundtrip ".\pyrowave-patched\arm64\install\bin" ".\roundtrip-output"
```

Cycles 0, 1, and 2 each reported the same frame size, extent, plane sizes, and
ARM64 MAE above. Each `PyroWave P0: nonzero reserved byte` diagnostic was an
intentional malformed-frame rejection, followed by successful recovery; these
lines are not failures. Final result:
`PASS: known CPU pixel buffer copied; SDL IYUV-compatible (no SDL window/pacing test)`.
The native ARM64 artifact's successful GPU decode validates this Snapdragon/
Adreno hardware, not merely an ARM64 build or loader probe. ARM64 OS build,
Windows display-driver version, artifact hash, and generated output hashes
were not supplied; no values are inferred from the x64 record.

At the historical P0 revision the original codec fork returned 404. That helper preferred
[the original source](https://github.com/joemossjr16/pyrowave) at exact commit
`f6fb84eb0d8538f43f6f54e58d2040d101c8676c`, then uses the verified Git bundle
with SHA-256
`e4387ce6b691724aa342d6df7677f51efffe30e98e3949415718e7b2b955c56e`.
This is a clean audited source-history/content snapshot preserving license and
provenance, not a built codec binary. Optional CI checks the expected commit
and checksum; moving HEAD or a different revision is not an accepted substitute.
See [bundle provenance](../scripts/pyrowave/README.md) and the
[spike evidence](M1B_PYROWAVE_SPIKE.md) for all dependency pins, toolchain,
patch/import inventory, and x64 hashes.

**Current status beyond the historical P0 record:** P0-R new-codec hardware
requalification and P0.5 offline SDL presentation qualification are complete on
both named targets; see their separate records above. PyroWave remains experimental
and off by default; normal releases do not ship active PyroWave streaming support.
Exact chroma siting, physical GPU device loss, full pacing, 4:4:4, HDR,
production/end-to-end latency and final runtime shipping/package policy remain
unqualified. Live SCM/RTSP/SDP integration is not implemented and no live
Vibepollo end-to-end PyroWave stream has been validated. The next future milestone
is [P1a live SDR 4:2:0 integration](NEXT_STEP.md), with safe standard-codec fallback
and H.264/HEVC/AV1 regressions; P1b transport hardening follows later.

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
