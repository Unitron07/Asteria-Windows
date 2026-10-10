# Stage 5: live native PyroWave integration

Status: **Stage 5 COMPLETE / OWNER-CONFIRMED**.
Qualified implementation: `5c7ec593d9c5c27b097d194ea758b48b09a2bba4`.
[PR #41](https://github.com/Unitron07/Asteria-Windows/pull/41) merged at
`beb64659fd9217f95beaa2f92547b4933518ecdf`.
The first live run is log-verified on that implementation; additional functional
tests are owner-confirmed as recorded below. Stage 2/3/4 historical evidence
remains preserved. PyroWave stays Experimental; no release/graduation is implied.
The [owner procedure](STAGE5-OWNER-TEST.md) is retained for reproduction.

## Main audit and thread/lifecycle map

Audited authorization/main base: `000431c9056a8bdae4d9fc06ed5a8696b51a0789`.
Reviewed the live decoder/runtime/parser/GPU/queue/stats/SDL helpers, Vulkan owner,
dispatch/policy, qualified Stage 3/4 contracts and presenter/shaders, Session,
OverlayManager, roadmap/presentation documents and build/package workflows.

| Operation | Verified caller/thread and exclusion |
| --- | --- |
| Preflight construction/initialize/destruction | Session::initialize -> populateDecoderProperties -> chooseDecoder, on Qt/SDL main thread; hidden testWindow is destroyed after its test decoder |
| Live construction/initialize | Session::exec SDL event loop, initial SHOWN / decoder-recreation branch; holds m_DecoderLock |
| Stream setup callback | AsyncConnectionStartThread -> LiStartConnection -> drSetup; records negotiated properties and defers actual decoder creation |
| Live submitDecodeUnit | common-c VideoStream.c VideoDecoderThreadProc, named VideoDec; Session::drSubmitDecodeUnit holds m_DecoderLock through the complete call |
| renderFrameOnMainThread | Session::exec dispatches SDL_CODE_FRAME_READY on SDL/main thread |
| Destruction | Session::exec deferred-cleanup branch and renderer-recreation branch; toggleFullscreen's Windows branch also runs on main; all hold m_DecoderLock |
| Deferred connection cleanup | QThreadPool DeferredSessionCleanupTask asserts decoder already null, then LiStopConnection; it never destroys native WSI |
| Wakeups | Decoder/overlay notifications push one coalesced SDL frame-ready event. Finite WSI retries use one 16 ms SDL timer with no decoder pointer in its callback; teardown removes it |
| Window events | Native PyroWave handles SIZE_CHANGED, EXPOSED, MINIMIZED, MAXIMIZED, RESTORED, SHOWN and DISPLAY_CHANGED before Session's generic recreation filter |
| Overlay updates | OverlayManager publishes CPU SDL_Surface via atomic exchange, notifies IOverlayRenderer, and frees replaced surfaces. Main thread takes/frees the update and redraws retained video without another network frame |

The existing lifecycle guarantees main-thread deletion. No cross-thread teardown
handoff or broad Session redesign is required. Native WSI checks its owner thread;
Session excludes in-flight decode before deletion. Standard-codec branches remain
unchanged. Native PyroWave survives fullscreen/display refresh changes through
swapchain recreation; SDL_RENDER_DEVICE_RESET becomes a stream error.

## Production architecture and initialization-only fallback

```text
Vibepollo -> common-c/live framing -> existing strict parser
 -> borrowed Asteria Vulkan device -> native GPU decode -> caller-owned R8 Y/U/V
 -> unchanged qualified BT.709 shader -> Vulkan swapchain -> existing HWND

native initialization fails, all native resources/wrapper are destroyed
 -> existing SDL/D3D11 renderer + legacy Vulkan/D3D11 interop
 -> existing compute + CPU I420 fallback
```

Runtime loads after verified provenance; Asteria then creates/selects the Vulkan
device and borrows it through pyrowave_create_device. VkInstance/PhysicalDevice/
Device must all match before decoder creation. No default codec device is created
for a native candidate. BackendSelection is used by production initialization and
mocked fallback tests; live/fatal state forbids reselection. PyroWave remains
explicit and Experimental. Automatic, H.264/HEVC/AV1, bitrate, audio and input
selection are unchanged.

The existing owner is extended to create a Win32 Vulkan surface on the existing
SDL window's HWND. It does not require changing that window to SDL_WINDOW_VULKAN
or create another production window/SDL_Renderer. Stage 2/3/4 retain their
historical SDL Vulkan window surface path. Present-mode policy honors enableVsync
and logs the actual selection. Startup presents black after both pipelines have
been created, without sampling uninitialized Y/U/V or reading video back.

Reusable queue/feature contracts and the presenter are promoted into
app/streaming/video/pyrowave_vulkan_shared*, _presenter*, _video_policy*, _live*
and _overlays*. Historical tests consume this core. Exact-plane/offscreen RGB
verification remains in tests; its fourth image set is compiled only with
PYROWAVE_VULKAN_VERIFIER. Live resources have exactly three video sets, no video
staging/readback, and six persistent video descriptor sets.

## Slot/timeline and overlay rules

One displayed, one pending and one decoding/reusable slot are protected by one
state mutex. Each slot owns a nonexportable timeline. Decode waits its latest
consumer, signals the next decode value; sampling waits that decode value and
signals consumer completion. Pending replacement/minimize drops submit a GPU-only
wait decode / signal consumer operation before releasing the slot. No host signal
substitutes for completion. Queued retirement is not host completion: subsequent
codec acquire still waits the queued consumer value. Failed submissions leave the
payload unavailable and stop the stream.

Displayed planes remain protected until superseded. Every redraw waits the latest
consumer value and advances it again. Overflow and invalid progression fail.
Retained redraw publication starts only after successful WSI acquisition; a
pre-record retry cannot strand a displayed slot's payload when a newer frame
supersedes it. Aspect fitting uses negotiated video dimensions and preserves the
qualified 16:9 fixture geometry.
Minimize/zero drawable retires new pending frames and retains bounded state;
restore resumes recreation/rendering. Hot-path fences are queried and finite
acquire retries are timer-serviced. Device idle is limited to recreation/teardown.
The nonrecursive queue mutex protects all shared-queue submissions, present and
drains. It is never held across a PyroWave API call using codec queue callbacks.

Overlays convert CPU surfaces to RGBA32 on main, retain only the latest bounded
update (16 MiB maximum), and use three reusable texture/staging/descriptor
generations per existing overlay. Completed consumer timelines gate rewriting or
destruction. GPU uploads occur only on changed surfaces. Busy generations defer
upload; redraws reuse current textures. A separate checked fragment shader uses
straight-alpha source-over blending after video. Debug stays at top-left; status
stays bottom-left. Disable removes it immediately on the next redraw. This does
not upload or read back decoded video.

Metadata selects actual FULL/LIMITED; native CENTER is checked before packet
submission. Existing parser enforces SDR/8-bit/420/BT.709 and dimensions. LEFT,
unsupported metadata and range transitions are clearly rejected, never guessed.
Malformed parser/packet-push input before GPU submission cancels reservation;
subsequent independent frames recover. GPU/API/WSI fatal errors enter decoder
getError -> Session cleanup/reconnect, with no hot fallback.

## Explicit teardown and diagnostics

Session excludes decode; stop new work; GPU-retire outstanding pending slot;
drain caller queue/device for outstanding consumers; destroy decoder; report
resolved native performance data; destroy borrowed wrapper; unload runtime after
wrapper destruction; destroy overlays/presenter; drain/destroy WSI resources;
destroy Asteria device/surface/instance; release its System32 loader. Owner
create-info/queue callback storage survives wrapper destruction. Cleanup is
idempotent and does not rely on process exit. The Vulkan dispatch retains its
loader until the final caller handles are gone.
The initial consumer drain is attempted even when GPU drop retirement throws;
retirement/drain faults are recorded without skipping the resource safety gate.

Every native decoder lifetime logs PYROWAVE_NATIVE_STREAM_SUMMARY JSON with
testOnly, exact source/runtime identities, backend/path/handle matches, lifetime
frame/drop counts, retirement/reuse/timeline counts, CPU readback API count,
recreations/overlay uploads/redraws, fatal/validation/cleanup state and CPU API
duration totals. Runtime explicitly refuses CPU output while borrowing a native
device and counts actual synchronous CPU calls; native cpuYuvReadbackFrames must
be zero. No end-to-end latency is claimed. Validation unavailable is SKIP.

## Build, provenance and CI

Normal application builds dynamically load only System32 vulkan-1.dll, with no
mandatory vulkan-1.lib import, bundled loader, SDK or runtime shader compiler.
The qualified video GLSL/SPIR-V/header/provenance are byte-identical to Stage 4.
Overlay provenance is separate and reproducible with the same checked compiler.
Runtime stays PyroWave `186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream
`186f0393`, API 0.6.0, Granite `b6cffd5ce81f540f0855e6778428483e14763d9b`,
with the already-approved patches, including borrowed factory cleanup. No patch
or dependency pin changes are part of Stage 5.

Stage 5 x64/ARM64 CI compiles production core and runs fallback, slot, retirement,
redraw, overflow, thread and actual overlay/dispatch mocks plus Stage 2/3/4
regressions. Existing Windows baseline and PyroWave workflows remain required.
The normal exact-head Windows baseline packages include Asteria, runtime,
shader provenance/source/bytes, notices, source revision, manifest/PE inventory,
owner verifier and checklist. All six exact-head workflows/fourteen jobs passed:
[Windows x64/ARM64 Asteria and upstream baseline](https://github.com/Unitron07/Asteria-Windows/actions/runs/37718066589),
[Stage 5](https://github.com/Unitron07/Asteria-Windows/actions/runs/37718066450),
[Stage 4](https://github.com/Unitron07/Asteria-Windows/actions/runs/37718066518),
[Stage 3](https://github.com/Unitron07/Asteria-Windows/actions/runs/37718066439),
[Stage 2](https://github.com/Unitron07/Asteria-Windows/actions/runs/37718066425)
and [PyroWave regressions](https://github.com/Unitron07/Asteria-Windows/actions/runs/37718066447).
Stage 5 had 9 PASS / 3 GPU SKIP per architecture. Hosted GPU absence and Vulkan
validation remain SKIP; real Surface execution is separate evidence below.

## Stage 5 owner qualification record

**FIRST LIVE RUN VERIFIED IN LOG ON EXACT HEAD.** The supplied reviewed record
for `Asteria-1791668616.log` and the visible overlay screenshot is from normal
ARM64 Asteria on Surface Pro 11 / Snapdragon X Plus / Qualcomm Adreno X1-85,
connected to normal Vibepollo. The summary is `testOnly=false` at the qualified
implementation SHA above. No additional host configuration or logs are invented.

```text
Vibepollo -> common-c PyroWave framing/parser -> same Asteria-owned Vulkan device
 -> fragment GPU decode -> caller-owned GPU-resident Y/U/V
 -> qualified Stage 4 BT.709 shader -> Vulkan swapchain
```

| First-run summary | Recorded values |
| --- | --- |
| Selected GPU / backend | Qualcomm Adreno X1-85 / NATIVE_VULKAN |
| Preferred / actual decoder path | fragment / fragment |
| borrowedInstanceMatch / borrowedPhysicalDeviceMatch / borrowedDeviceMatch | true / true / true |
| presentationPath | GPU_DECODE_CALLER_YUV_SHADER_SWAPCHAIN |
| cpuYuvReadbackFrames / externalMemoryHandles / externalSemaphoreHandles / d3d11Resources | 0 / 0 / 0 / 0 |
| slots / slotReuse | 3 / 19628 |
| receivedFrames / decodedFrames / renderedFrames | 19631 / 19631 / 19538 |
| presentationDrops / nativeRetiredDrops | 93 / 93 |
| decodeTimelineWaits / consumerTimelineSignals | 19878 / 19878 |
| timelineErrors / fatalPresenterErrors / cleanupOkay | 0 / 0 / true |
| overlayUploads / retainedFrameRedraws / overlayRedraws | 213 / 247 / 0 |
| swapchainRecreations / networkDrops | 2 / 30 |
| validation / validationErrors / validationWarnings | SKIP / 0 / 0 |

The first log proves native GPU-resident presentation, correct dropped-slot
retirement, bounded slot reuse and clean teardown. It alone does not prove five
reconnects or overlay-only redraw: `overlayRedraws=0` in this run.

**FIVE RECONNECTS/OVERLAY-ONLY/STANDARD-CODEC/AUDIO-INPUT OWNER-CONFIRMED.**
The owner explicitly reports successful completion of:

- Five connect/stream/disconnect/reconnect cycles in the SAME Asteria process,
  with resize, minimize/restore, maximize/restore and fullscreen/windowed changes.
- Functional overlay-only updates on paused or low-frame-rate content, with
  debug/status overlays functioning.
- A standard-codec streaming smoke test with normal behavior.
- Audio/input functioning across reconnect cycles.

These functional confirmations are separate from the first log's recorded
counters. No five additional JSON summaries or exact counters are asserted;
`overlayRedraws>0` has not been machine-verified in the available evidence.
Full-range live mode is log-verified; precise LIMITED-range live evidence is
unavailable, while Stage 4 independently qualified FULL and LIMITED.

The screenshot sample was 2560x1440 at about 119.91 incoming/decoding/render FPS,
0.38 ms decode CPU API time, 0.34 ms frame queue delay and 0.27 ms render CPU time.
Shutdown codec GPU timing was iDWT fragment 1.886 ms/frame and Dequant
0.782 ms/frame. Async CPU API/submission time is not GPU execution duration;
these samples are not an end-to-end/scanout latency measurement or matched
performance benchmark.

Validation remains **SKIP**, not PASS. The native success run did not exercise
initialization-time legacy GPU interoperability/CPU-I420 fallbacks, which remain
available. Forced physical device-loss recovery, broader/x64 live hardware and
controlled performance qualification remain separate. The next phase is
[controlled native performance qualification and v1.0 polish](../../../docs/NEXT_STEP.md#next-phase-controlled-performance-qualification-and-v10-polish),
with no code optimization, release, Experimental graduation, MultiSeat or VR work
included in this qualification record.
