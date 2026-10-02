# PyroWave P1a GPU presentation and stats

The owner reports the first successful live P1a video on Surface Pro 11,
Snapdragon X Plus / Adreno X1-85: 2560x1440, target 120 FPS / 120 Hz. Video
rendered, audio/input worked, and visual quality and responsiveness felt good.
The CPU-output bring-up path measured approximately 93.3 incoming/decoded/rendered
FPS, 450.9 Mbps, 21 network drops, one presentation drop, 12.45 ms decode pipeline,
1.24 ms render, and 13 ms RTT. These are owner observations, not measurements of
this change. P1a performance qualification remains **PENDING owner retest**.

## Chosen path and pinned API evidence

Codec commit `186f0393b77f7755953b5ecde994bb1cec2e4155`, C API 0.6.0 is unchanged.
`pyrowave_decoder_decode_gpu_buffer` writes into caller-provided UNORM image
views; it does not return an owned output image. Three R8_UNORM images retain
the native Y/U/V planes. The application supplies explicit external acquire and
release references and a timeline semaphore; the release signal flushes the
codec's queued command buffers. Layouts are GENERAL. Acquire from
QUEUE_FAMILY_IGNORED deliberately discards the old image with UNDEFINED, exactly
as documented by the API. Release transfers ownership to QUEUE_FAMILY_EXTERNAL.

The implementation follows the pin's Windows interop direction: D3D11 creates
shared NT-handle textures and a shared D3D11 fence; PyroWave imports both. D3D11
fences use the D3D12_FENCE alias and TIMELINE type. Successful import transfers
ownership of the NT handle to PyroWave. SDL3 in the pinned v15 dependency wraps
separate D3D11 Y/U/V texture pointers, with the same explicit BT.709 full/limited
colorspace values as the CPU path. No new shader, swapchain, Vulkan loader import,
or codec runtime import is required. Existing aspect fitting, linear scaling,
overlays, resize behavior and V-sync presentation remain SDL-owned.

GPU decode follows `pyrowave_decoder_device_prefers_fragment_path`: proprietary
Qualcomm uses the pin's fragment path and R8 color attachments; desktop drivers
use compute and R8 storage images. Failed GPU preflight recreates the original
CPU bring-up decoder before starting live packets.

Cross-API output is enabled only after adapter LUID equality, ID3D11Device5 /
Context4 availability, every real R8 output-image import, every timeline fence
import, and both range-specific SDL wrappers succeed. This is a capability gate
on both x64 and ARM64, not a claim that all NVIDIA or Qualcomm drivers support it.
If any initialization step fails, the reason is logged and the already validated
CPU I420 renderer remains usable. Partial resources are destroyed. A live device
loss/submission failure ends the stream through normal Session cleanup; it does
not replay host launch or silently continue with suspect GPU resources.

A separate Vulkan presenter was considered. It would require another surface,
swapchain, YUV shader/conversion and overlay integration (or changes to the
borrowed-device creation contract). Because this pin and SDL already support
D3D-owned planes and imported timeline fences, gated sharing keeps the existing
presentation machinery. Unsupported drivers retain CPU output until hardware
qualification establishes what further work is needed.

## Lifetime and latency

Three frame slots allow one displayed frame, one rendering frame, and one pending
frame. The displayed slot stays protected for overlay-only redraw. Publication
replaces an older pending frame; a full set recycles only that pending slot.
Discarded pending frames still have an outstanding decode fence: subsequent
decode waits for that exact payload before discarding/reusing the allocation.
There is no unbounded application frame queue.

D3D11 queues a GPU Wait on the decoded payload before SDL sampling, then signals
the next payload after SDL has submitted rendering/Present. Decode waits on that
reuse payload. Each slot has its own timeline, preventing unrelated slots from
signaling values out of order. D3D Flush submits the reuse signal without a CPU
GPU wait. No per-frame vkDeviceWaitIdle, queue drain or polling loop is added.
The pinned codec may itself wait when advancing its bounded Granite frame
contexts. Decoder destruction drains Vulkan; teardown also waits on the owned
D3D fence payloads before destroying imported resources.

## Stats and timing definitions

The overlay uses VIDEO_STATS counters and Moonlight ordering/terminology. Like
FFmpeg it displays the previous plus active roughly one-second windows. Host
latency uses RTP tenths of a millisecond; unknown samples are omitted. Network
drop percentage is missing network frame numbers / total expected frames.
Jitter/presentation percentage is stale presentation frames / decoded frames.
RTT reports `N ms (variance: N ms)`, or N/A when unavailable. Bitrate excludes
FEC overhead. Malformed frame rejection is logged separately, not fabricated as
network loss or jitter.

Measured stages:

- Network reassembly: decode-unit enqueue minus first packet receipt.
- Decoder queue wait: submit callback start minus enqueue.
- Parser/frame preparation: fragment assembly, validation, packet pushes, and
  output setup, excluding the decode API call.
- Average decoding time: the narrow decode API call only. GPU output measures
  CPU recording/submission, including any internal bounded-context wait. It is
  **not GPU execution duration** and must not be compared directly against an
  FFmpeg hardware completion duration. The overlay explicitly says this.
- CPU fallback decode includes GPU execution/readback in the synchronous API;
  the pin cannot separate those. The overlay explicitly reports this limitation.
- Frame queue delay: submission/output-ready publication to main-thread dequeue.
  On GPU output the frame is submitted, not necessarily GPU-complete; the
  remaining fence wait executes on the GPU during presentation.
- Rendering: upload (CPU fallback), draw/overlay submission, Present and reuse
  signal. It includes the CPU-observed monitor V-sync wait. It does not claim a
  GPU timestamp. Overlay stats formatting is excluded from this interval.

No true GPU decode duration or separate CPU readback duration is invented.
The old approximately 12.45 ms included decoder queueing/preparation; the new
narrow fallback interval excludes them. Compare pipeline FPS and measured stages,
not just the changed definition of the decode line.

## Qualification

GPU-free tests exercise queue bounds/stale drops/retained redraw, window merging,
host latency, percentages, queue/decode/render units and unmeasured values.
Runtime tests retain CPU range decode/rejection/recovery. SDL software tests
verify full/limited rendered endpoints and chromatic BT.709 values. GPU proof
repeats import/decode/fence/reuse and rendered endpoints with both ranges.
Unsupported Vulkan/interoperability is an explicit 77 skip; it is not GPU proof.
CI builds/packages x64 and native ARM64 and preserves runtime provenance and
baseline isolation checks. The experimental packages are for owner retesting.

On RTX 4070 Ti x64 and Surface Pro 11 ARM64, confirm `GPU presentation initialized`
in the log, inspect range/color, overlays, resizing/minimize/restore/reconnect,
and record incoming/decoded/rendered FPS, host latency min/max/average, network
and presentation drop percentages, RTT, decode timing mode, queue delay, render
time, bitrate, and all additional timing stages. At 2560x1440/120 on ARM64 compare
to ~93.3 FPS / ~12.45 ms old decode pipeline / ~1.24 ms render. Report actual
results; no sub-millisecond target or sustained 120 FPS qualification is asserted.
