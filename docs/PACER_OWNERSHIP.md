# Shared FFmpeg Pacer: first targeted implementation

Base: `2c299babdb14a511f614850beaf068af235cd028` (latest main at start).
Upstream PRs #47/#49/#56/#57 and native PyroWave demand gating (#50) are preserved.
PR #51 is merged; Phase 2 cross-codec optimization is active. PR #46 is closed
without merging; Phase 1B remains paused. Historical hardware records are unchanged.

## Confirmed defect and correction

Each Pacer queue independently retains up to three AVFrames. A stalled renderer
can additionally hold its current frame and the deferred previous frame. That
permits eight retained surfaces, exceeding the previous advertised budget of five.
VBlank catch-up can detach another frame for `av_frame_free()` outside the queue
mutex; while its finalizer is blocked, the producer can refill the pacing queue.
The complete bound is therefore **nine**, not eight:

| Ownership location | Maximum |
| --- | ---: |
| Pacing queue | 3 |
| Render queue | 3 |
| Rendering, old deferred release, or render-side catch-up release | 1 |
| Deferred frame | 1 |
| VBlank catch-up release outside the mutex | 1 |

The single FFDecoder producer's incoming output remains caller-owned until queue
transfer; it is not an extra queued frame. A frame moving between queues remains
under the mutex. Only one renderer and one VBlank worker consume each Pacer.
Render-side catch-up happens after the old deferred frame is freed, so it reuses
the rendering allowance. No detached list of arbitrary size is introduced.

`PACER_MAX_OUTSTANDING_FRAMES`, already used by FFmpeg's `extra_hw_frames`, now
covers these owners. Both queue caps, oldest replacement, history windows,
catch-up thresholds, VBlank slack and deferred-release ordering are unchanged.
This requests four more hardware surfaces; it does not increase queue depth or
establish a measured FPS/latency gain. Actual driver pool starvation has not been
hardware-profiled. Pool allocation remains subject to FFmpeg/backend limits.
AV1 Main8/Main10, HEVC/H.264 selection, metadata, HDR and shader paths are unchanged.

Saturation replacement previously freed frames without incrementing the drop
counter. Every Pacer drop now counts exactly once. A small stats mutex serializes
Pacer writers and FFDecoder's existing window snapshot/reset. GPU rendering and
AVFrame finalizers do not run under the stats mutex. Queue-to-stats is the only
nested lock order; FFDecoder holds only stats during its snapshot/reset.

Shutdown uses an atomic stop flag and publishes stop/wakes under the queue mutex,
preventing a worker from missing shutdown between its empty check and wait.
FFDecoder still joins its producer before destroying Pacer; Pacer joins workers
before draining frames, and codec/device teardown still follows Pacer destruction.
Concurrent calls on an already-destroyed Pacer are not supported.

## Releasing saturation drops outside the mutex

Gemini's suggestion was evaluated and deferred. These releases currently occur
under the queue lock; the existing VBlank/render catch-up releases already unlock.
Moving producer saturation release out adds a detached surface while both queues
can refill. Simply unlocking around the existing helper also invalidates its
check/drop/enqueue transaction. A later implementation must carry detached owners
in the pool budget, count drops once, preserve publication/notification order and
join all release work before codec/device destruction. It needs separate evidence
of useful contention reduction; the present correction preserves the transaction.

## Focused regression coverage

`tests/pacer` compiles the actual production `pacer.cpp`, with real Qt queue/mutex/
condition primitives, SDL threads/events and reference-counted FFmpeg AVFrames.
Only Session/overlay/renderer/display dependencies are substituted. Manual VBlank
ticks and gated renderer/AVBuffer finalizers force specific interleavings, with
finite watchdogs and no scheduling sleeps. The key regression proves eight owners,
then nine while VBlank release is blocked; the old five-frame declaration fails
that bound. Other cases cover strict/lenient pacing, late/empty VBlank, saturation,
oldest selection, an external AVBuffer reference, deferred lifetime, stale events,
concurrent enqueue/drop/render/stats rollover, NO_BUFFERING, pacing on/off,
reconnects and waiting-worker teardown. A finite concurrent stress case supplements
the deterministic ownership schedules; it does not exhaust all interleavings.

The candidate baseline workflow runs this suite on native x64 and ARM64 with its
existing pinned Qt/SDL/FFmpeg dependencies, then builds/packages the full client.
Headless tests do not qualify real D3DKMT timing or GPU/driver behavior.

Baseline CI: [post-merge run 41148537600](https://github.com/Unitron07/Asteria-Windows/actions/runs/41148537600)
passed all x64/ARM64 build, package and launch steps. Candidate ARM64 was cancelled
during post-checkout cleanup, after those steps and artifact uploads succeeded.
[PR #57 run 41145730301](https://github.com/Unitron07/Asteria-Windows/actions/runs/41145730301)
passed all four upstream/candidate architecture jobs. No unrelated rerun is needed.

## Separate next candidates

- D3D11VA redundant binding and same-size overlay resource reuse: inspect context
  invalidation and concurrent COM ownership first; test NV12/P010, changed/disabled
  overlays, resize and allocation failure. Keep shared-device synchronization.
- Legacy PyroWave demand gating: port #50's demand semantics only after covering
  window damage, overlay disable, SDL recovery/retry, displayed-slot fences and
  reconnect. Native and legacy paths stay independently functional.
- Qualcomm copy/shared-device overhead: retain current defaults. Any bind/copy or
  device-policy change is a separate opt-in experiment requiring Adreno X1-85
  validation for Main8/Main10, HDR, resize and teardown. No such change is bundled.

## Owner smoke test

Extract the candidate ARM64 portable artifact into a fresh folder, keep the old
package and use ordinary settings/logging with profiling off. Play AV1 at the usual
resolution/FPS for a few minutes; toggle pacing once, statistics overlay, resize
and minimize/restore. Check video/colors, input/audio, disconnect and reconnect once.
If already available, briefly check AV1 Main10/HDR, HEVC and H.264, then ordinary
PyroWave. Do not force legacy/backend overrides. Save the normal log and report
the build SHA and any freeze/artifact/cleanup failure. Unavailable HDR stays untested.
No toolkit, manifest, repeated benchmark campaign, merge, tag or release is needed.
