> Owner update: live P1a video succeeded on Surface Pro 11 / Snapdragon X Plus /
> Adreno X1-85 at 2560x1440, target 120 FPS, with audio/input working. The CPU
> bring-up path measured ~93.3 FPS, ~12.45 ms decode pipeline, ~1.24 ms render.
> GPU-resident shared-plane presentation and standard-layout stats are now added.
> Final performance qualification remains **PENDING owner x64/ARM64 retest**.
> See [GPU presentation and timing definitions](../../docs/PYROWAVE_GPU_PRESENTATION.md).

# P1a GPU performance owner retest (qualification PENDING)

Use `Asteria-P1a-Experimental-x64-<run>` on RTX 4070 Ti, or the matching
`arm64` package on Surface Pro 11 / Snapdragon X Plus / Adreno X1-85.
Extract the artifact, then extract its `Asteria-P1a-Experimental-<arch>.zip`.
Run `Asteria.exe` from the extracted directory. No local rebuilding is required.
The portable marker keeps this test's settings/pairing beside this executable.
Use the included `pyrowave/` directory intact; do not copy a different codec DLL.

1. Pair/connect to Vibepollo using the ordinary UI.
2. Open Settings â†’ Video codec â†’ **PyroWave (Experimental)**. Turn HDR and YUV
   4:4:4 off. Keep decoder Auto or Force hardware. Initially use 1920x1080 at
   60 FPS, default packet size, and record the configured bitrate. PyroWave
   needs substantially more bandwidth than conventional codecs; this build
   does not calibrate, probe or automatically change bitrate.
3. Start/resume an app. Check the log for local API 0.6.0, bitstream `186f0393`,
   Vulkan adapter, pinned HTTPS SCM support, exact DESCRIBE marker and matching
   host ID, negotiated format `0x10000`, extent/FPS, first complete compatibility
   decode unit, packet count/bytes, successful GPU output or fallback I420 decode and SDL initialization.
   The first valid sequence must log `PyroWave sequence: BT.709 full-range, SDR 4:2:0`
   or `limited-range`, followed by the first successful decode diagnostic.
   Confirm `GPU presentation initialized` and the compute/fragment decoder path;
   a CPU fallback must log its specific initialization failure.
   Retest full-range Vibepollo output that previously caused every frame to be rejected.
4. Confirm continuously updating video, clean output, audio, controller/mouse/
   keyboard input, resize/fullscreen, minimize/restore and clean disconnect.
5. Reconnect using PyroWave. Then select Auto, H.264, HEVC and AV1 and verify
   standard streams still work, including normal HDR where supported. Auto
   must continue to select standard codecs.
6. Exercise a standard-only host and missing/renamed runtime or metadata. The
   app must still launch; PyroWave attempts must report a clear rejection and
   a subsequent standard-codec attempt must work. Missing/mismatched host ID
   must fail at DESCRIBE before receiving codec frames.
7. Open View logs from Asteria's menu. Copy the current log, or run the included
   `collect-pyrowave-logs.ps1` after disconnect to collect portable logs. Fill in
   `OWNER-RESULT.txt`; attach the client log, relevant host log, artifact/run/hash,
   Windows and driver versions, adapter, resolution/FPS/bitrate and failures.

Failures use normal cleanup and **manual retry**. Select a standard codec and
reconnect/resume the existing app. The client never automatically replays a host
launch/resume command or switches codec within a running PyroWave stream.

Presentation is SDR 8-bit I420, BT.709 full or limited according to the parsed
sequence, with explicit SDL3 texture colorspace, aspect fit and linear scaling.
Range changes within a connection are rejected with a diagnostic; reconnect to
establish a different range. The first Surface test passed negotiation/runtime,
audio and video packet receipt but rejected every full-range frame. This fix
was followed by successful live video/audio/input on ARM64. The GPU presentation
performance retest remains **PENDING**.
Source chroma is CENTER; qualified SDL2-compat metadata uses LEFT. No visible
issue occurred during P0.5 tests; exact phase remains formally unqualified.
V-sync follows the ordinary Session setting; P1a uses a bounded latest-frame
handoff rather than introducing advanced pacing. The statistics overlay now follows standard Moonlight ordering, with host
latency, network/jitter percentages, RTT, decode, queue and render timing plus
bitrate and stage timing. GPU decode timing is CPU submission, not GPU execution;
CPU fallback decode/readback is combined.
Full latency/pacing and true GPU loss are not qualified by a successful stream.

Supported allocations are even dimensions 128..4096 with at most 3840Ã—2160
pixels; only 1080p was previously hardware-qualified offline. Live complete
envelopes are limited to 8 MiB and 65,536 codec packets; transport packet sizes
are 1024..2048 bytes and decode units contain at most 4,000 transport fragments.
Rejected independent frames clear decoder state and the next valid frame can
recover. Consecutive malformed input is logged with rate limiting.

P1b/later exclusions: live record framing, record-start/lost-buffer metadata,
critical-packet handling, adaptive FEC, partial recovery, sideband readiness,
bandwidth probing, bitrate usability tuning, 4:4:4, HDR and advanced pacing.
CI build/test success is not performance qualification. At 2560x1440/120 FPS
record incoming/decoded/rendered FPS, host latency min/max/average, network and
jitter/presentation drops, RTT/variance, decode timing mode, frame queue delay,
render time, bitrate and extra stages. Compare to the ~93.3 FPS / ~12.45 ms old
decode pipeline / ~1.24 ms render baseline, allowing for the changed decode-time
definition. Record actual results in `docs/VALIDATION.md`; no hardcoded timing
target or P1a performance qualification is implied by these artifacts.
