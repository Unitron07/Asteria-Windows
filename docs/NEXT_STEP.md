# Next step: M1B P0 hardware qualification, then P1 live opt-in 420

M0A native Windows ARM64 is complete based on the owner's real-device validation. M1 identity/rebrand is implemented. **PR #14 completed the source-diff phase. [PR #16](https://github.com/Unitron07/Asteria-Windows/pull/16) implements the isolated P0 dependency/parser/runtime/offline-decode experiment.** Use the [P0 evidence and remaining gates](M1B_PYROWAVE_SPIKE.md) and [reproduction commands](../tests/pyrowave/README.md); do not repeat broad protocol research or enable live negotiation as part of P0.

## M1A baseline decision

Moonlight PC's existing performance statistics are sufficient for the initial M1A comparison. Use the overlay and logs to record decode time, rendering time, frame-queue delay, network latency/variance, dropped frames, codec, resolution, and actual frame rate where exposed by the selected build. Record settings, device/driver, host version, workload, and run duration alongside the readings. The owner's repeated Surface Pro 11th Edition runs already provide **initial evidence of stream parity**: native ARM64 Asteria and the official x64 Moonlight release under Windows ARM64 emulation streamed the same Apollo AV1 workload at 2560×1440 and approximately 60 FPS with no meaningful decode, render, or frame-queue regression observed. The menus/settings felt smoother in Asteria, but that observation was qualitative. See [BASELINE.md](BASELINE.md) and [VALIDATION.md](VALIDATION.md) for limits and a repeatable comparison method.

Do not build a new telemetry subsystem by default. If an investigation needs a metric the existing stats cannot provide reliably, name that metric and the decision it would inform, then add the smallest targeted measurement. Do not alter frame-pacing behavior unless repeatable measurements demonstrate a real problem or a benefit without unacceptable regressions. An M1A baseline can conclude with the existing behavior retained.

## M1B after the offline implementation

1. Complete native ARM64 GPU P0 on actual hardware with a target-native Vulkan loader/ICD. Run the generated 1080p SDR 420 roundtrip, record selected GPU/driver/API, output hashes and three decoder lifetimes. A native hosted parser/load run is valuable but does not qualify GPU decoding.
2. Qualify presentation and color: the P0 proof supplies a known I420 CPU buffer, not an SDL window or measured live renderer. Confirm SDR BT.709/range, chroma siting, texture upload and visible patterns on both architectures. Add a separate 444 fixture after 420 is established; keep HDR excluded.
3. Resolve the recorded Session single-format selection/RTSP fallback problem before P1. Keep a validated ordinary-codec candidate and the correct per-decoder properties; a missing SDP mapping or failed setup must recover without feeding PyroWave to FFmpeg or replaying host-app actions.
4. P1 is a separate focused change: runtime-gated, explicitly opted-in 420 streaming against the pinned Pyrollo host, plus only the minimal reviewed common-c protocol delta on Asteria's current gitlink. No live bits, RTSP/SDP changes or user preference are included in P0.
5. Before any live offer, test missing/wrong DLL/export/API/driver, unavailable GPU features, absent/partial SCM/SDP, forced standard codecs, HDR, software decoder policy, setup failure after preflight, reconnect/device loss and host encoder failure. Re-run H.264/HEVC/AV1 lifecycle/audio/input and existing-statistics comparisons. Review container overhead, frame/packet limits, FEC/MTU and loss tests before accepting host-controlled allocations.

The P1 exit gate is a qualified opt-in stream with safe ordinary-codec fallback and measured behavior on named hardware. Normal preview builds continue to exclude PyroWave until those gates are reviewed. Preserve current bitrate and frame pacing.

## Separate first-preview release checks

Use [VALIDATION.md](VALIDATION.md) for same-commit x64/ARM64 smoke tests, Apollo lifecycle/input/audio checks, clean-machine launch, artifact hashes, tested Windows build and driver, selected decoder, Apollo version, and known issues. These release records do not reopen M0A. PyroWave is not a first-preview prerequisite.
