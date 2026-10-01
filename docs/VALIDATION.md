# P0-R Vibepollo compatibility contract

## M1B P1a implementation / live qualification pending

The explicit **PyroWave (Experimental)** live path is implemented behind
`CONFIG+=pyrowave_experimental`. Live owner qualification remains **PENDING**;
build/test evidence is recorded in [VALIDATION.md](VALIDATION.md). The implementation
is **COMPLETE / READY FOR HARDWARE INTEROPERABILITY TEST**: both optional client
builds and baseline CI pass as recorded there. P0-R and P0.5 qualification history is unchanged.

The contract is Vibepollo at `8a8c4b03a280ab9f567beb380110abb80f5220b8`, codec
`186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, C API 0.6.0.
Only SDR 8-bit 4:2:0 is advertised: `SCM_PYROWAVE=0x00800000`, client format
`0x010000`, exact DESCRIBE `a=rtpmap:99 PYROWAVE/90000` and exactly one valid,
matching `a=x-ss-pyrowave.bitstream:186f0393`; ANNOUNCE uses `bitStreamFormat=3`.
No record-feature or adaptive-FEC attribute is sent, preserving compatibility framing.

Session tests restricted runtime loading, required exports/API, packaged build
metadata/DLL SHA-256, Vulkan device/decoder creation and a real hidden SDL IYUV
upload/presentation before launch. Host capability is rechecked using paired,
certificate-pinned HTTPS without HTTP fallback before sending launch/resume.
PyroWave has its own `IVideoDecoder`, never FFmpeg: complete common-c decode unit
→ LE compatibility envelope/codec-record validation → individual codec packets
→ full readiness → CPU I420 → main-thread SDL presentation. Audio/input and
ordinary Session cleanup remain on existing Moonlight paths.

Auto and standard codec candidate ordering are unchanged. HDR, 4:4:4 and forced
software decode reject PyroWave attempts. Missing/mismatched IDs abort DESCRIBE.
Malformed independent frames clear decoder state and can recover on the next frame;
runtime/presentation failure ends the attempt. Retry is manual: choose a standard
codec and reconnect. No hot switch or automatic host launch/resume replay occurs.

The common-c gitlink remains `f900dd4767759c7b9d0e93bcea666b55c69ea62f`.
`scripts/pyrowave/common-c-p1a.patch` is a maintained, separately reviewable delta
to Limelight constants, strict SDP validation, ANNOUNCE and opaque-picture
validation. qmake verifies the pin and applies it idempotently; the explicit
PowerShell helper does the same for CMake tests. The submodule is not flattened.
Reserved host HDR/444 bits are consumed by Session's SCM mapping with zero client
formats; standard HDR/444 masks are unchanged. SCM and VIDEO_FORMAT namespaces
are never interchanged.

Live bounds: even 128..4096 dimensions, at most 3840×2160 pixels; 8 MiB envelopes,
65,536 codec packets, at most 4,000 transport fragments and 1024..2048-byte transport
packet sizes. The byte bound covers Vibepollo's 4,000-packet complete-frame budget
at the largest permitted MTU, including compatibility overhead. Codec packet
count is independent of RTP count. Output is at most 12,441,600 CPU bytes/frame;
one replaceable pending image bounds the render queue. Offline limits remain
850,000 bytes / 1,024 packets. Only 1080p has prior offline hardware qualification.

The first owner P1a test on Surface Pro 11 reached live video transport:
negotiation, pinned runtime/API, Vulkan decoder on Adreno X1-85, SDL I420
initialization, audio and live video packet receipt succeeded. Every frame was
rejected because Asteria incorrectly required sequence bit 30 to indicate limited
range; Vibepollo sent valid BT.709 full-range SDR 4:2:0 frames. The range fix
accepts both full and limited SDR 8-bit 4:2:0 without relaxing framing, dimensions,
chroma or SDR checks. BT.2020, PQ/HDR and 4:4:4 remain rejected.

Sequence range is parsed into `YuvRange` and carried with decoded I420. The first
valid live sequence establishes the range for that decoder lifetime; changes
are rejected before codec packet submission with a reconnect diagnostic. Malformed
frames cannot establish range. SDL3 texture properties explicitly select
`SDL_COLORSPACE_BT709_FULL` or `SDL_COLORSPACE_BT709_LIMITED` on the pinned
SDL2-compat renderer, without changing global conversion mode or FFmpeg rendering.
The preflight black image does not establish live range. P1a owner qualification
remains **PENDING** until a new hardware retest confirms decoded/rendered live video.

Presentation retains aspect fit and linear scaling. Source chroma
is CENTER; the SDL3 BT.709 colorspaces use LEFT. P0.5 found no visible issue on the
named targets, but exact phase remains formally unqualified. Session V-sync is
retained; advanced pacing/latency work is deferred. The existing statistics
overlay receives VIDEO_STATS-derived rates, bytes, drops, timing and RTT.

Optional CI builds separate x64 and native ARM64 experimental portable packages,
with runtime provenance, matching PE types, import/CRT closure, source notices,
an owner guide and log collection. Ordinary packages contain no PyroWave DLL,
no startup Vulkan/codec imports and no PyroWave UI option. See
[the live owner guide](../tests/pyrowave/LIVE-OWNER-TEST.md) for launch/test steps.

Deferred P1b/later: live records, record-start/lost-buffer metadata, critical
packet handling, adaptive FEC, partial recovery, sideband readiness, bandwidth
probing, bitrate usability tuning, 4:4:4, HDR and advanced pacing/latency work.

The earlier milestone descriptions below retain the P0/P0-R/P0.5 history;
future P1a statements there are superseded by the implementation above.


Nonary/Vibepollo is Asteria's primary PyroWave host target. The authoritative
[host protocol](https://github.com/Nonary/Vibepollo/blob/8a8c4b03a280ab9f567beb380110abb80f5220b8/docs/pyrowave-protocol.md)
and [vendored codec/patches](https://github.com/Nonary/Vibepollo/tree/8a8c4b03a280ab9f567beb380110abb80f5220b8/third-party/pyrowave)
were audited at `8a8c4b03a280ab9f567beb380110abb80f5220b8`.
Nonary/moonlight-qt's PyroWave branch was inspected at
`5f9ce4a46d2b8fd2191f47cef043bc43f3d772d0`, including its framing/parser tests,
vendor lock and decoder. The host protocol and pinned upstream codec remain
the primary authority; live recovery behavior is reserved for P1b.
The joemossjr16/pyrollo comparisons and old `PYRW` proof remain historical evidence.

## Independent compatibility axes

- Active upstream codec: Themaister/pyrowave
  `186f0393b77f7755953b5ecde994bb1cec2e4155`.
- Experimental built bitstream ID: `186f0393` (`PyroWave::BitstreamId`).
- C API: **0.6.0**, verified in pinned headers, at compile time, and on DLL load.
- Granite, volk and Vulkan-Headers retain their already tested revisions in
  [dependencies.json](../scripts/pyrowave/dependencies.json).

The bitstream has no version field. API 0.6.0 alone does not establish codec
compatibility. P1a reads `a=x-ss-pyrowave.bitstream:...` and compares it
with the local build ID and refuses a mismatch. Runtime load checks the
API and exports; it cannot discover a DLL's source commit. Deployments must bind
the DLL to the build's source/patch/inventory hashes.

The historical P0-R scope was **1920x1080, 8-bit SDR 4:2:0**, offline only. No Session
hooks, host advertisement, RTSP negotiation, UI choice, HDR path, bandwidth probe,
release packaging or frame-pacing change was included. H.264/HEVC/AV1 retain their
existing behavior. Complete local roundtrip does not prove network interoperability.

## Patch decisions

The lock file records immutable Vibepollo provenance and SHA-256 for all three
verbatim patches (LF endings). The dependency helper checks every hash and
`git apply --check` against the exact upstream revision on both architectures.
It archives unmodified source hashes, applied diffs, modified-file hashes and
patch inventory; retained patches ship in experimental source notices.

| Patch | Build decision | Effect |
| --- | --- | --- |
| 0001 encoder buffer pool | Retained, not applied | Encoder allocation/performance only; unnecessary for a single-frame fixture |
| 0002 4:4:4 payload sizing | Retained, not applied | Encoder buffer safety for 4:4:4; apply before future 4:4:4 fixture work |
| 0003 decoder short-block rejection | Applied x64 + ARM64 | Decoder safety: prevents non-advancing parse loops on malformed duplicate blocks |

These local patches preserve the bitstream. Encoder-only fixes do not qualify
4:4:4 client decode or presentation. The separate Granite MSVC ARM64 portable
math patch remains architecture-scoped; see [COMPATIBILITY.md](../scripts/pyrowave/COMPATIBILITY.md).
The old `f6fb84...` source bundle is retained for history and never used by the
active build helper. Failed exact fetch or revision mismatch fails the build.

## Complete-frame parsing

`parseFrame` selects framing from bit 31 of the first LE word: set means records;
clear means compatibility packet count. Invalid input is rejected deterministically;
it never retries the legacy wrapper. `parseLegacyOfflineFrame` is only an explicitly
called historical fixture helper.

Compatibility format:

```text
[u32 LE packet_count] { [u32 LE size] [size bytes] } * packet_count
```

`parseCompatibilityFrame` validates the envelope/count/lengths and returns packet
offsets into immutable caller-owned bytes, without copying payload. Supplying a
`StreamContext` to `parseFrame` additionally validates the codec records in those
packets before any decoder call. Codec packet boundaries differ from RTP boundaries:
the pinned packetizer keeps each record intact, using a 1024-byte packing target.

R…3904 tokens truncated…ed. The first portable preview covers inherited streaming and Asteria identity.

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
