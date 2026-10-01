# Porting plan

Updated after owner validation: Asteria identity and x64/ARM64 CI packaging are implemented, and M0A native Windows ARM64 is complete on the recorded Surface Pro 11th Edition. M1A starts with Moonlight's existing performance statistics. M1B historical P0 proof used the old codec; P0-R is complete with Vibepollo-compatible x64/ARM64 real-hardware qualification; P0.5 offline SDL tooling is implemented and awaits owner visual qualification. [v0.1.0-preview.1](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.1.0-preview.1) is released for both targets. Completed items are checked below; baseline evidence lives in [BASELINE.md](BASELINE.md), and PyroWave hardware evidence in [VALIDATION.md](VALIDATION.md#m1b-p0-offline-pyrowave-validation).

## Review outcome

The original choice to reuse Moonlight's Windows streaming stack is sound. The revised plan makes the fork decision explicit, removes a redundant new application shell, separates existing upstream features from porting work, and gives every milestone an observable completion gate.

**Chosen approach: extend the merged Moonlight PC fork, establish native Windows x64 and ARM64 baselines, then add selected Artemis-inspired features incrementally.** Preserve upstream history and layout. See [the feature audit](FEATURE_AUDIT.md) and [architecture](ARCHITECTURE.md).

## Scope and dependency order

M0 is merged and M0A native ARM64 is complete based on owner-verified process architecture and Apollo AV1 streaming on real hardware. M1 identity is implemented; its profiles/session work, M1A performance work, and M2–M4 remain planned. M1B has historical source-diff/P0 evidence; P0-R Vibepollo compatibility and hardware requalification are complete. P0.5 offline SDL visual qualification is active before P1a/P1b live work. Apply M5 qualification to the initial preview's inherited streaming and identity scope; later feature milestones are not prerequisites for that preview. Clipboard (M3) depends on capability work in M2. Performance changes must follow measurements, and a cross-build alone does not complete M0A. M6 Asteria VR follows the core streaming and performance baselines as a separate later feature; it does not depend on M1B PyroWave or the M2–M4 Apollo extensions.

Primary targets: **Windows 11 x64 and native ARM64**, both included in v0.1.0-preview.1. Native ARM64 means the client and its process-loaded runtime DLLs run as ARM64, without x64 emulation; cross-compiling on an x64 build host is acceptable. Windows 10 x64 remains a separate compatibility target pending runtime documentation and real-machine tests. Record exact minimum OS builds before publishing qualified binaries. M0A completion is specific to the recorded device and workload, not a broad ARM64 support matrix.

The first public preview covers inherited Moonlight streaming and the implemented Asteria identity, delivered as x64 and native ARM64 portable ZIPs after release-specific validation. Desktop profiles, additional session actions, measured frame-pacing changes, Apollo clipboard, virtual-display controls, and host commands remain later roadmap work. Touch overlays, file transfer, simultaneous multiple streams, and a host companion service are outside the first release.

## M0 — Establish the Moonlight fork and reproducible baseline

**Status:** integration merged in [PR #1](https://github.com/Unitron07/Asteria-Windows/pull/1); x64 CI passed and the owner reported a successful manual test. Detailed qualification records remain outstanding as listed in the [baseline report](BASELINE.md).

- [x] Preserve the planning repository and Moonlight source history in a reviewed merge, retaining docs and resolving README/layout conflicts explicitly.
- [x] Record an `upstream` remote, baseline SHA, recursive submodule SHAs, source licenses, and imported-code provenance.
- [x] Build unmodified upstream first in Windows CI using its MSVC/Qt/qmake scripts. Capture compiler/SDK information, pinned Qt and dependency archive inputs, configuration, and commands in evidence artifacts.
- [x] Capture the existing source-based feature inventory.
- [ ] Capture measured performance baselines before branding or feature changes, and complete missing hardware/host-version records.
- [x] Adapt x64 Windows CI: recursive checkout, pinned actions and dependency checksums, build logs, executable artifacts, and symbols; ordinary builds use no release/signing credentials.

**Qualification gate (partly evidenced, carried forward):** a fresh checkout builds locally and in Windows CI; the deployed build launches on a clean test machine without developer tools; manually pair and stream 1080p60 H.264 SDR with audio, keyboard, mouse, and a gamepad. Record the Apollo host version and test the advertised Apollo preview scope. Store results using [VALIDATION.md](VALIDATION.md). The owner's general test confirmation does not supply these individual records. A build-only VM does not establish hardware-decoder support.

**Deliverable:** baseline import PR, build instructions, CI artifact, and baseline report. No client feature rewrite is needed here.

## M0A — Native Windows ARM64 baseline (complete)

- [x] Extend the existing build harness to accept explicit x64/ARM64 targets, preserving upstream source layout and the default x64 command. Dependency/preflight tests pass.
- [x] Use the upstream Qt 6.11.2 ARM64 cross kit and matching MSVC ARM64 tools. Keep host-side Qt build tools distinct from deployed ARM64 runtime files. Implemented in PR #6 and exercised by successful ARM64 CI.
- [x] Pin and verify the v15 Windows ARM64 dependency archive; record versions, hashes and source/license locations in [dependency notes](DEPENDENCIES_WINDOWS.md). Isolate dependencies with one target per checkout and architecture-specific output/evidence folders.
- [x] Build both unmodified upstream and the candidate for ARM64 in Windows CI. Keep x64 coverage; publish separate portable ZIPs, symbols, source, and compiler/SDK/dependency evidence for each architecture. All four jobs passed in [run 34790903403](https://github.com/Unitron07/Asteria-Windows/actions/runs/34790903403); PR #6 is merged.
- [x] Implement final-ZIP PE machine validation for every EXE/DLL, including nested Qt plugins, SDL, codecs, and AntiHooking, with a hash-bound evidence report. Local tests reject x64 DLL contamination in ARM64 packages and the reverse. Host build tools outside the ZIP are not scanned.
- [x] Hosted upstream/candidate builds and final-ZIP architecture gates pass for both targets in [run 34797782854](https://github.com/Unitron07/Asteria-Windows/actions/runs/34797782854). The owner subsequently verified native ARM64 process execution on the Surface Pro 11th Edition.
- [x] Record the owner's real-device comparison: native ARM64 Asteria felt smoother in menus/settings than emulated x64 Moonlight (**subjective**); repeated same-game 2560×1440 approximately 60 FPS AV1 streams on an Apollo host appeared effectively identical, without meaningful decode/render/frame-queue differences or an observed stream regression. This completes the M0A device streaming gate for the owner's Apollo setup; see [BASELINE.md](BASELINE.md) for evidence limits.
- [x] Verify the Asteria process as native ARM64 on a Surface Pro 11th Edition with Snapdragon X Plus and 16 GB RAM; record successful Apollo AV1 streaming on the real device.
- [x] Compare with the official x64 Moonlight release under Windows ARM64 emulation on the same device, the practical available upstream release. There is no official native ARM64 upstream Moonlight release. CI-built unmodified upstream ARM64 artifacts are internal reference builds only.

Additional codec, input/audio, lifecycle, clean-machine, decoder, and exact host/driver/build records belong to first-preview release validation in [VALIDATION.md](VALIDATION.md), not the completed M0A gate.

**Exit gate: complete.** Reproducible ARM64 reference/Asteria builds and artifacts, native ARM64 Asteria process verification, real-device Apollo AV1 streaming, and passing x64 regression builds are recorded. The official x64 Moonlight release under emulation is the practical comparison for users; the Asteria process itself was native ARM64.

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
- [ ] Use Moonlight PC's existing performance overlay and logs for the initial baseline: decode time, rendering time, frame-queue delay, network latency/variance, dropped frames, codec, resolution, and FPS where available. Record settings and test conditions with each run. The owner's ARM64 versus emulated x64 Moonlight AV1 comparison is initial evidence of stream parity, not a controlled benchmark.
- [ ] Identify a concrete missing metric and the decision it would inform before adding targeted instrumentation. Do not create a telemetry subsystem or per-frame logging by default. Clearly distinguish measured values from estimates.
- [ ] Audit Artemis Android performance-related changes and classify them as platform-neutral, Android/MediaCodec-specific, device-workaround-specific, or already present in Moonlight PC.
- [ ] Only if repeatable measurements reveal a real pacing problem or a benefit worth testing, prototype explicit presentation policies such as **Low Latency**, **Balanced**, and **Smooth**, while preserving an upstream-compatible/default mode. Define each policy by concrete queue/scheduling behavior rather than labels alone.
- [ ] If measured jitter or queue behavior warrants it, prototype a bounded adaptive presentation/jitter queue that can absorb short network/decode timing variance and shrink when conditions improve. Cap queue growth and expose the latency cost rather than silently accumulating delay.
- [ ] Test stable-LAN and induced-jitter scenarios at representative 60/90/120/144 FPS targets where hardware permits. Compare smoothness, dropped/repeated frames, input feel, and measured latency against unmodified Moonlight.
- [ ] Investigate VRR-aware presentation on supported Windows displays. Measure DXGI/compositor/fullscreen behavior and frame pacing before enabling a dedicated VRR mode; do not assume VRR automatically lowers latency.
- [ ] Extend the performance overlay only for a concrete unanswered question and with Windows counters whose timing boundaries are understood. Useful candidates include network latency/variance, decode time, presentation queue depth, present timing, dropped frames, codec/decoder, and active pacing mode.
- [ ] Avoid changing networking, decoder selection, or input paths unless measurements identify them as the actual bottleneck. Any default behavior change requires reproducible evidence that it improves a stated metric or pacing condition without unacceptable regressions.

**Exit gate:** the owner comparison is retained as initial parity evidence, and at least one representative x64 system and one ARM64 system have repeatable comparisons using existing Moonlight statistics. Any shipped performance mode must improve a stated metric or pacing condition without unacceptable latency, stability, power, or compatibility regressions. If no measured problem or reliable benefit warrants a pacing change, retain upstream presentation behavior. Add a targeted metric only when the built-in stats leave a concrete question unanswered.

**Deliverable:** benchmark procedure and upstream-vs-Asteria results based on existing statistics, plus only justified diagnostics or pacing changes.

## M1B — Experimental PyroWave

**Status:** historical P0 groundwork merged in [PR #16](https://github.com/Unitron07/Asteria-Windows/pull/16). Vibepollo-compatible P0-R merged in [PR #19](https://github.com/Unitron07/Asteria-Windows/pull/19) and is **complete on both hardware targets**. P0.5 offline SDL tooling is implemented and passes both-target CI; code completion follows merge and owner visual qualification remains pending. PyroWave stays experimental and off by default. Normal releases do not ship active PyroWave streaming support. This optional milestone is independent of M2–M4 and was not required for v0.1.0-preview.1. See [validation](VALIDATION.md#m1b-p05-offline-sdl-qualification) and [next step](NEXT_STEP.md).

### Historical P0 complete: older f6fb84 codec / private PYRW validation

- [x] Complete the source diff, pin the codec/dependency graph and reference host, and record protocol/licensing/provenance findings.
- [x] Build the exact pinned dependencies for x64 and native ARM64, including the isolated ARM64 portable-math compatibility patch and source-history recovery bundle.
- [x] Implement the bounded GPU-free PYRW parser, restricted dynamic runtime loading, API/export checks, and parser/runtime/compatibility tests under an off-by-default experimental build flag.
- [x] Validate x64 offline 1920×1080 SDR 4:2:0 GPU decode on Windows 11 / RTX 4070 Ti (MAE 0.000694444).
- [x] Validate native Windows ARM64 offline 1920×1080 SDR 4:2:0 GPU decode on Surface Pro 11th Edition / Snapdragon X Plus / Adreno X1-85 (MAE 0.00104167).

Both hardware proofs passed three decoder lifetimes and malformed-frame rejection/recovery into known I420 CPU buffers. They do not qualify SDL presentation or live end-to-end streaming.

### P0-R COMPLETE: Vibepollo compatibility and hardware requalification

- [x] Realign active upstream codec to `186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, preserving API 0.6.0 and architecture isolation.
- [x] Add LE compatibility and complete record-framing parsers; keep PYRW as an explicit historical fixture helper.
- [x] Apply decoder short-block safety patch with exact provenance/hashes; retain encoder-only pool/4:4:4 sizing patches.
- [x] Complete x64/native ARM64 dependency/parser/loader/qmake/PE CI for the new codec; see run 36795604865 in VALIDATION.md. Hosted GPU attempts were unavailable skips.
- [x] Both framing modes pass on RTX 4070 Ti x64 and native Surface Pro 11 / Snapdragon X Plus / Adreno X1-85 ARM64 at the new codec/bitstream ID and API 0.6.0: three decoder lifetimes, malformed rejection/recovery, expected I420 planes. Old GPU results remain historical.

See [the current Vibepollo contract](PYROWAVE_VIBEPOLLO.md). No live negotiation,
advertisement, UI setting, packaging or frame-pacing change is made here.

### P0.5 ACTIVE: offline SDL presentation qualification

The standalone raw/codec pattern harness is implemented, with ten lifecycle
cycles, reset handling, aspect fit, scaling controls, timing and evidence.
Code/CI completion is recorded once merged; **hardware visual qualification is
pending** on both targets. Synthetic recreation does not prove real device loss.

- [ ] Exercise real SDL IYUV presentation on x64 and ARM64; verify SDR color/range/chroma siting with visible test patterns and record pacing behavior.
- [ ] Test resize, decoder/device recreation, repeated runtime lifetimes, and device-loss/recovery behavior.
- [ ] Qualify 4:4:4 separately after 4:2:0; keep HDR excluded.
- [ ] Review runtime deployment/import closure, clean-machine CRT/Vulkan requirements, licenses, and packaging/shipping policy.

### P1a planned: explicit SDR 4:2:0 live integration

- [ ] Add explicit experimental SDR 4:2:0 choice, full runtime/presentation preflight, bitstream-ID check and minimal reviewed SCM/RTSP/SDP/common-c changes. Auto remains standard codecs; use Vibepollo compatibility framing first.
- [ ] Implement safe decoder selection and standard H.264/HEVC/AV1 fallback/reconnect, with correct per-decoder properties and no replay of host-app actions.
- [ ] Validate interoperability against Nonary/Vibepollo with matching codec/bitstream ID; pyrollo comparisons remain historical evidence only. No live end-to-end PyroWave stream is yet validated.
- [ ] Exercise negative negotiation/runtime cases and H.264/HEVC/AV1 lifecycle/audio/input regressions on both targets. Preserve current bitrate and frame pacing.
- [ ] Compare production latency, decode/present time, drops, GPU use, bandwidth, and network queuing against standard codecs under the same conditions.

**Live exit gate:** a qualified opt-in stream works with the pinned host on named hardware, safe fallback and regressions pass, and measurements/limitations are recorded. P1 has not started. Keep PyroWave experimental and out of normal release behavior until these gates and shipping policy are reviewed.

### P1b planned: record framing and transport hardening

- [ ] Add live records, record-start/lost-buffer metadata, critical packet counts, adaptive FEC and partial recovery with sideband readiness.
- [ ] Keep bandwidth probing as a later usability enhancement; document host link/probe fields, warmup, slowest-of-three and 20% reserve without activating them.
- [ ] Keep HDR capability bits documented but disabled; no 10-bit decoder support is claimed.

**Deliverables:** Vibepollo-compatible offline source/build/parser evidence and real-hardware requalification; then P0.5 presentation, P1a explicit live integration and P1b transport hardening.

## M2 — Pointer/scaling correctness and Apollo capability foundation

- [ ] Validate upstream direct/relative mouse modes, wheel input, keyboard layouts, focus behavior, and mixed-DPI monitor moves before changing them.
- [ ] Implement only verified gaps in fit/fill/stretch, pointer-mode switching, and session controls. Keep pan/zoom and touchpad overlays as later work unless a specific desktop requirement needs them.
- [ ] Parse authenticated host extension fields, permission bits, driver readiness, and command names; keep absence, denial, and transient failure distinct.
- [ ] Add fixtures for a standard host, Apollo with permissions, Apollo with denied permissions, malformed/missing fields, and changed capabilities after reconnect.

**Exit gate:** direct-pointer corner/center mapping stays correct under every implemented scaling mode, 100/150/200% DPI, and monitor switching; focus loss releases input. Ordinary Apollo streaming works with extension fields absent and no unsolicited extension actions. A denied or failed extension does not stop the stream.

## M3 — Apollo text clipboard, then virtual-display requests

- [ ] Implement manual Send/Receive using the audited HTTPS contract and existing pairing trust. Add bounded requests, Unicode handling, echo suppression, cancellation, and explicit error states.
- [ ] Start with a proposed 1 MiB UTF-8 text limit and a 5-second request timeout; validate them during the spike. Limit response accumulation as well as outgoing data.
- [ ] Add automatic per-host synchronization only after manual behavior passes; default it off and document its triggers. Exclude file/image clipboard formats.
- [ ] Request Apollo virtual displays only after authenticated capability/readiness checks. Validate launch and resume separately; do not assume their behavior is identical.
- [ ] Exercise driver-missing, permission-denied, launch-failure, disconnect, client-crash, and reconnect cases. Document host-owned cleanup and recovery rather than attempting to restore the host's entire display configuration from the client.

**Exit gate:** plain text transfers both ways only with an active authorized session; failures preserve the local clipboard and do not leak content to another host. Unsupported responses, including an HTTP-200 error document, are not treated as clipboard text or successful writes. Apollo display requests succeed on the recorded host build or give an actionable reason; ordinary Apollo launch remains unchanged.

## M4 — Apollo server commands

- [ ] Audit the Android JNI/native path against the chosen PC core and host revision. Implement a minimal native extension; preserve modern PC protocol fixes.
- [ ] Map host-advertised command names to their original indexes; enforce the native range and permissions, and handle missing/changed command lists.
- [ ] Add explicit action confirmation as described in the architecture. Do not retry commands automatically or present a transport send as confirmed execution.

**Exit gate:** a harmless configured command reaches the intended action on the test host, invalid/denied commands are blocked, reconnect does not replay actions, and standard Apollo streaming still passes. Include packet/API tests for the native patch and record provenance.

## M5 — Qualify and release

- [ ] When M1B PyroWave support is included, qualify the host/client/GPU support matrix, runtime packaging, codec fallback, and comparisons; keep it experimental until evidenced.
- [ ] When M1A performance changes are included, re-run the candidate-to-upstream performance comparison using the same hardware, host, display mode, codec, network, and workload for release candidates.
- [ ] Complete required [functional and performance checks](VALIDATION.md), including GPU-specific paths available for the claimed support matrix.
- [x] Publish v0.1.0-preview.1 as separate x64 and native ARM64 portable ZIPs, with hashes, symbols, notices, corresponding source including submodules, and architecture/build evidence; see the [release record](VALIDATION.md#first-public-preview-record-2026-09-25). Repeat artifact checks and publish build steps/limitations for each later release.
- [ ] Validate ZIP data location, update, and clean-machine launch. Adapt upstream installer infrastructure after the portable preview is stable; test install/upgrade/uninstall and preservation of user data.
- [ ] Sign public stable executables/installers when release credentials are provisioned. Treat signing as a release gate, not a dependency for local development or clearly labeled unsigned previews.

**Exit gate:** all required checks for the advertised release scope pass, unsupported hardware/OS combinations are listed honestly, artifacts can be reproduced from the published inputs, and no unresolved regression defeats an included feature.

## M6 — Asteria VR (remote PCVR)

**Status:** planned for after the core Windows streaming and M1A performance work. Asteria VR is PC-to-PC: the host PC runs SteamVR and renders the game; the VR headset is physically connected to the Windows client PC. This milestone is separate from the phone/wearable PSVR2 wireless-adapter project. PSVR2 plus its PC adapter may eventually be one qualified client-side headset configuration, but the architecture must remain headset-agnostic. M6 may reuse suitable codec/transport work from M1B, but PyroWave is neither required nor the default design.

- [ ] Prove a host SteamVR driver/protocol path and client headset/runtime backend. Define device capabilities, coordinate spaces, timestamps, and a clock-synchronization method; transport head/controller/tracker poses to the host and measure pose age and jitter.
- [ ] Deliver stereoscopic, low-latency frames from the host renderer to the client headset. Measure encode, network, decode, presentation, and motion-to-photon timing with a supported test headset.
- [ ] Add bidirectional interaction: client-to-host buttons/analog inputs and microphone audio, and host-to-client controller haptics and audio. Check device identity, reconnect, and loss behavior.
- [ ] Investigate local pose prediction and client-side reprojection/timewarp, then tune latency and jitter handling from measurements. Document runtime-specific support and a safe fallback for unavailable features.
- [ ] Qualify broader locally attached PCVR headsets and runtimes, then consider optional hand, eye, and full-body tracking only where exposed by the client runtime and explicitly negotiated.

**Exit gate:** a documented headset/runtime matrix demonstrates stable stereo presentation, tracked input, haptics, audio/mic, reconnect behavior, and measured latency on named host/client hardware. Report unsupported devices and optional tracking capabilities explicitly; successful desktop streaming alone does not qualify VR.

**Deliverable:** SteamVR integration proof of concept, timestamped transport design, staged interoperability tests, latency results, and a qualified headset/runtime matrix.

## Upstream maintenance

Keep feature PRs small and avoid mass renames of upstream source directories. Record upstream merge points and native-library patches. Check upstream changes before every release; prioritize security and correctness fixes, then rerun affected checks and the baseline streaming smoke test. Keep upstream platform code even when Windows is the only release target.

## Remaining decisions

- Exact Windows 11 ARM64 minimum build, GPU driver, selected decoder, and Apollo version: record for first-preview release qualification; M0A device/SoC/RAM and native process architecture are recorded.
- Exact Windows 10 x64 minimum build and runtime support: resolve before advertising compatibility.
- Tested Apollo version and the original manual test's client/host details: record for release qualification; extension-specific support boundaries follow in M2. Sunshine is not a required project/preview qualification target.
- Default frame-pacing policy and whether adaptive buffering/VRR modes graduate from experimental status: resolve from M1A measurements, not Android behavior alone.
- Overlay rendering approach: choose only after testing the existing video-window integration and latency impact.
- Touch-device qualification beyond the baseline keyboard/mouse/gamepad cases remains later work.

Do not attach calendar estimates until the ARM64 baseline, Windows performance experiments, and Apollo protocol spikes identify actual effort. The next concrete development task is M1B P0.5 presentation/color qualification, followed by a separate P1 live opt-in negotiation change. M1A initial comparisons use Moonlight's existing statistics; new instrumentation or frame-pacing changes require a measured reason. Same-commit x64 smoke testing and detailed Apollo/hardware records remain first-preview release checks. M1 identity/storage isolation is implemented; profiles, session workflows, M1A performance work, M1B presentation/live integration, M2–M4, and M6 Asteria VR remain future work.
