# Next step: M1B P0.5 / presentation qualification

M0A native Windows ARM64 is complete based on the owner's real-device validation. M1 identity/rebrand is implemented. **PR #14 completed the source-diff phase; merged [PR #16](https://github.com/Unitron07/Asteria-Windows/pull/16) completed P0 dependency/parser/runtime/offline-decode proof.** Windows x64 GPU decode is validated on an RTX 4070 Ti; native Windows ARM64 GPU decode is validated on a Surface Pro 11th Edition / Snapdragon X Plus / Adreno X1-85. These are offline codec/runtime results, not live streaming or SDL presentation qualification. Use the [P0 evidence and remaining gates](M1B_PYROWAVE_SPIKE.md), [validation record](VALIDATION.md#m1b-p0-offline-pyrowave-validation), and [reproduction commands](../tests/pyrowave/README.md). PyroWave remains experimental and off by default.

## M1A baseline decision

Moonlight PC's existing performance statistics are sufficient for the initial M1A comparison. Use the overlay and logs to record decode time, rendering time, frame-queue delay, network latency/variance, dropped frames, codec, resolution, and actual frame rate where exposed by the selected build. Record settings, device/driver, host version, workload, and run duration alongside the readings. The owner's repeated Surface Pro 11th Edition runs already provide **initial evidence of stream parity**: native ARM64 Asteria and the official x64 Moonlight release under Windows ARM64 emulation streamed the same Apollo AV1 workload at 2560×1440 and approximately 60 FPS with no meaningful decode, render, or frame-queue regression observed. The menus/settings felt smoother in Asteria, but that observation was qualitative. See [BASELINE.md](BASELINE.md) and [VALIDATION.md](VALIDATION.md) for limits and a repeatable comparison method.

Do not build a new telemetry subsystem by default. If an investigation needs a metric the existing stats cannot provide reliably, name that metric and the decision it would inform, then add the smallest targeted measurement. Do not alter frame-pacing behavior unless repeatable measurements demonstrate a real problem or a benefit without unacceptable regressions. An M1A baseline can conclude with the existing behavior retained.

## M1B P0.5: the next hardware/presentation gate

1. Exercise a real SDL IYUV window on x64 and native ARM64 using the known decoded I420 buffer. Record texture upload, visible output, and presentation/pacing observations; the P0 CPU-buffer copy does not establish these.
2. Use SDR color/range/chroma test patterns to qualify BT.709, limited/full-range interpretation, chroma siting, and scaling. The gray-ramp/neutral-chroma roundtrip is insufficient for display color correctness. Keep HDR excluded.
3. Test resize, decoder/device recreation, repeated module lifetimes, and device-loss/recovery behavior on both targets. Three successful P0 decoder lifetimes do not qualify a long-running presentation path or device-loss recovery.
4. Review runtime deployment and normal/delay/dynamic import closure, target-native CRT/Vulkan loader/ICD requirements, clean-machine behavior, notices, and package PE architecture checks. Decide the experimental runtime shipping policy before changing package contents.
5. Qualify 4:4:4 separately after the 4:2:0 presentation path passes. Record distinct fixtures and color/conversion results; P0 proves only 1920×1080 8-bit SDR 4:2:0.

P0.5 stays offline. Do not add Session hooks, capability bits, RTSP/SDP/common-c changes, or UI settings in this gate. Preserve current bitrate, frame pacing, and normal release packaging.

## After P0.5: P1 live opt-in 4:2:0

P1 live host negotiation is not implemented or started. A separate focused change may add explicit opt-in, runtime-gated advertisement, the minimal reviewed capability/RTSP/SDP/common-c delta, and a dedicated decoder against the pinned PyroWave-enabled Pyrollo host. Ordinary Apollo/Vibepollo support must not be inferred from that fork.

Resolve Session's single-format selection and RTSP fallback contract during P1: retain a validated ordinary-codec candidate and the correct per-decoder properties. Missing SDP or failed setup must recover without feeding PyroWave to FFmpeg or replaying host-app actions. Test safe standard-codec fallback/reconnect, missing/wrong DLL/export/API/driver, unavailable GPU features, absent/partial SCM/SDP, forced standard codecs, HDR exclusion, software decoder policy, setup failure after preflight, device loss, and host encoder failure. Review container overhead, frame/packet limits, FEC/MTU, and loss tests before accepting host-controlled allocations.

The P1 exit gate is a qualified opt-in stream with pinned-host interoperability, H.264/HEVC/AV1 lifecycle/audio/input regressions, and measured behavior on named x64 and ARM64 hardware. Production latency/performance comparison and Apollo/Vibepollo end-to-end streaming remain unvalidated. Normal releases do not ship active PyroWave streaming support.

## Separate first-preview release checks

Use [VALIDATION.md](VALIDATION.md) for same-commit x64/ARM64 smoke tests, Apollo lifecycle/input/audio checks, clean-machine launch, artifact hashes, tested Windows build and driver, selected decoder, Apollo version, and known issues. These release records do not reopen M0A. PyroWave is not a first-preview prerequisite.
