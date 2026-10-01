# P0-R Vibepollo compatibility contract

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

1. **P0-R:** exact new codec pin and patches; both full-frame parsers; x64/ARM64
   dependency, parser, loader, qmake and PE/import CI; rerun both formats on RTX
   4070 Ti and Surface Pro 11 / Snapdragon X Plus / Adreno X1-85. Old GPU results
   at `f6fb84...` do not requalify `186f0393...`, including ARM64.
2. **P0.5:** use this Vibepollo-compatible build for SDL IYUV presentation;
   SDR color/range/chroma, resize/recreation/device loss, runtime deployment;
   validate 4:4:4 presentation separately after 4:2:0. Stay offline; keep HDR excluded.
3. **P1a:** explicit SDR 4:2:0 choice, complete runtime/presentation preflight,
   bitstream-ID check, reviewed SCM/RTSP/SDP negotiation, compatibility framing,
   safe standard-codec fallback/reconnect, H.264/HEVC/AV1 lifecycle/audio/input regressions.
4. **P1b:** live record framing, record-start and critical-count metadata,
   adaptive FEC and partial recovery. Bandwidth probing is a later usability gate.

Optional PyroWave CI stays separate from the baseline workflows and release
packaging. Artifacts include target runtime/install notices, parser/probe executables,
source/patch/import hashes, qmake evidence and roundtrip logs. Unavailable Vulkan
is recorded as exit 77; failures after device availability remain failures. Both
successful build evidence and outstanding hardware gates must be reported honestly.
