# common-c aeba988 compatibility review

PR #49 originally advanced only the gitlink. Both candidate architectures stopped
at the intentional qmake SHA guard before application compilation. That failure
was a request to review the integration, not evidence of a runtime codec failure.

## Reviewed upstream range

`f900dd4767759c7b9d0e93bcea666b55c69ea62f` →
`aeba988e0a302145eff2e70fa3e11cd98ec01f5d` contains three commits:

| Commit | Change | Effect on this integration |
| --- | --- | --- |
| `e2722dc` | Writes the periodic ping length as 32 bits before the 32-bit timestamp | Initializes all eight payload bytes; retained without modification |
| `5ec8397` | Frees replaced RTSP option fields and the incoming duplicate node | Retains correct option ownership during duplicate insertion |
| `aeba988` | Replaces token-based RTSP parsing with line parsing; case-insensitive header lookup/replacement, header whitespace handling, numeric validation, explicit serialization terminator | The transport header parser changes; the SDP body and explicit body length still reach the existing codec negotiation code |

Upstream modifies only `src/ControlStream.c`, `src/Rtsp.h` and
`src/RtspParser.c`. None is touched by the P1a patch. The base blobs for
`Limelight.h`, `RtspConnection.c`, `SdpGenerator.c` and `VideoDepacketizer.c`
are identical at both revisions; `PyroWave.h` remains an Asteria addition.
The regenerated full-index patch therefore retains every protocol hunk. Its
header records the new reviewed base; no artificial codec change is required.

## Preserved contract and semantic checks

- Client format `0x010000`, host SCM `0x00800000`, explicit SDR 8-bit 4:2:0.
- Exact `a=rtpmap:99 PYROWAVE/90000` plus exactly one matching
  `a=x-ss-pyrowave.bitstream:186f0393`; missing, duplicate, malformed or
  mismatched values still abort the attempt. RTSP header normalization does not
  normalize SDP attribute names, values or trailing whitespace.
- ANNOUNCE `bitStreamFormat=3`, with no PyroWave record feature/adaptive-FEC
  advertisement. Codec `186f0393`, API 0.6.0 and dependency hashes stay unchanged.
- Opaque picture validation still permits fully assembled AV1/PyroWave decode
  units. The frame assembler, fragment bounds, native Vulkan and legacy fallback
  implementations are unchanged.
- Standard selection still follows AV1, HEVC and H.264 branches. The integration
  does not intercept those masks or change HDR, video quality or pacing policy.
- Session header extraction still strips the timeout suffix. Mixed-case Transport
  header names now work with the existing server-port parser. ENet's existing
  incomplete final separator compatibility is retained upstream.

Both qmake and PowerShell now require the new exact SHA. Forward/reverse patch
checks and corresponding-source archive validation remain enabled. Historical
references to the v6.2.0 pin in `UPSTREAM_SYNC.md` remain historical.

This candidate starts from main including merged PR #50. PR #47 was open and
unmerged at inspection; none of its changes is needed. PRs #46 and #51 are outside
this change.

## Regression coverage and limits

`tests/pyrowave` adds three GPU-free executables using production common-c code:

- `pyrowave-rtsp-parser`: full DESCRIBE responses into the P1a SDP validator;
  header case/whitespace, LF/CRLF, numeric errors, standard codec bodies, strict
  PyroWave failures, serialization roundtrip, malformed cleanup, and allocation
  accounting for 1,000 duplicate replacements with owned/borrowed fields.
- `pyrowave-sdp-generator`: actual ANNOUNCE generation and parser roundtrip for
  PyroWave, AV1, HEVC and H.264, preserving SDR420 and feature exclusions.
- `pyrowave-rtsp-handshake`: actual OPTIONS/DESCRIBE/SETUP/ANNOUNCE/PLAY logic
  with an in-memory TCP transport, including byte-sized response fragments,
  codec choice, Session/Transport handling, explicit standard choice on a PyroWave
  host, rejection and standard-codec reconnect after a failed PyroWave attempt.
  Encryption, real sockets, ENet transport and pairing are not exercised here.

`scripts/test-common-c-p1a.ps1` uses a disposable local clone to reject an
unexpected SHA even with identical source, verify idempotent application, and
reject modified patched source/archive content. It requires no network or Qt.
The PyroWave workflow runs it before the CMake suites. Common-c gitlink/guard/patch
changes now trigger the existing Stage 2–5 and offline workflows on both targets.

Local validation uses Cygwin GCC 14 for the portable suites and PowerShell for
dependency/package guards. Full Windows application builds, native resource mocks,
packaging and provenance validation use the existing x64/native ARM64 CI jobs.
CI results must be associated with the candidate SHA; old PR #49 failures and
earlier PR #50 results do not qualify this candidate.

GPU-dependent probes that report device unavailability are skips. Compilation,
mock handshakes and policy tests do not qualify a real host, driver, native Vulkan
path or performance. Pairing has no standalone automated suite in this repository.
If an owner smoke is needed: connect to Vibepollo, stream AV1 briefly, check a
PyroWave stream, disconnect and reconnect normally. No benchmark is required.
