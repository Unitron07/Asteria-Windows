# PyroWave P1a GPU presentation and stats

## Current live ARM64 owner result

Owner-reported test: Surface Pro 11, Snapdragon X Plus, Qualcomm Adreno X1-85;
Vibepollo host, 2560x1440, up to 120 Hz target, PyroWave SDR 8-bit 4:2:0.
Negotiation, decoding, rendering, audio and input worked; the owner reported good
visual quality and responsiveness. Workload FPS varied and was not always 120 FPS.

Latest tested compute/CPU-I420 fallback timings were approximately:

| Stage | Owner-observed time |
| --- | --- |
| Decode/readback | 3.20 ms |
| Frame queue | 0.26 ms |
| Render | 1.78 ms |
| Frame assembly | 0.06 ms |
| Parser/packet preparation | 1.29 ms |

These are observations from this named workload/device, not universal benchmarks,
an end-to-end latency measurement or a production-stability claim. The driver
rejects shared-fence import with `PYROWAVE_ERROR_UNSUPPORTED_EXTERNAL_HANDLE`;
compute decode plus CPU I420 presentation works. Broader x64/hardware and shared
GPU-path qualification remain separate. Earlier failures/timings below remain
historical, superseded for the current named ARM64 fallback by this owner result.

## Historical bring-up measurements

The owner reports the first successful live P1a video on Surface Pro 11,
Snapdragon X Plus / Adreno X1-85: 2560x1440, target 120 FPS / 120 Hz. Video
rendered, audio/input worked, and visual quality and responsiveness felt good.
The earlier compute CPU fallback baseline was ~109.7 incoming/decoded/rendered FPS, ~501.6 Mbps,
~3.47 ms synchronous decode/readback, ~2.64 ms combined parser/frame preparation,
~8.72 ms reassembly, ~0.55 ms decoder queue wait, ~0.08 ms frame queue delay and
~1.20 ms render. The game did not produce a full 120 FPS. These are owner
observations before the preparation cleanup. A later fragment CPU fallback test
on the same system at 2560x1440/120 Hz reported ~101.7 FPS, ~4.31 ms decode/readback,
~2.19 ms decoder queue wait, ~0.97 ms assembly, ~1.76 ms parser/packet preparation
and ~1.50 ms render. Native stages were ~1.98 ms/frame `iDWT fragment` and
~0.83 ms/frame `Dequant`. Conditions were not identical: fragment CPU output did
not improve measured performance and appears worse on this tested device, not
universally. This correction restores compute CPU fallback; its performance
qualification was pending at that stage; the latest named ARM64 fallback result is recorded above.

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
ownership of the NT handle to PyroWave. SDL3 in the pinned v19 dependency wraps
separate D3D11 Y/U/V texture pointers, with the same explicit BT.709 full/limited
colorspace values as the CPU path. No new shader, swapchain, Vulkan loader import,
or codec runtime import is required. Existing aspect fitting, linear scaling,
overlays, resize behavior and V-sync presentation remain SDL-owned.

GPU presentation preflight follows `pyrowave_decoder_device_prefers_fragment_path`.
The tested Qualcomm Adreno X1-85 still selects fragment for GPU output. Successful
presentation keeps that decoder. The tested Qualcomm driver rejects D3D11/D3D12
timeline-fence import with `-7` (`PYROWAVE_ERROR_UNSUPPORTED_EXTERNAL_HANDLE`).
After any failed presentation probe, the decoder is recreated for synchronous
CPU I420 output using the default compute path for v0.2.0. Offline CPU validation
also defaults to compute. Preferred, GPU and fallback modes remain logged; this
output-path policy has no vendor table. Native timing and preparation diagnostics
are retained. Future native Vulkan presentation may produce a different result.

Cross-API output is enabled only after adapter LUID equality, ID3D11Device5 /
Context4 availability, every real R8 output-image import, every timeline fence
import, and both range-specific SDL wrappers succeed. This is a capability gate
on both x64 and ARM64, not a claim that all NVIDIA or Qualcomm drivers support it.
If any initialization step fails, the reason is logged and the already validated
CPU I420 renderer remains usable. Partial resources are destroyed. A live device
loss/submission failure ends the stream through normal Session cleanup; it does
not replay host launch or silently continue with suspect GPU resources.

A dedicated native Vulkan PyroWave presenter (GPU Y/U/V images, conversion shader
and Vulkan swapchain) is **deferred until after v0.2.0**. It is not implemented.
Target architecture: `PyroWave Vulkan decode -> GPU-resident Y/U/V -> Vulkan presentation shader -> Vulkan swapchain`. This avoids Vulkan -> CPU -> D3D11 round-trips and dependence on unsupported external-fence sharing. Unsupported interop currently uses synchronized CPU I420 output. Other Moonlight forks have explored GPU-resident PyroWave interop, but they are not dependencies; Asteria is independently maintained with Vibepollo as its primary protocol target.

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
- Frame assembly: common-c transport fragments into a contiguous compatibility
  envelope. Per-decoder byte capacity is retained across frames.
- Parser/packet preparation: compatibility/record validation, packet pushes and
  output setup, excluding assembly and the decode API. Runtime packet, record
  and duplicate-index storage is reused. The complete first validation pass is
  retained; the metadata pass avoids scanning coefficient payloads again.
  Bounds, duplicate checks, fragment semantics and range restrictions are unchanged.
- Average decoding time: the narrow decode API call only. GPU output measures
  CPU recording/submission, including any internal bounded-context wait. It is
  **not GPU execution duration** and must not be compared directly against an
  FFmpeg hardware completion duration. The overlay explicitly says this.
- CPU fallback decode includes GPU execution/readback in the synchronous API;
  this API interval cannot separate those. The overlay reports this limitation.
- Native GPU diagnostics: `pyrowave_device_report_performance_stats` forwards the
  pin's callback text, including `Dequant`, `iDWT` / `iDWT fragment` and its
  **ms per frame** terminology, to `PyroWave GPU timing` log lines. Memory-budget
  messages retain their own terminology under `PyroWave device performance`.
  Reports run once after ten seconds of live frames on the decoder thread and
  once after decoder teardown drains work, before device destruction. Neither
  report resets the cumulative counters or adds a per-frame wait. Missing
  reporting exports or no resolved timestamps are nonfatal diagnostics.
  Both CPU/GPU output use the same device reporting. Native stages aid comparison
  with CPU API time; their difference is not an isolated readback measurement.
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

On RTX 4070 Ti x64 and Surface Pro 11 ARM64, inspect range/color, overlays,
resize/minimize/restore/reconnect and audio/input. On ARM64 confirm adapter
Adreno X1-85, preferred fragment mode, the unsupported external-handle fallback
reason, `PyroWave GPU decoder path: fragment`, `PyroWave CPU fallback decoder path: compute`, and first I420 decode
success. Capture native timing logs after at least ten seconds and shutdown.
Record all standard stats plus assembly and parser/preparation separately.
Compare assembly + parser/preparation to the previous combined ~2.64 ms; compare
synchronous decode/readback to the latest fragment ~4.31 ms and earlier compute
~3.47 ms at 2560x1440/120 Hz. Record the other stages above without requiring an
exact timing or FPS target: gameplay and network conditions vary. No CI
performance improvement or final hardware qualification is claimed.
