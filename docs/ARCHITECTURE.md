# Architecture

## M1B P1a implementation / live qualification pending

The explicit experimental PyroWave path now uses a dedicated `IVideoDecoder`
with the existing runtime/parser, common-c complete decode units and main-thread
SDL IYUV presentation. Session gates host launch on runtime/API/provenance,
Vulkan/device/decoder and SDL preflight; paired pinned HTTPS SCM plus strict
DESCRIBE/bitstream-ID checks gate negotiation. Audio/input remain on existing
Moonlight paths. Auto and standard codec selection are unchanged.

Only SDR 8-bit I420 BT.709 limited is accepted. The render queue contains one
replaceable pending image; the existing statistics overlay receives decoder
rates, bytes, loss, timing and RTT. Failures clean up normally and require manual
retry with a standard codec, avoiding automatic launch/resume replay. The
maintained common-c patch keeps the upstream gitlink and submodule layout intact.
Optional x64/ARM64 packages stage runtime/CRT closure and provenance separately
from ordinary builds. See [the current contract](PYROWAVE_VIBEPOLLO.md) for bounds,
strict parsing, chroma caveat and P1b/later exclusions. Live owner qualification
is **PENDING**; build success alone does not establish interoperability.

The earlier milestone descriptions below retain the P0/P0-R/P0.5 history;
future P1a statements there are superseded by the implementation above.


## Decision: extend a Moonlight PC fork

Use Moonlight PC as the application, retaining its source history and layout. Do not build a second Windows shell or extract its streaming internals into a new framework for the first release. This follows the project owner's September 12, 2026 direction.

The [audit](FEATURE_AUDIT.md) pins the reviewed code. M0 integration is merged, x64 CI passed, and the owner confirmed the tested client works; see [BASELINE.md](BASELINE.md) for the evidence and remaining qualification records. Native x64/ARM64 builds and packaging pass CI, and the Asteria identity is implemented. M0A real-device ARM64 validation is complete; M1A begins with Moonlight's built-in statistics; M1B source-diff groundwork and historical P0 offline proof are complete on x64 and native ARM64. P0-R codec/framing requalification and P0.5 offline SDL presentation qualification are complete on the named x64/ARM64 hardware; P1a live SDR 4:2:0 integration is implemented experimentally; owner interoperability qualification is pending. Frame-pacing changes require measured evidence. The extension architecture below describes planned work.

## Native Windows targets

Use the same Qt/QML/C++ application and streaming stack for x64 and ARM64. Compile the client and every process-loaded runtime dependency for the target architecture. An x64 executable running under Windows emulation does not meet the native ARM64 goal. Cross-compilation from an x64 build host is acceptable; host-side Qt tools must not be deployed as ARM64 runtime dependencies.

Reuse upstream's ARM64 Qt/MSVC/qmake and packaging paths. Keep architecture-specific dependency manifests, output directories, symbols, and runtime PE verification while sharing feature implementation. Qualify hardware decoding, input, display behavior, and performance on actual Windows 11 ARM64 hardware. M0A owns the build and device baseline; subsequent profiles and Apollo work must preserve both targets. No Windows UI framework rewrite is required for native ARM64.

## Integration points

| Area | Existing Moonlight location | Planned change |
| --- | --- | --- |
| UI | `app/gui/` | Extend existing settings and session actions; use keyboard-accessible controls |
| Settings | `app/settings/streamingpreferences.*` | Versioned profiles and validated Asteria settings |
| Discovery, pairing, host HTTP | `app/backend/nvcomputer.*`, `nvhttp.*` | Parse Apollo fields and add a bounded authenticated clipboard request path |
| Session lifecycle | `app/streaming/session.cpp` | Attach extension services to the existing session |
| Input | `app/streaming/input/` | Extend existing capture, direct-pointer, and shortcut paths |
| Decode/render | Existing `app/streaming/` implementation | Preserve existing codecs; isolated P0 PyroWave offline GPU decode exists. P0-R is hardware-qualified on both targets; standalone P0.5 SDL presentation is qualified on both named targets; P1a explicit live integration remains future work |
| Native protocol | `moonlight-common-c/moonlight-common-c/` | Keep the current upstream pin; P1 may add the reviewed minimal PyroWave protocol delta. Server-command extensions remain later work |
| Build/package | `moonlight-qt.pro`, `app/app.pro`, `scripts/`, `wix/` | Asteria identity implemented; qualify portable packages before installer distribution |

New extension classes belong beside the existing backend/session code. Class names and exact filenames can be chosen during implementation; these are responsibilities, not a demand for a new service framework.

## Responsibilities and boundaries

**Host capabilities:** maintain a per-host snapshot of supported, unsupported, and unknown extension states; permission bits; driver readiness; and the advertised command list. Refresh through the paired HTTPS path before launch/resume and after reconnect. Missing permission fields do not disable ordinary Sunshine streaming, but must not grant extension access. Parse permission values as an unsigned bitmask; preserve unknown bits without enabling unknown actions.

**Clipboard:** use paired HTTPS and plain text only initially. Expose Send and Receive first; automatic synchronization is an explicit per-host option added after correctness tests. Bound payloads and request time, suppress echo loops, and stop pending work on disconnect or host switch. Qt clipboard access stays on the GUI thread; network work must not block the GUI, input, decode, or render loops. Apply a completed response only if it still belongs to the active session. Clipboard contents, pairing secrets, and private keys must not appear in logs.

**Host display requests:** send verified Apollo launch/resume parameters. Apollo owns its virtual-display driver, host monitor creation, mode changes, and host cleanup. The client owns local window placement, monitor selection, and only local state it actually changed. A companion driver/service is out of scope. Requested stream dimensions, local render scaling, and the host desktop mode are separate settings; changing one does not prove the others changed. Do not promise live host resolution changes until supported and tested.

**Server commands:** show the host-advertised names and send only their validated index through the native control channel. Preserve original list order even if the UI sorts labels; reject indexes outside the supported range. Refresh the list before presenting actions, and do not replay a command after reconnect or timeout. A successful send is not proof of host execution. Do not introduce arbitrary shell-text execution. Confirm actions explicitly marked disruptive; if the host supplies no usable safety classification, confirm each command.

**Input and presentation:** retain upstream SDL routing rather than running a competing XInput/raw-input pipeline. Session shortcut configuration needs conflict detection and a reliable local capture-release action. Release pressed keys/buttons on focus loss and disconnect. Fit/fill/stretch or pan/zoom changes must share a coordinate transform with direct-pointer mapping. Prefer the existing rendering path; prototype and benchmark any overlay integration before depending on a QML overlay over the video window.

**Settings and identity:** Asteria's separate app/settings/pairing/log/package identity is implemented. Qualify data-location and installer lifecycle behavior for the artifacts offered. Test coexistence with Moonlight. Do not silently copy pairing credentials. Profiles use stable host/app identifiers and explicit precedence: global defaults, host profile, app override, session-only override. Version the schema and preserve recoverable settings when migration fails. Verify whether upstream portable mode meets the intended data-location contract before promising a self-contained ZIP.

## PyroWave boundary (M1B)

P0-R targets Nonary/Vibepollo with upstream codec
`186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, C API 0.6.0.
The GPU-free parser distinguishes LE compatibility packet envelopes from complete
sequence/block/padding record frames. `PYRW` is an explicit historical fixture
helper, never detected as host framing. Immutable input offsets and record/loss
metadata structures keep framing separate from device ownership and future transport.
See [the current contract](PYROWAVE_VIBEPOLLO.md) for bounds, patches and future interfaces.

The restricted explicit-path loader preserves API/export checks, unload/reload
and no PATH/CWD search. Offline decode remains 1920x1080 8-bit SDR 4:2:0 I420.
`CONFIG+=pyrowave_experimental` is required; ordinary builds have no codec import,
startup load, runtime copy or Session hook. Existing H.264/HEVC/AV1, frame pacing
and release packaging are unchanged.

Old RTX 4070 Ti and ARM64 Surface Pro 11 / Snapdragon X Plus / Adreno X1-85
GPU results at `f6fb84...` are historical. **P0-R is complete** at `186f0393...`
on both named hardware targets: API 0.6.0, both formats, three decoder lifetimes,
malformed rejection/recovery and expected I420 output. The isolated P0.5 probe
adds raw/codec I420 patterns, SDL2 IYUV upload, explicit BT.709 limited conversion,
aspect fit, nearest/linear scaling, recreation and reset-event handling. It uses
the existing v15 SDL2 compatibility runtime backed by SDL3, with no Session
coupling. **P0.5 is complete**: owner visual inspection passed on RTX 4070 Ti x64
and native Surface Pro 11 / Snapdragon X Plus / Adreno X1-85 ARM64 for raw,
compatibility and record modes, all five patterns, resize/scaling,
fullscreen/maximize/restore and recreation. Ten lifecycles and synthetic reset
recovery passed; 60 FPS loops provide pacing sanity only. See [hardware evidence](VALIDATION.md#m1b-p05-offline-sdl-qualification).
No visible chroma anomaly was observed; authored CENTER versus SDL3 LEFT metadata
sampling remains formally unqualified because SDL2 has no independent siting
control. True physical GPU loss was not induced and remains manual/unproven.
Production/end-to-end latency, full pacing, 4:4:4 and HDR remain unqualified.

P1a is the next future milestone and would add explicit SDR 4:2:0 choice (Auto remains standard codecs), complete
runtime/presentation preflight, bitstream-ID comparison, minimal SCM/RTSP/SDP
negotiation, compatibility framing and safe fallback/reconnect without repeating
host-app actions. P1b adds live record framing, record-start/critical-count/loss
metadata, adaptive FEC and partial recovery. No live negotiation, user setting,
partial loss recovery or bandwidth probe is implemented by P0-R or P0.5.

## Planned Asteria VR boundary (M6)

Asteria VR is a separate, later PC-to-PC remote PCVR path. The host PC runs the game and SteamVR rendering; a Windows client PC has the headset physically attached and interfaces with its local VR runtime/driver. A host-side SteamVR driver/protocol integration should expose the remote headset and controllers to SteamVR. The client-side headset backend should adapt locally supported PCVR hardware without making any one headset or vendor protocol the transport definition. Both x64 and ARM64 support claims require hardware qualification, not just successful builds.

The network path must carry low-latency stereoscopic frames from host to client and time-sensitive head, controller, and tracker poses plus buttons/analog inputs from client to host. It must also account for host-to-client haptics and audio, client-to-host microphone audio, and eventually optional hand, eye, and full-body tracking where the local runtime exposes them. Define timestamps, coordinate spaces, device identity, and capability negotiation explicitly; unavailable sensors must remain optional. Clock synchronization, pose prediction, jitter and motion-to-photon latency measurement are design requirements. Investigate client-side reprojection/timewarp using the freshest local pose, with a clear fallback if a headset/runtime does not support the chosen approach.

Keep Asteria VR independent of desktop streaming session assumptions and of the M1B PyroWave codec experiment. Reuse proven codec or transport components later only when their timing and stereo behavior fit VR. Asteria VR is also separate from the PSVR2 wireless-adapter project, whose phone and wearable bridge are outside this architecture. PSVR2 plus its PC adapter may eventually be qualified as one headset physically connected to the Windows client; it must not dictate the driver, protocol, or client backend.

## Dependency policy

Keep the pinned upstream Qt/MSVC/qmake and dependency workflow for the baseline. Do not add a CMake, SDL major-version, decoder, or framework migration to the port. Record actual compiler/SDK/runtime versions, submodule SHAs, dependency archive hashes, and build commands.

The reviewed Android repository uses a different `moonlight-common-c` fork. Do not replace the newer PC core wholesale with that older snapshot. Review the minimal required native change, its exported API and wire behavior, and test it against standard hosts. Record its provenance and maintain it as a small, separately reviewable patch set.

## Session lifecycle

Extension work follows the existing session: disconnected → connecting → streaming → stopping → disconnected. Cancel requests and discard stale responses when the session generation changes. Network failure, focus loss, sleep/resume, and application shutdown must converge on the same cleanup behavior.

A failed optional extension reports a useful status and leaves ordinary streaming usable. Do not silently terminate a running host application, retry a side-effecting command, or overwrite unrelated display state as a recovery step.
