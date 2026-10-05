# Source audit and feature matrix

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

## Historical source audit

Earlier future/PENDING PyroWave statements below describe the inspected snapshots;
the current state above supersedes them.

Source review: September 12, 2026. Findings below describe the inspected snapshots. Subsequent x64 builds passed and the owner confirmed the client works; those results and their limits are recorded in [BASELINE.md](BASELINE.md). Current Asteria identity and x64/ARM64 CI packaging are implemented; M0A real-device native ARM64 validation and M1B P0-R Vibepollo-compatible x64/ARM64 hardware qualification are complete; [P0.5 offline SDL visual qualification](VALIDATION.md#m1b-p05-offline-sdl-qualification) is complete on both named targets; [P1a](NEXT_STEP.md) live SDR 4:2:0 is now implemented and owner-validated on the named ARM64 target, with broader qualification open; initial M1A comparisons use Moonlight's existing statistics. This historical source audit is not a list of shipped Apollo extensions; see the [README](../README.md) for current preview scope.

## Comparison snapshots

| Repository | Reviewed commit | Purpose |
| --- | --- | --- |
| [MobinYengejehi/Artemis](https://github.com/MobinYengejehi/Artemis/commit/42eb11abf8954aa11b24900e2d5ae670f94d43b6) | `42eb11abf8954aa11b24900e2d5ae670f94d43b6` | User-selected Android behavior reference |
| [moonlight-stream/moonlight-qt](https://github.com/moonlight-stream/moonlight-qt/commit/e3fd29e4d7dc5723d8d0da7d19e2698daec74456) | `e3fd29e4d7dc5723d8d0da7d19e2698daec74456` | Windows application foundation |
| [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo/commit/adc5c5a0bd80831ce495434bb16aee2cd4175fb8) | `adc5c5a0bd80831ce495434bb16aee2cd4175fb8` | Extension implementation reference |

The selected Android snapshot dates to February 3, 2025. It is the requested fork; do not silently substitute another Artemis branch or assume it matches today's host behavior. These are audit pins, not qualified release versions.

Native protocol pins differ:

- Android: `ClassicOldSong/moonlight-common-c` at `40bec19cc1b2fd0a2fcc413e28d5a09377af846d`.
- PC: `moonlight-stream/moonlight-common-c` at `62e066388f1a1b133e0bee947b9a374311a3354b`.

The parent repositories' gitlinks establish those SHAs; their [.gitmodules (Android)](https://github.com/MobinYengejehi/Artemis/blob/42eb11abf8954aa11b24900e2d5ae670f94d43b6/.gitmodules) and [.gitmodules (PC)](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/.gitmodules) identify the different repositories. Review the native delta before porting server commands.

## Feature disposition

“Retain” means present in upstream source, still requiring Windows runtime validation. “Extend” means preserve that implementation and add only the identified workflow gap. “Port” means new integration work. No row claims completed Asteria support.

| Feature | Disposition | Windows work and evidence |
| --- | --- | --- |
| Discovery, pairing, app list, streaming, audio | Retain | Baseline acceptance tests; existing PC backend/session |
| Custom resolution, FPS, bitrate | Extend | Existing controls; add profiles/validation instead of duplicating them [S1, S2] |
| Hardware decode, H.264/HEVC/AV1, HDR, YUV 4:4:4 | Retain | Qualify hardware/host combinations; 4:4:4 is useful for office text [S3] |
| Captured/direct pointer, keyboard shortcut forwarding | Extend | Existing modes; focus/capture reliability, discoverability and configurable session actions [S2, S3, S4] |
| Gamepad, rumble, motion, multitouch | Retain/validate | Device-specific support already exists upstream; add gaps only after testing [S3, S4] |
| Fit/fill/stretch and pan/zoom | Audit then extend | Compare renderer behavior and pointer transforms; defer pan/zoom from first preview [S5] |
| Performance display | Extend only if needed | Preserve existing statistics; add useful capability/decoder diagnosis [S14] |
| Multi-monitor, high DPI, portrait use | Validate then extend | Start with one stream moved between local monitors; distinguish this from simultaneous host-monitor streams |
| Session back/quit menu and custom shortcuts | Extend | Separate disconnect, remote app quit, local close, and capture release [S5] |
| Apollo text clipboard | Deprioritized (historical M3) | Paired HTTPS, separate read/write permissions, active-session restriction [S6, S7] |
| Apollo virtual display and scale factor | Deprioritized (historical M3) | Host capability/readiness and launch/resume contract; host owns driver/lifecycle [S6, S7] |
| Apollo server commands | Deprioritized (historical M4) | Command list from HTTPS; native control-channel dispatch [S6, S8, S9] |
| Virtual buttons/layout import/export, touchpad overlay | Defer | Windows touch-device follow-up; investigate Android layout schema before claiming import compatibility [S5] |
| Android soft keyboard, DeX, foldable positioning, device vibration fallback | Omit direct port | Use Windows equivalents only when an actual requirement exists [S5] |
| File transfer, simultaneous multiple streams | Out of first release | Not implied by clipboard support or local monitor selection |
| Isolated sessions / MultiSeat | Plan after performance (historical M7) | Primarily host-side sessions/VMs, virtual display/audio and isolated input; paired client controls, status and streaming |
| Asteria VR (remote PCVR) | Plan after isolated sessions (historical M6) | New PC-to-PC SteamVR driver/protocol and Windows client headset path; no shipped support or qualification is established by this source audit |

The active order is measured codec/performance work, post-v0.2.0 native Vulkan
PyroWave presentation, isolated sessions / MultiSeat, then VR. The Apollo contract
findings below retain technical notes for possible future supporting integrations;
M2–M4 no longer define standalone active milestones. See the [porting plan](PORTING_PLAN.md).

## Planned Asteria VR scope

This is a future design item, not a feature found in the pinned Moonlight, Artemis, or Apollo snapshots. The host PC will run SteamVR/game rendering; a VR headset will be physically connected to the Windows client PC. The proposed bidirectional path covers stereoscopic video, head/controller/tracker poses, buttons/analog inputs, haptics, audio, and microphone input, with hand/eye/full-body tracking optional later. Clock synchronization, pose prediction, motion-to-photon latency, and client-side reprojection/timewarp need proof-of-concept measurements before compatibility claims.

Keep the headset backend generic. PSVR2 with a PC adapter could be one qualified local headset configuration, but the phone/wearable PSVR2 wireless-adapter project is separate. M6 may reuse proven codec/transport work, but does not depend on M1B PyroWave. See the [architecture](ARCHITECTURE.md) and [porting plan](PORTING_PLAN.md) for boundaries and staged gates.

## Apollo contract findings

### Discovery and permissions

Android reads `Permission`, `VirtualDisplayCapable`, `VirtualDisplayDriverReady`, and repeated `ServerCommand` elements. The reviewed Apollo host emits extension information in paired HTTPS server information; an unauthenticated response is not authoritative [S6, S7].

Use per-feature detection, not a hostname or product-string check. There is no generic extension-version handshake established by this audit. In particular, permission bits alone do not prove that a clipboard endpoint behaves as expected. Keep unknown support separate from unsupported and permission-denied states, and verify a known contract before enabling automatic actions.

### Clipboard is authenticated HTTP

The Android path uses `GET /actions/clipboard?type=text` to read and `POST` to the same path with a plain-text body to write. The reviewed host checks the paired client, view-related permission, the relevant read/write permission, and whether that client is connected to a stream. It distinguishes unauthorized access, inactive sessions, unsupported types, and write failures [S6, S7].

The Android implementation notes that Sunshine may return an HTTP-200 response containing an error for an unknown endpoint. The port must validate the response contract, not just HTTP status, and must not put an error document into the clipboard. Resolve ambiguous responses conservatively during the protocol spike. Never downgrade extension requests to unauthenticated HTTP.

### Virtual display belongs to the host

Android appends `virtualDisplay` and `scaleFactor` to session launch/resume requests. The reviewed Apollo source reads the display request and exposes readiness fields [S6, S7]. This supports a client request interface; it does not justify a client-side host display driver or an assumption that every Apollo version implements the same lifecycle. Scale-factor behavior and units need a focused host-code/runtime check before exposure.

### Server commands require a native patch

Android routes `sendExecServerCmd` through `NvConnection` and JNI to its native fork. That fork's `LiSendExecServerCmd(uint8_t)` sends the Apollo `0x3000` control message with a four-byte payload whose first byte is the command index. The host checks command permission and looks up that index in its configured command list [S8, S9].

The inspected PC core header has no `LiSendExecServerCmd` API [S10]. Port the smallest compatible extension into the selected PC core; do not replace it with the older Android fork. Validate transport/encryption behavior and command-list ordering. Indexes must fit 0–255 and the actual advertised list. The inspected host handler supplies no command-completion response, so the UI should say “sent” rather than assert success.

## Build findings

The inspected PC branch uses Qt/QML and qmake projects. Its Windows workflow selects Qt 6.11.2, the `windows-2025` runner, and MSVC-named Qt kits, including an ARM64 cross-compiled kit. Its README names Visual Studio 2026, while the kit paths retain `msvc2022` naming [S3, S11]. The Asteria harness records the actual selected Visual Studio/SDK/compiler for both targets. PR #6 added upstream/candidate x64 and ARM64 CI; all four jobs passed in the run linked from [BASELINE.md](BASELINE.md). The new package architecture gate and outstanding device qualification are tracked there separately from this source audit.

Upstream `setup-deps.ps1` downloads the `v15` Windows dependency archives. The M0A harness now pins and independently verifies both x64 and ARM64 archives; see [dependency notes](DEPENDENCIES_WINDOWS.md). The input code contains SDL2 and sdl2-compat/SDL3 handling, so record the actual deployed runtime for each architecture; avoid a blanket SDL-major-version assumption [S4, S12]. Existing Windows build scripts already package portable artifacts and installer components [S13].

## Sources

- S1: [PC settings UI](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/app/gui/SettingsView.qml)
- S2: [PC streaming preferences](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/app/settings/streamingpreferences.h)
- S3: [PC features and build requirements](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/README.md)
- S4: [PC input handling](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/app/streaming/input/input.cpp)
- S5: [Android feature list](https://github.com/MobinYengejehi/Artemis/blob/42eb11abf8954aa11b24900e2d5ae670f94d43b6/README.md) and [Android session behavior](https://github.com/MobinYengejehi/Artemis/blob/42eb11abf8954aa11b24900e2d5ae670f94d43b6/app/src/main/java/com/limelight/Game.java)
- S6: [Android host HTTP and extension parsing](https://github.com/MobinYengejehi/Artemis/blob/42eb11abf8954aa11b24900e2d5ae670f94d43b6/app/src/main/java/com/limelight/nvstream/http/NvHTTP.java)
- S7: [Apollo host HTTP implementation](https://github.com/ClassicOldSong/Apollo/blob/adc5c5a0bd80831ce495434bb16aee2cd4175fb8/src/nvhttp.cpp)
- S8: [Android native dispatch](https://github.com/MobinYengejehi/Artemis/blob/42eb11abf8954aa11b24900e2d5ae670f94d43b6/app/src/main/java/com/limelight/nvstream/NvConnection.java) and [native control implementation](https://github.com/ClassicOldSong/moonlight-common-c/blob/40bec19cc1b2fd0a2fcc413e28d5a09377af846d/src/ControlStream.c)
- S9: [Apollo control-channel command handler](https://github.com/ClassicOldSong/Apollo/blob/adc5c5a0bd80831ce495434bb16aee2cd4175fb8/src/stream.cpp)
- S10: [PC pinned native API](https://github.com/moonlight-stream/moonlight-common-c/blob/62e066388f1a1b133e0bee947b9a374311a3354b/src/Limelight.h)
- S11: [PC Windows/macOS CI](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/.github/workflows/build-win-mac.yml)
- S12: [PC Windows dependency setup](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/setup-deps.ps1)
- S14: [PC session and performance overlay](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/app/streaming/session.cpp)
- S13: [PC Windows build/package script](https://github.com/moonlight-stream/moonlight-qt/blob/e3fd29e4d7dc5723d8d0da7d19e2698daec74456/scripts/build-arch.bat)

## Host-dependent roadmap boundaries

Asteria-Windows is the client repository. Isolated sessions and remote PCVR require
substantial host-side work in a separately maintained Asteria-oriented Vibepollo
fork/host extension, outside this repository. Neither feature is implemented here.
The active order is native Vulkan presentation, isolated sessions, then VR.

### Isolated sessions / MultiSeat (historical M7)

The concept is one physical host PC remaining usable locally while a remote user
receives an isolated desktop/session. Candidate backends include separate Windows
sessions/multiseat or a VM, virtual displays, virtual audio, and isolated keyboard,
mouse and gamepad routing. Most implementation belongs on the host.

| Asteria client | Vibepollo fork / host extension |
| --- | --- |
| Session-mode selection, UI/control requests and session state/status | Session/VM creation and cleanup/resource management |
| Stream reception and isolated input transport | Virtual display/audio lifecycle and isolated input routing |
| Reconnect/disconnect UX | Application/session launch and stream binding |

There is no equal-performance promise. Native-class performance is a possible
target only when hardware headroom permits; CPU/GPU contention determines results.

### Asteria VR responsibilities (historical M6)

Remote PCVR follows isolated-session work because host-control and session-lifecycle
infrastructure may be reusable; this ordering is not a hard technical dependency.
The host extension would own SteamVR/OpenXR and virtual HMD/runtime integration,
pose/controller ingestion, stereo frame capture, timing metadata and session
lifecycle. Asteria would own local headset/runtime integration, pose/controller
capture, upstream tracking/input transport, stereo decode, headset presentation
and timing/reprojection.

PSVR2 with its PC adapter is one possible future local headset configuration;
the design remains headset-agnostic. PyroWave may be evaluated later for VR but
is not a prerequisite.
