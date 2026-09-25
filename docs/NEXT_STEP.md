# Next step: M1B Vibepollo/PyroWave protocol spike

M0A native Windows ARM64 is complete based on the owner's real-device validation. M1 identity/rebrand is implemented. **The next development task is M1B groundwork:** pin and inspect the Vibepollo/PyroWave implementation, then define a small, testable Asteria integration boundary. Begin with the [source audit and open questions](M1B_PYROWAVE_SPIKE.md).

## M1A baseline decision

Moonlight PC's existing performance statistics are sufficient for the initial M1A comparison. Use the overlay and logs to record decode time, rendering time, frame-queue delay, network latency/variance, dropped frames, codec, resolution, and actual frame rate where exposed by the selected build. Record settings, device/driver, host version, workload, and run duration alongside the readings. The owner's repeated Surface Pro 11th Edition runs already provide **initial evidence of stream parity**: native ARM64 Asteria and the official x64 Moonlight release under Windows ARM64 emulation streamed the same Apollo AV1 workload at 2560×1440 and approximately 60 FPS with no meaningful decode, render, or frame-queue regression observed. The menus/settings felt smoother in Asteria, but that observation was qualitative. See [BASELINE.md](BASELINE.md) and [VALIDATION.md](VALIDATION.md) for limits and a repeatable comparison method.

Do not build a new telemetry subsystem by default. If an investigation needs a metric the existing stats cannot provide reliably, name that metric and the decision it would inform, then add the smallest targeted measurement. Do not alter frame-pacing behavior unless repeatable measurements demonstrate a real problem or a benefit without unacceptable regressions. An M1A baseline can conclude with the existing behavior retained.

## M1B investigation sequence

1. Pin the host, protocol, codec, and client reference revisions. The [September 24 PyroWave streaming handoff](https://github.com/joemossjr16/pyrowave-streaming) points to a Vibepollo fork and Moonlight Qt/protocol changes; verify the exact refs before implementation.
2. Trace opt-in negotiation from host capability through RTSP/SDP to selected video format. Confirm how unsupported hosts, disabled settings, and unavailable decoders fall back.
3. Trace video transport, `PYRW` frame framing, size limits, packet loss/FEC, and decode-unit boundaries; distinguish the private Moonlight container from the upstream PyroWave bitstream.
4. Audit Windows x64 and native ARM64 Vulkan/PyroWave runtime dependencies, decoder and renderer integration, SDR color and 4:2:0/4:4:4 handling, and HDR metadata or explicit HDR exclusion.
5. Write a narrow integration proposal and an interoperability test matrix against a pinned host. Preserve Asteria's H.264/HEVC/AV1 paths, pairing, audio, and input.

The [M1B spike notes](M1B_PYROWAVE_SPIKE.md) record confirmed findings and unresolved checks. No PyroWave feature is claimed for Asteria yet.

## Separate first-preview release checks

Use [VALIDATION.md](VALIDATION.md) for same-commit x64/ARM64 smoke tests, Apollo lifecycle/input/audio checks, clean-machine launch, artifact hashes, tested Windows build and driver, selected decoder, Apollo version, and known issues. These release records do not reopen M0A. PyroWave is not a first-preview prerequisite.
