# P0-R Vibepollo compatibility contract

## M1B P1a implementation / live qualification pending

The explicit **PyroWave (Experimental)** live path is implemented behind
`CONFIG+=pyrowave_experimental`. Live owner qualification remains **PENDING**;
build/test evidence is recorded in [VALIDATION.md](VALIDATION.md). The implementation
is ready for hardware interoperability testing only after both optional client
builds and baseline CI pass. P0-R and P0.5 qualification history is unchanged.

The contract is Vibepollo at `8a8c4b03a280ab9f567beb380110abb80f5220b8`, codec
`186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, C API 0.6.0.
Only SDR 8-bit 4:2:0 is advertised: `SCM_PYROWAVE=0x00800000`, client format
`0x010000`, exact DESCRIBE `a=rtpmap:99 PYROWAVE/90000` and exactly one valid,
matching `a=x-ss-pyrowave.bitstream:186f0393`; ANNOUNCE uses `bitStreamFormat=3`.
No record-feature or adaptive-FEC attribute is sent, preserving compatibility framing.

Session tests restricted runtime loading, required exports/API, packaged build
metadata/DLL SHA-256, Vulkan device/decoder creation and a real hidden SDL IYUV
upload/presentation before launch. Host capability is rechecked using paired,
certificate-pinned HTTPS without HTTP fallback before sending launch/resume.
PyroWave has its own `IVideoDecoder`, never FFmpeg: complete common-c decode unit
→ LE compatibility envelope/codec-record validation → individual codec packets
→ full readiness → CPU I420 → main-thread SDL presentation. Audio/input and
ordinary Session cleanup remain on existing Moonlight paths.

Auto and standard codec candidate ordering are unchanged. HDR, 4:4:4 and forced
software decode reject PyroWave attempts. Missing/mismatched IDs abort DESCRIBE.
Malformed independent frames clear decoder state and can recover on the next frame;
runtime/presentation failure ends the attempt. Retry is manual: choose a standard
codec and reconnect. No hot switch or automatic host launch/resume replay occurs.

The common-c gitlink remains `f900dd4767759c7b9d0e93bcea666b55c69ea62f`.
`scripts/pyrowave/common-c-p1a.patch` is a maintained, separately reviewable delta
to Limelight constants, strict SDP validation, ANNOUNCE and opaque-picture
validation. qmake verifies the pin and applies it idempotently; the explicit
PowerShell helper does the same for CMake tests. The submodule is not flattened.
Reserved host HDR/444 bits are consumed by Session's SCM mapping with zero client
formats; standard HDR/444 masks are unchanged. SCM and VIDEO_FORMAT namespaces
are never interchanged.

Live bounds: even 128..4096 dimensions, at most 3840×2160 pixels; 8 MiB envelopes,
65,536 codec packets, at most 4,000 transport fragments and 1024..2048-byte transport
packet sizes. The byte bound covers Vibepollo's 4,000-packet complete-frame budget
at the largest permitted MTU, including compatibility overhead. Codec packet
count is independent of RTP count. Output is at most 12,441,600 CPU bytes/frame;
one replaceable pending image bounds the render queue. Offline limits remain
850,000 bytes / 1,024 packets. Only 1080p has prior offline hardware qualification.

Presentation stays BT.709 limited, aspect fit and linear scaling. Source chroma
is CENTER; SDL2-compat metadata uses LEFT. P0.5 found no visible issue on the
named targets, but exact phase remains formally unqualified. Session V-sync is
retained; advanced pacing/latency work is deferred. The existing statistics
overlay receives VIDEO_STATS-derived rates, bytes, drops, timing and RTT.

Optional CI builds separate x64 and native ARM64 experimental portable packages,
with runtime provenance, matching PE types, import/CRT closure, source notices,
an owner guide and log collection. Ordinary packages contain no PyroWave DLL,
no startup Vulkan/codec imports and no PyroWave UI option. See
[the live owner guide](../tests/pyrowave/LIVE-OWNER-TEST.md) for launch/test steps.

Deferred P1b/later: live records, record-start/lost-buffer metadata, critical
packet handling, adaptive FEC, partial recovery, sideband readiness, bandwidth
probing, bitrate usability tuning, 4:4:4, HDR and advanced pacing/latency work.

The earlier milestone descriptions below retain the P0/P0-R/P0.5 history;
future P1a statements there are superseded by the implementation above.


Nonary/Vibepollo is Asteria's primary PyroWave host target. The authoritative
[host protocol](https://github.com/Nonary/Vibepollo/blob/8a8c4b03a280ab9f567beb380110abb80f5220b8/docs/pyrowave-protocol.md)
and [vendored codec/patches](https://github.com/Nonary/Vibepollo/tree/8a8c4b03a280ab9f567beb380110abb80f5220b8/third-party/pyrowave)
were audited at `8a8c4b03a280ab9f567beb380110abb80f5220b8`.
Nonary/moonlight-qt's PyroWave branch was inspected at
`5f9ce4a46d2b8fd2191f47cef043bc43f3d772d0`, including its framing/parser tests,
vendor lock and decoder. The host protocol and pinned upstream codec remain
the primary authority; live recovery behavior is reserved for P1b.
The joemossjr16/pyrollo comparisons and old `PYRW` proof remain historical evidence.

## Independent compatibility axes

- Active upstream codec: Themaister/pyrowave
  `186f0393b77f7755953b5ecde994bb1cec2e4155`.
- Experimental built bitstream ID: `186f0393` (`PyroWave::BitstreamId`).
- C API: **0.6.0**, verified in pinned headers, at compile time, and on DLL load.
- Granite, volk and Vulkan-Headers retain their already tested revisions in
  [dependencies.json](../scripts/pyrowave/dependencies.json).

The bitstream has no version field. API 0.6.0 alone does not establish codec
compatibility. Future P1a must read `a=x-ss-pyrowave.bitstream:...`, compare it
with the local build ID, and refuse/warn on a mismatch. Runtime load checks the
API and exports; it cannot discover a DLL's source commit. Deployments must bind
the DLL to the build's source/patch/inventory hashes.

Current scope remains **1920x1080, 8-bit SDR 4:2:0**, offline only. No Session
hooks, host advertisement, RTSP negotiation, UI choice, HDR path, bandwidth probe,
release packaging or frame-pacing change is included. H.264/HEVC/AV1 retain their
existing behavior. Complete local roundtrip does not prove network interoperability.

## Patch decisions

The lock file records immutable Vibepollo provenance and SHA-256 for all three
verbatim patches (LF endings). The dependency helper checks every hash and
`git apply --check` against the exact upstream revision on both architectures.
It archives unmodified source hashes, applied diffs, modified-file hashes and
patch inventory; retained patches ship in experimental source notices.

| Patch | Build decision | Effect |
| --- | --- | --- |
| 0001 encoder buffer pool | Retained, not applied | Encoder allocation/performance only; unnecessary for a single-frame fixture |
| 0002 4:4:4 payload sizing | Retained, not applied | Encoder buffer safety for 4:4:4; apply before future 4:4:4 fixture work |
| 0003 decoder short-block rejection | Applied x64 + ARM64 | Decoder safety: prevents non-advancing parse loops on malformed duplicate blocks |

These local patches preserve the bitstream. Encoder-only fixes do not qualify
4:4:4 client decode or presentation. The separate Granite MSVC ARM64 portable
math patch remains architecture-scoped; see [COMPATIBILITY.md](../scripts/pyrowave/COMPATIBILITY.md).
The old `f6fb84...` source bundle is retained for history and never used by the
active build helper. Failed exact fetch or revision mismatch fails the build.

## Complete-frame parsing

`parseFrame` selects framing from bit 31 of the first LE word: set means records;
clear means compatibility packet count. Invalid input is rejected deterministically;
it never retries the legacy wrapper. `parseLegacyOfflineFrame` is only an explicitly
called historical fixture helper.

Compatibility format:

```text
[u32 LE packet_count] { [u32 LE size] [size bytes] } * packet_count
```

`parseCompatibilityFrame` validates the envelope/count/lengths and returns packet
offsets into immutable caller-owned bytes, without copying payload. Supplying a
`StreamContext` to `parseFrame` additionally validates the codec records in those
packets before any decoder call. Codec packet boundaries differ from RTP boundaries:
the pinned packetizer keeps each record intact, using a 1024-byte packing target.

Record framing uses the upstream 8-byte sequence header first (`extended=1`,
`code=0`), followed by block records in any block-index order and optional padding:
`0xFFFFFFFF`, LE word count, then that many zero words. The parser walks the full
reassembled byte stream. Records may straddle any transport payload boundary.
It does not depend on critical-prefix ordering, first-fit packing or record-start flags.

Validation rejects truncated records, short/header-only blocks, nonzero/runoff
padding, another sequence header, sequence mismatch, impossible/duplicate block
indices, inconsistent total block count, malformed ballot/coefficient lengths,
and trailing bytes. The block-index capacity and partial-edge mapping mirror the
pinned codec's five wavelet levels and minimum 128-pixel aligned image extent.
Negotiated width/height/chroma are checked when supplied. The P0 runtime also
requires SDR BT.709 metadata. Padding is represented in `Frame.records` but excluded
from `Frame.packets` submitted to the C API. Input byte loads do not require alignment.

The existing conservative P0 cap remains **850,000 total bytes** and **1,024
compatibility packets**; record metadata is capped by input bytes. These are offline
policy choices, not claims about Vibepollo's full live budget (up to 3000/4000 transport
packets). Live limits must be reviewed against overhead, negotiated MTU, FEC and memory
before P1. Record lengths/counts are validated before allocating record metadata.
Every rejection clears the output model. A new independent frame can then be decoded.

## Future transport interface (P1b only)

`PayloadMetadata`/`RecoveryMetadata` reserve caller-owned frame offsets, payload
lengths, lost ranges, record-start flags and critical packet counts. They are not
accepted as full-frame input or connected to moonlight-common-c in this milestone.
Future transport must distinguish `BUFFER_TYPE_LOST` after parity recovery from
`BUFFER_TYPE_RECORD_START`, preserve the announced critical count and expose
whether critical data was recovered. Never treat zero-filled loss as valid records.

A separate recovery parser must skip records touched by loss, resume from known
record boundaries, verify coarsest-band integrity and integrate sideband readiness
(`pyrowave_decoder_decode_is_ready_with_sideband`, more than 90% records).
Adaptive FEC, alignment heuristics, record-start delivery and partial decode belong
to P1b. Complete frames now clear the decoder per frame, push all valid records,
require full readiness and decode to fixed I420 CPU planes.

## Future negotiation audit (no active constants in common-c)

| Profile | Server SCM bit | Client-local format bit |
| --- | --- | --- |
| SDR 4:2:0 | `0x00800000` | `0x010000` |
| SDR 4:4:4 | `0x01000000` | `0x020000` |
| HDR10 4:2:0 | `0x02000000` | `0x040000` |
| HDR10 4:4:4 | `0x04000000` | `0x080000` |

Client-local `VIDEO_FORMAT_MASK_PYROWAVE=0x0F0000`. Future SDP DESCRIBE recognizes
`a=rtpmap:99 PYROWAVE/90000` and `a=x-ss-pyrowave.bitstream:186f0393`.
The initial live target is SDR 4:2:0 compatibility framing. HDR bits are documented
for collision/negotiation review only; no 10-bit support is claimed or enabled.
Auto stays standard-codec selection. PyroWave should be explicit/experimental;
no user-facing setting is added here.

Later usability work may read `PyroWaveHostLinkMbps` and
`PyroWaveBandwidthProbeBytes=33554432`, then time the paired HTTPS
`/pyrowave-bandwidth-probe`: discard warmup, use slowest of three, reserve 20%.
No probe is implemented here, and bulk throughput would not prove UDP loss tolerance.

## Qualification order

1. **P0-R COMPLETE:** exact new codec pin and patches; both full-frame parsers;
   x64/ARM64 dependency, parser, loader, qmake and PE/import CI; both formats
   hardware-qualified on RTX 4070 Ti x64 and native Surface Pro 11 / Snapdragon
   X Plus / Adreno X1-85 ARM64, at `186f0393...`, bitstream ID `186f0393`, API 0.6.0,
   with three decoder lifetimes, expected I420 output and malformed recovery.
   See [current hardware evidence](VALIDATION.md#m1b-p0-r-vibepollo-validation).
   Old `f6fb84...` GPU results remain historical.
2. **P0.5 COMPLETE:** owner visual qualification passed on RTX 4070 Ti x64 and
   native Surface Pro 11 / Snapdragon X Plus / Adreno X1-85 ARM64. Raw I420 and
   compatibility/record SDL IYUV passed all five SDR 8-bit 4:2:0 BT.709 limited
   patterns, fit/resize, nearest/linear, fullscreen/maximize/restore and recreation.
   Ten lifecycles and synthetic reset recovery passed; 60 FPS loops passed pacing
   sanity checks. See [hardware evidence](VALIDATION.md#m1b-p05-offline-sdl-qualification)
   and [commands and limits](../tests/pyrowave/README.md#p05-offline-sdl-presentation).
   No visible chroma anomaly was observed; exact authored CENTER versus SDL3 LEFT
   sampling is formally unqualified. True physical GPU loss remains manual/unproven;
   full pacing, production latency, 4:4:4, HDR and live interoperability are unqualified.
3. **P1a NEXT / FUTURE (not implemented):** explicit SDR 4:2:0 choice, complete runtime/presentation preflight,
   bitstream-ID check, reviewed SCM/RTSP/SDP negotiation, compatibility framing,
   safe standard-codec fallback/reconnect, H.264/HEVC/AV1 lifecycle/audio/input regressions.
4. **P1b LATER:** live record framing, record-start and critical-count metadata,
   adaptive FEC and partial recovery. Bandwidth probing is a later usability gate.

Optional PyroWave CI stays separate from the baseline workflows and release
packaging. Artifacts include target runtime/install notices, parser/probe executables,
source/patch/import hashes, qmake evidence and roundtrip logs. Unavailable Vulkan
is recorded as exit 77; failures after device availability remain failures. Both
successful build evidence and outstanding hardware gates must be reported honestly.
