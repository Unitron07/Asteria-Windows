# Phase 2: native Vulkan pacing and presentation

Phase 1B is **PAUSED at the owner's request**. PR #46 stays draft. Its ten-run
block and all later failed attempts remain unchanged. The unresolved bitrate
pilot included multiple stream lifetimes; it is not a qualified comparison.
Phase 2 does not require further multi-run benchmarks.

## Evidence and limits

Surface Pro 11 ARM64 has completed ten native PyroWave 1440p120 runs. Some runs
showed submission-interval spikes and presentation drops. Those observations
do not identify a bottleneck or establish a causal profiling benefit. Code
review can identify avoidable work; it cannot quantify the benefit on hardware.

## Lifecycle review

| Area | Current implementation and implication |
| --- | --- |
| Decode/publication | `NativePresentation::decode()` serializes native state, reserves one of three slots and submits decode on the borrowed device. `LiveSlots::publish()` replaces an older pending frame. This deliberately bounds latency, rather than building a video queue. |
| Retirement | `Presenter::retire()` submits a GPU timeline wait/signal for a dropped decode. Slot reuse waits on that consumer value; a failed submission retains the payload. Retirement cannot simply be removed to reduce drop overhead. |
| Scheduling | `wakeFrame()` coalesces decode/overlay/window notifications. The separate one-shot SDL retry timer can also post a frame-ready event. An event is a wakeup and does not prove there is new work. |
| WSI | `Probe::draw()` queries command/acquisition fences, uses a finite 1 ms acquire, and returns for a later retry on not-ready/timeout. Two command resources and per-image present semaphores remain bounded. |
| Synchronization | Decode, sampling and retirement use monotonic per-slot timeline payloads. Queue operations use a separate nonrecursive queue mutex. `render()` holds the native state mutex through draw/present, so unnecessary calls also exclude decode publication. |
| Retained images | Before this change, every render wake with a displayed slot could acquire, submit, advance the consumer timeline and present again, even without video, overlay or window work. |
| Recovery/teardown | Window events request swapchain recreation; finite retries must preserve redraw demand even after recreation clears its flag. Shutdown retires pending decode, independently drains consumers, then destroys borrowed codec resources and the owner. |

## First focused improvement

Gate native draws on actual demand: pending video, changed/deferred overlays,
window/swapchain rebuild, or an unfinished draw retry. Ignore an idle wake before
entering WSI. Successful clean draws clear retry demand. Failed finite draws keep
it, including the recreation-only iteration. Accepted submissions that request a
rebuild also keep it, allowing the existing bounded timer to recover without
waiting for another video frame.

This avoids redundant acquire/submit/present calls and timeline advancement,
and avoids their additional time under the native state mutex. It should reduce
unnecessary contention and GPU/WSI work when stale wakes occur. There is no
claim that these wakes caused the observed spikes, or that measured drop rates
will improve. Legitimate retained-frame redraws remain supported.

The change leaves slot count/replacement, decode paths, timeline synchronization,
shaders, present modes, capture formats, codec pins and the 16 ms retry timer
unchanged. Standard codecs and legacy PyroWave fallback are outside the gate.

## Implementation order

1. Land only the demand gate and focused automated regressions in a separate
   draft PR; run existing x64/ARM64 policy, resource, shader and application CI.
2. Perform the short optional owner smoke test below when its build is ready.
   Keep Phase 1B paused; no profiling/bitrate comparison or repeated runs.
3. Review retry scheduling next: the fixed 16 ms timer exceeds a 120 FPS frame
   period (8.33 ms). Any change needs explicit bounded retry/coalescing tests and
   consideration of SDL timer granularity and CPU wake cost. No timing tuning is
   included in this first change.
4. Consider narrower native mutex ownership only if future evidence warrants it.
   Separating decode/publication from presentation changes slot/timeline ownership
   and is higher risk. Do not change locking or WSI modes speculatively.

## Automated validation

`tests/pyrowave/vulkan/stage5_policy_tests.cpp` exercises idle duplicate wakes,
video and overlay demand, window recreation, repeated finite retries, accepted
submission plus rebuild, and a new frame satisfying retained retry demand.
The existing randomized 200000 slot operations, retirement-failure checks and
cross-thread queue-lock tests remain. Existing Stage 5 resource mocks cover real
timeline retirement, retained redraw submission and deferred overlay generations.
CI additionally compiles the production core and full application for x64/ARM64.
GPU-unavailable CI cases may skip; that is not owner hardware qualification.

Policy-only build (no GPU, codec runtime, SDL or Vulkan SDK required):

```powershell
cmake -S tests/pyrowave/vulkan -B build/phase2-policy
cmake --build build/phase2-policy --config Release --parallel 2
ctest --test-dir build/phase2-policy -C Release --output-on-failure
```

## Owner smoke test, when CI builds are ready

Use the **candidate Asteria-Windows-arm64** portable artifact from the Phase 2
PR's Windows baseline run on Surface Pro 11. Extract to a new folder; keep the
qualified old package and every Phase 1B evidence directory. No toolkit or
manifest editing is needed. Keep profiling disabled and use normal settings.

1. Launch the candidate and start one PyroWave stream at the usual resolution,
   FPS and bitrate. Watch moving content for about a minute and check input/audio.
2. Toggle the statistics overlay; resize, minimize/restore and expose the window.
   Verify video resumes and overlays update without frozen/black output.
3. Disconnect normally. Keep the normal console log and note the build commit,
   settings, any visible freeze and whether shutdown completed. Check the real
   stream's native summary for `NATIVE_VULKAN`, actual decoder path, zero fatal/
   timeline errors and successful cleanup. Keep preflight and live lifetimes
   distinct; do not combine them as benchmark evidence.
4. Optionally start one ordinary H.264/HEVC/AV1 stream as a compatibility smoke
   check, then disconnect. No repeated measurements are requested.

Report a smoke-test failure with its original log; do not overwrite past evidence.
This is a functional check, not proof of pacing improvement. Keep both PRs draft;
no merge or release without explicit owner approval.
