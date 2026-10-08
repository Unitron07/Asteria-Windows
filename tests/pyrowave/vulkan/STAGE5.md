# Stage 5: live native PyroWave integration

Status: **Stage 5 IMPLEMENTED / LIVE OWNER QUALIFICATION PENDING**.
The draft implementation requires exact-head CI and the real Surface procedure
in [STAGE5-OWNER-TEST.md](STAGE5-OWNER-TEST.md). Do not merge or release.
Stage 4 remains the historical owner-qualified color/presenter evidence; Stage 5
does not imply successful live hardware qualification.

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

Every native decoder lifetime logs PYROWAVE_NATIVE_STREAM_SUMMARY JSON with
testOnly, exact source/runtime identities, backend/path/handle matches, lifetime
frame/drop counts, retirement/reuse/timeline counts, CPU readback API count,
recreations/overlay uploads/redraws, fatal/validation/cleanup state and CPU API
duration totals. Runtime explicitly refuses CPU output while borrowing a native
device and counts actual synchronous CPU calls; native cpuYuvReadbackFrames must
be zero. No end-to-end latency is claimed. Validation unavailable is SKIP.

## Build, provenance and remaining qualification

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
owner verifier and checklist. GPU absence is SKIP. Full live Surface qualification,
visual/audio/input/overlay/window parity, five reconnects and standard-codec smoke
test are still required; desirable x64 hardware qualification is separate.
