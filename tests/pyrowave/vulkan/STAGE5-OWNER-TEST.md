# Stage 5 live owner procedure

LIVE OWNER QUALIFICATION PENDING. DO NOT MERGE. Use the exact-head normal ARM64
Asteria portable package on Surface Pro 11 / Snapdragon X Plus / Adreno X1-85.

1. Extract the ZIP to a new directory. Verify the SHA printed in CI's evidence
   artifact. From that directory, run Windows PowerShell or PowerShell 7:
   `./verify-stage5-owner-package.ps1 -ExpectedSourceRevision <PR-head-SHA> -Architecture arm64 -ArchivePath <ZIP> -ExpectedArchiveSha256 <CI-ZIP-SHA256>`.
   This verifies source, architecture, inventory hashes, runtime and shader
   provenance. Do not mix files from prior packages.
2. Launch `Asteria.exe` normally. Pair/authenticate manually if needed and select
   **PyroWave (Experimental)** explicitly. Connect to the normal Vibepollo host.
   Prefer 2560x1440, 120 FPS target, SDR 8-bit 4:2:0 BT.709 CENTER. Use the actual
   host output rate; there is no arbitrary FPS/latency pass threshold.
3. Run a representative workload for several minutes. Confirm live image,
   FULL/LIMITED colors, Y/U/V and RGB ordering, orientation, centered 16:9 fit,
   black bars, chroma alignment, and no recurring corruption/stale-frame bursts.
   Confirm audio and input.
4. Enable/disable the debug overlay, trigger a status/update overlay when
   practical, and inspect readable text, correct alpha and top/bottom placement.
   Exercise multiple UI updates during paused or infrequent video output so
   overlay-only redraw is observed. Confirm no stale textures/flicker.
5. Resize, minimize/restore, maximize/restore, fullscreen/windowed, and change
   displays when available. Confirm safe resume without native-to-legacy switching.
6. Perform at least five complete connect/stream/disconnect/reconnect cycles in
   the same Asteria process. Confirm no stale frame, hang, crash or overlay
   registration problem; audio/input and native initialization recover each time.
7. Smoke test one standard codec. Do not change Automatic codec behavior.
8. Use the existing View logs feature and `collect-pyrowave-logs.ps1`. Return the
   complete log for every cycle and a written checklist with actual settings,
   host commit, Windows/driver versions, GPU, source SHA and package SHA. Return
   any validation ERROR/WARNING and any fallback/fatal reason. Keep unavailable
   Vulkan validation as SKIP. Do not send credentials/private keys.

For each live stream (summary `testOnly=false`), return the parseable
`PYROWAVE_NATIVE_STREAM_SUMMARY` line. Required evidence:

- X1-85 selected; backend=NATIVE_VULKAN; preferred/actualDecoderPath=fragment.
- borrowedInstanceMatch/borrowedPhysicalDeviceMatch/borrowedDeviceMatch=true.
- presentationPath=GPU_DECODE_CALLER_YUV_SHADER_SWAPCHAIN.
- cpuYuvReadbackFrames=0; externalMemoryHandles=0; externalSemaphoreHandles=0;
  d3d11Resources=0.
- slots=3; slotReuse>0; decodeTimelineWaits and consumerTimelineSignals advance;
  nativeRetiredDrops demonstrates dropped unpresented frames when pressure/minimize
  produces them. Absence of drops is not proof of the retirement hardware path.
- timelineErrors=0; fatalPresenterErrors=0; cleanupOkay=true.
- Received/decoded/rendered frames, network/presentation drops, recreations,
  overlayUploads/overlayRedraws, CPU API durations, and native GPU timing reports.

Synthetic/offline tests and successful compilation do not count as live owner
qualification. Stop and return the evidence for review; do not merge or release.
