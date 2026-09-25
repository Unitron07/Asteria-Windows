# Porting plan

Updated for the first portable preview: Asteria identity and x64/ARM64 CI packaging are implemented; real Windows 11 ARM64 device qualification remains pending. Completed integration items are checked below; build and test evidence lives in [BASELINE.md](BASELINE.md).

## Review outcome

The original choice to reuse Moonlight's Windows streaming stack is sound. The revised plan makes the fork decision explicit, removes a redundant new application shell, separates existing upstream features from porting work, and gives every milestone an observable completion gate.

**Chosen approach: extend the merged Moonlight PC fork, establish native Windows x64 and ARM64 baselines, then add selected Artemis-inspired features incrementally.** Preserve upstream history and layout. See [the feature audit](FEATURE_AUDIT.md) and [architecture](ARCHITECTURE.md).

## Scope and dependency order

M0 is merged. M0A build and packaging work passes CI; real-device qualification remains open. M1 identity is implemented, while its profiles/session work and M1A–M4 remain planned. Apply M5 qualification to the initial preview's inherited streaming and identity scope; later feature milestones are not prerequisites for that preview. Clipboard (M3) depends on capability work in M2. Performance changes must follow measurements, and a cross-build alone does not complete M0A.

Primary targets: **Windows 11 x64 and native ARM64**, both intended for the first preview. Native ARM64 means the client and its process-loaded runtime DLLs run as ARM64, without x64 emulation; cross-compiling on an x64 build host is acceptable. Windows 10 x64 remains a separate compatibility target pending runtime documentation and real-machine tests. Record exact minimum OS builds before publishing qualified binaries. These are support goals, not claims of completed ARM64 testing.

The first public preview covers inherited Moonlight streaming and the implemented Asteria identity, delivered as x64 and native ARM64 portable ZIPs after hardware qualification. Desktop profiles, additional session actions, measured frame-pacing changes, Apollo clipboard, virtual-display controls, and host commands remain later roadmap work. Touch overlays, file transfer, simultaneous multiple streams, and a host companion service are outside the first release.

## M0 — Establish the Moonlight fork and reproducible baseline

**Status:** integration merged in [PR #1](https://github.com/Unitron07/Asteria-Windows/pull/1); x64 CI passed and the owner reported a successful manual test. Detailed qualification records remain outstanding as listed in the [baseline report](BASELINE.md).

- [x] Preserve the planning repository and Moonlight source history in a reviewed merge, retaining docs and resolving README/layout conflicts explicitly.
- [x] Record an `upstream` remote, baseline SHA, recursive submodule SHAs, source licenses, and imported-code provenance.
- [x] Build unmodified upstream first in Windows CI using its MSVC/Qt/qmake scripts. Capture compiler/SDK information, pinned Qt and dependency archive inputs, configuration, and commands in evidence artifacts.
- [x] Capture the existing source-based feature inventory.
- [ ] Capture measured performance baselines before branding or feature changes, and complete missing hardware/host-version records.
- [x] Adapt x64 Windows CI: recursive checkout, pinned actions and dependency checksums, build logs, executable artifacts, and symbols; ordinary builds use no release/signing credentials.

**Qualification gate (partly evidenced, carried forward):** a fresh checkout builds locally and in Windows CI; the deployed build launches on a clean test machine without developer tools; manually pair and stream 1080p60 H.264 SDR with audio, keyboard, mouse, and a gamepad. Record exact Sunshine and Apollo host versions and test each independently. Store results using [VALIDATION.md](VALIDATION.md). The owner's general test confirmation does not supply these individual records. A build-only VM does not establish hardware-decoder support.

**Deliverable:** baseline import PR, build instructions, CI artifact, and baseline report. No client feature rewrite is needed here.

## M0A — Native Windows ARM64 baseline (device qualification pending)

- [x] Extend the existing build harness to accept explicit x64/ARM64 targets, preserving upstream source layout and the default x64 command. Dependency/preflight tests pass.
- [x] Use the upstream Qt 6.11.2 ARM64 cross kit and matching MSVC ARM64 tools. Keep host-side Qt build tools distinct from deployed ARM64 runtime files. Implemented in PR #6 and exercised by successful ARM64 CI.
- [x] Pin and verify the v15 Windows ARM64 dependency archive; record versions, hashes and source/license locations in [dependency notes](DEPENDENCIES_WINDOWS.md). Isolate dependencies with one target per checkout and architecture-specific output/evidence folders.
- [x] Build both unmodified upstream and the candidate for ARM64 in Windows CI. Keep x64 coverage; publish separate portable ZIPs, symbols, source, and compiler/SDK/dependency evidence for each architecture. All four jobs passed in [run 34790903403](https://github.com/Unitron07/Asteria-Windows/actions/runs/34790903403); PR #6 is merged.
- [x] Implement final-ZIP PE machine validation for every EXE/DLL, including nested Qt plugins, SDL, codecs, and AntiHooking, with a hash-bound evidence report. Local tests reject x64 DLL contamination in ARM64 packages and the reverse. Host build tools outside the ZIP are not scanned.
- [x] Hosted upstream/candidate builds and final-ZIP architecture gates pass for both targets in [run 34797782854](https://github.com/Unitron07/Asteria-Windows/actions/runs/34797782854). Real-device execution remains unqualified.
- [ ] Test the portable ARM64 build on a real Windows 11 ARM64 device without development tools: verify native process architecture, launch, discovery/manual host, pairing, H.264 1080p60 SDR, audio, keyboard, mouse, and gamepad with separately recorded Sunshine and Apollo hosts.
- [ ] Record hardware decoding and a performance baseline on that device against unmodified ARM64 Moonlight built with the same inputs. Test available additional codecs without claiming unsupported GPU paths.

**Exit gate:** reproducible ARM64 upstream/candidate builds and artifacts, native ARM64 runtime verification, real-device launch and streaming evidence, and passing x64 regression builds. Missing hardware or host access is an explicit open gate. An x64 binary under emulation does not satisfy the native ARM64 deliverable.

**Deliverable:** native ARM64 baseline PR and portable development build, per-architecture evidence, and hardware test report. See the implementation handoff in [NEXT_STEP.md](NEXT_STEP.md).

## M1 — Project identity and desktop workflow foundation

- [x] Implement Asteria app/package identity and artwork while preserving upstream attribution and licenses (PR #9).
- [x] Isolate settings, pairing identity, logs, and installer identifiers.
- [ ] Qualify side-by-side use with Moonlight on the release test machines; installer lifecycle checks apply when installers are offered.
- [ ] Add versioned global/host/app profiles around existing resolution, FPS, bitrate, codec, and input settings. Validate bounds and explain which changes require reconnecting.
- [ ] Add configurable session shortcuts and distinct actions for disconnecting the client, quitting the remote application, and closing the local app. Preserve a local capture-release shortcut.
- [ ] Extend existing diagnostics only for missing data; retain upstream stats and avoid adding per-frame logging.

**Exit gate on x64 and ARM64:** profiles survive restart and invalid data fails safely; settings/credentials remain isolated; 20 connect/disconnect cycles leave no stuck input or active extension tasks; disconnect leaves the host application running while an explicitly selected quit action has the documented host effect. M1A–M4 changes also retain both architecture builds and run affected checks on each target.

## M1A — Windows streaming performance and frame pacing

The goal is not to blindly copy Artemis Android decoder tweaks. Artemis Android and Moonlight Android use Android-specific decoder and presentation paths; Windows uses different hardware decode/render/presentation APIs. Port only platform-neutral ideas that prove beneficial on Windows.

- [ ] Establish repeatable x64 and ARM64 performance comparisons against unmodified Moonlight using the same client hardware, display mode, host, codec, bitrate, frame rate, network path, and workload.
- [ ] Instrument useful pipeline boundaries such as packet/frame arrival, decode start/finish, presentation-queue entry, present request, and presentation completion. Keep telemetry low-overhead and aggregated; do not enable per-frame logging by default.
- [ ] Record network variance, decode timing, render/present timing, dropped frames, queue depth, CPU/GPU use, and any trustworthy end-to-end latency observations. Clearly distinguish measured values from estimates.
- [ ] Audit Artemis Android performance-related changes and classify them as platform-neutral, Android/MediaCodec-specific, device-workaround-specific, or already present in Moonlight PC.
- [ ] Prototype explicit presentation policies such as **Low Latency**, **Balanced**, and **Smooth**, while preserving an upstream-compatible/default mode. Define each policy by concrete queue/scheduling behavior rather than labels alone.
- [ ] Prototype a bounded adaptive presentation/jitter queue that can absorb short network/decode timing variance and shrink when conditions improve. Cap queue growth and expose the latency cost rather than silently accumulating delay.
- [ ] Test stable-LAN and induced-jitter scenarios at representative 60/90/120/144 FPS targets where hardware permits. Compare smoothness, dropped/repeated frames, input feel, and measured latency against unmodified Moonlight.
- [ ] Investigate VRR-aware presentation on supported Windows displays. Measure DXGI/compositor/fullscreen behavior and frame pacing before enabling a dedicated VRR mode; do not assume VRR automatically lowers latency.
- [ ] Extend the performance overlay only with Windows counters whose timing boundaries are understood. Useful candidates include network latency/variance, decode time, presentation queue depth, present timing, dropped frames, codec/decoder, and active pacing mode.
- [ ] Avoid changing networking, decoder selection, or input paths unless measurements identify them as the actual bottleneck. Any default behavior change requires reproducible evidence that it improves a stated metric or pacing condition without unacceptable regressions.

**Exit gate:** at least one representative x64 system and one ARM64 system have reproducible upstream-vs-Asteria traces. Any shipped performance mode improves a stated metric or frame-pacing condition without unacceptable latency, stability, power, or compatibility regressions. Stable-network and jittered-network cases are both tested, and the upstream-compatible mode remains available. If no prototype reliably beats upstream, retain upstream presentation behavior and keep only the useful instrumentation/diagnostics.

**Deliverable:** performance trace format, benchmark procedure, upstream-vs-Asteria results, and only the presentation/pacing modes that survive measurement.

## M1B — Experimental PyroWave streaming with Vibepollo

**Status:** planned, after M1A baseline measurements. This optional milestone is independent of M2–M4 Apollo features and is not required for the first public preview.

- [ ] Pin the PyroWave implementation and a Vibepollo host revision. Verify licensing, actual codec negotiation, frame transport, color metadata, and compatibility with this fork's Moonlight protocol core.
- [ ] Add opt-in codec negotiation and an isolated Windows GPU decoder/presentation path. Select PyroWave only when host and client support are confirmed; retain H.264/HEVC/AV1 and recoverable fallback.
- [ ] Validate Vulkan/runtime packaging and x64/native ARM64 builds. Qualify each architecture on actual hardware before advertising support.
- [ ] Compare latency, decode/present time, drops, GPU use, bandwidth, and network queuing against existing codecs on the same host, client, network, and workload.
- [ ] Test reconnect, resolution and frame-rate changes, SDR/HDR and chroma where supported, and regression coverage with standard Sunshine and Apollo streams.

**Exit gate:** an opt-in stream works with a pinned Vibepollo build on qualified Windows hardware, measurements and limitations are recorded, and existing codec paths still work. If interoperability or measured benefit is insufficient, leave it experimental and out of release builds.

**Deliverable:** protocol spike, isolated integration, benchmark results, and a documented support matrix.

## M2 — Pointer/scaling correctness and Apollo capability foundation

- [ ] Validate upstream direct/relative mouse modes, wheel input, keyboard layouts, focus behavior, and mixed-DPI monitor moves before changing them.
- [ ] Implement only verified gaps in fit/fill/stretch, pointer-mode switching, and session controls. Keep pan/zoom and touchpad overlays as later work unless a specific desktop requirement needs them.
- [ ] Parse authenticated host extension fields, permission bits, driver readiness, and command names; keep absence, denial, and transient failure distinct.
- [ ] Add fixtures for a standard host, Apollo with permissions, Apollo with denied permissions, malformed/missing fields, and changed capabilities after reconnect.

**Exit gate:** direct-pointer corner/center mapping stays correct under every implemented scaling mode, 100/150/200% DPI, and monitor switching; focus loss releases input. Sunshine works with extension fields absent and no unsolicited Apollo actions. A denied or failed extension does not stop the stream.

## M3 — Apollo text clipboard, then virtual-display requests

- [ ] Implement manual Send/Receive using the audited HTTPS contract and existing pairing trust. Add bounded requests, Unicode handling, echo suppression, cancellation, and explicit error states.
- [ ] Start with a proposed 1 MiB UTF-8 text limit and a 5-second request timeout; validate them during the spike. Limit response accumulation as well as outgoing data.
- [ ] Add automatic per-host synchronization only after manual behavior passes; default it off and document its triggers. Exclude file/image clipboard formats.
- [ ] Request Apollo virtual displays only after authenticated capability/readiness checks. Validate launch and resume separately; do not assume their behavior is identical.
- [ ] Exercise driver-missing, permission-denied, launch-failure, disconnect, client-crash, and reconnect cases. Document host-owned cleanup and recovery rather than attempting to restore the host's entire display configuration from the client.

**Exit gate:** plain text transfers both ways only with an active authorized session; failures preserve the local clipboard and do not leak content to another host. Unsupported responses, including an HTTP-200 error document, are not treated as clipboard text or successful writes. Apollo display requests succeed on the recorded host build or give an actionable reason; ordinary Sunshine launch remains unchanged.

## M4 — Apollo server commands

- [ ] Audit the Android JNI/native path against the chosen PC core and host revision. Implement a minimal native extension; preserve modern PC protocol fixes.
- [ ] Map host-advertised command names to their original indexes; enforce the native range and permissions, and handle missing/changed command lists.
- [ ] Add explicit action confirmation as described in the architecture. Do not retry commands automatically or present a transport send as confirmed execution.

**Exit gate:** a harmless configured command reaches the intended action on the test host, invalid/denied commands are blocked, reconnect does not replay actions, and standard Sunshine streaming still passes. Include packet/API tests for the native patch and record provenance.

## M5 — Qualify and release

- [ ] When M1A performance changes are included, re-run the candidate-to-upstream performance comparison using the same hardware, host, display mode, codec, network, and workload for release candidates.
- [ ] Complete required [functional and performance checks](VALIDATION.md), including GPU-specific paths available for the claimed support matrix.
- [ ] Produce separate x64 and native ARM64 portable ZIPs for the first preview, with runtime dependencies, version information, hashes, symbols, notices, and corresponding source including pinned submodule contents. Publish exact build steps and known limitations for each architecture. Do not label an x64-emulated build as the ARM64 release.
- [ ] Validate ZIP data location, update, and clean-machine launch. Adapt upstream installer infrastructure after the portable preview is stable; test install/upgrade/uninstall and preservation of user data.
- [ ] Sign public stable executables/installers when release credentials are provisioned. Treat signing as a release gate, not a dependency for local development or clearly labeled unsigned previews.

**Exit gate:** all required checks for the advertised release scope pass, unsupported hardware/OS combinations are listed honestly, artifacts can be reproduced from the published inputs, and no unresolved regression defeats an included feature.

## Upstream maintenance

Keep feature PRs small and avoid mass renames of upstream source directories. Record upstream merge points and native-library patches. Check upstream changes before every release; prioritize security and correctness fixes, then rerun affected checks and the baseline streaming smoke test. Keep upstream platform code even when Windows is the only release target.

## Remaining decisions

- Exact Windows 11 ARM64 minimum build, device/SoC/GPU/driver, and qualification host access: resolve in M0A.
- Exact Windows 10 x64 minimum build and runtime support: resolve before advertising compatibility.
- Tested Sunshine/Apollo versions and the original manual test's client/host details: record during M0A; extension-specific support boundaries follow in M2.
- Default frame-pacing policy and whether adaptive buffering/VRR modes graduate from experimental status: resolve from M1A measurements, not Android behavior alone.
- Overlay rendering approach: choose only after testing the existing video-window integration and latency impact.
- Touch-device qualification beyond the baseline keyboard/mouse/gamepad cases remains later work.

Do not attach calendar estimates until the ARM64 baseline, Windows performance experiments, and Apollo protocol spikes identify actual effort. The next concrete task is M0A real-device qualification and same-commit x64 smoke testing for the initial preview. M1 identity/storage isolation is implemented; profiles, session workflows, and M1A–M4 remain future implementation work.
