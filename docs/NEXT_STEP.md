# Next step: M1A performance instrumentation

M0A native Windows ARM64 is complete based on the owner's real-device validation. M1 identity/rebrand is implemented; profiles and session workflows remain planned. The next development task is **M1A low-overhead streaming telemetry** on x64 and ARM64, followed by repeatable benchmark output. Do not change frame pacing defaults before measurements show a benefit.

## Completed M0A baseline

- x64 and ARM64 upstream/reference and Asteria CI builds, portable packaging, and final-ZIP architecture checks passed in [run 34797782854](https://github.com/Unitron07/Asteria-Windows/actions/runs/34797782854).
- The owner tested a Surface Pro 11th Edition with Snapdragon X Plus and 16 GB RAM and verified that the Asteria process runs as native ARM64.
- On the same device, native ARM64 Asteria and the official x64 Moonlight release under Windows ARM64 emulation streamed the same game repeatedly at 2560×1440, approximately 60 FPS, AV1 through Apollo. Streaming was effectively identical, without meaningful decode, render, or frame-queue regression. Asteria's menus/settings felt noticeably smoother and snappier; this was qualitative, not timed.
- There is no official native ARM64 upstream Moonlight release. CI-built unmodified upstream ARM64 artifacts are internal engineering references, not official upstream releases.
- Apollo is the owner's host for this project and preview. Sunshine is not a required qualification target.

See [BASELINE.md](BASELINE.md) for evidence and unrecorded details. M0A completion does not imply a controlled benchmark or a fully qualified public release.

## M1A implementation sequence

1. Add aggregated, low-overhead timestamps/counters for packet or frame arrival, decode start/end, presentation-queue entry, present request/completion, queue depth, dropped/repeated frames, and relevant CPU/GPU use where reliable.
2. Export structured results with exact client build, host, hardware, driver, codec, resolution, frame rate, bitrate, network, workload, and run duration.
3. Compare native ARM64 Asteria to the official emulated x64 Moonlight release for the practical user-facing comparison. Use a CI-built unmodified upstream ARM64 artifact only as an optional internal same-architecture reference.
4. Run repeatable same-device tests before prototyping presentation policies or the optional PyroWave path. Keep the existing pacing behavior as the baseline.

## Separate first-preview release checks

Use [VALIDATION.md](VALIDATION.md) for same-commit x64/ARM64 smoke tests, Apollo lifecycle/input/audio checks, clean-machine launch, artifact hashes, tested Windows build and driver, selected decoder, Apollo version, and known issues. These release records remain useful, but they do not reopen M0A. Publish portable ZIPs only with claims tied to the paths actually tested.
