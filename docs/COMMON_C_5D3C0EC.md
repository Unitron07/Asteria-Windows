# common-c 5d3c0ec compatibility review

PR #56 advanced only the gitlink. Both PyroWave offline jobs failed in the common-c
revision and patch guards, and both candidate builds failed in the build step, which
matches the qmake SHA guard stop recorded for [aeba988](COMMON_C_AEBA988.md). The
job logs were not read, so treat that as the likely cause rather than a confirmed
one. Neither failure is evidence of a runtime fault.

## Reviewed upstream range

`aeba988e0a302145eff2e70fa3e11cd98ec01f5d` →
`5d3c0ecca00c1bb3f587acb8060476edcf8e24aa` contains ten commits:

| Commit | Change | Effect on this integration |
| --- | --- | --- |
| `342b106` | Encrypts ENet control packets in place, removing the 256-byte temporary buffer | No wire format change intended; retained without modification |
| `4a95eb3` | Sends UTF-8 text on the keyboard channel and removes `CTRL_CHANNEL_UTF8` | Text can no longer be reordered against keyboard events |
| `0405384` | Packs several code points into each `NV_UNICODE_PACKET` for Sunshine hosts and skips the GFE flush and 50 ms delay | Changes what hosts receive from `app/streaming/input/keyboard.cpp`; see limits below |
| `448b2c6` | Latches mouse deltas in the packet holder before sending | Prevents motion from starving the input thread |
| `970b072` | Clears the queued controller pointer when an enqueue fails | Fixes a use after free |
| `f1cca2d` | Pen batching honours tool type and keeps the reliable flag across a batch | Applies to existing pen input |
| `421bd74` | Ends mouse motion batches on button events | Keeps motion and clicks in order |
| `6d0f73e` | Advances nanors `b1e3c22` → `a9b1636` | Forward error correction; see below |
| `41cedb8` | Acts on ENet disconnects only when they match our peer, session and address | Unrelated packets can no longer trigger the disconnect intercept |
| `5d3c0ec` | Adds the adaptive trigger sentinel to the Gen 3–7 packet type tables | Keeps those tables aligned with the index enum |

Upstream modifies only `nanors`, `src/ControlStream.c`, `src/InputStream.c` and
`src/Limelight-internal.h`. None is touched by the P1a patch, so its hunks and
index lines are unchanged and only its header records the new reviewed base.
Nothing in this repository references `CTRL_CHANNEL_UTF8`.

## nanors

The range adds integer overflow checks and tighter `reed_solomon_new` argument
validation. Stricter AVX-512 detection also requires BW, DQ and VL, so a CPU with
only AVX-512F now takes a lower dispatch path. `RtpAudioQueue.c` and
`RtpVideoQueue.c` call this code. The sources named in `moonlight-common-c.pro`
all still exist. The changes under `deps/obl` were only partly reviewed.

## Preserved contract

The PyroWave format, SCM bit, SDP identity, ANNOUNCE and decode-unit behaviour
recorded in the aeba988 review are unchanged, because none of those files moved.
Both qmake and PowerShell now require the new exact SHA. Forward/reverse patch
checks and corresponding-source archive validation remain enabled.

## Regression coverage and limits

`pyrowave-input-text` compiles production `InputStream.c` and
`LinkedBlockingQueue.c` with a synchronous fake send thread. It covers keyboard
channel use, reliability, packing at the 32-byte limit, code point boundaries,
truncated and invalid text, and the retained per-code-point GFE path with its
flush and 50 ms delay.

It uses a fake control stream, so real sockets, encryption, ENet and every host's
text handling are not exercised. Vibepollo acceptance of multi-code-point packets
and reordering-free typing have not been qualified on a real host. Mouse, pen,
controller, ENet disconnect and nanors changes have no new tests here; they rely
on upstream review and on the existing CI build jobs.

Local validation used MinGW GCC 7.4 for the input text, RTSP parser, SDP generator
and handshake suites, which all pass against the patched checkout; the input text
suite fails against the aeba988 input code. `test-common-c-p1a.ps1` passes on Windows
PowerShell 5.1 only with its error preference relaxed, because this machine has no
PowerShell 7. MSVC, ARM64, Qt, `nanors/deps/obl/oblas_lite.c` and packaging were not
built locally and rely on the existing CI jobs.

CI results must be associated with this candidate SHA; the PR #56 failures do not
qualify it. Owner smoke: connect to Vibepollo, stream AV1 briefly, check a PyroWave
stream, paste text that mixes ASCII and non-ASCII characters, use a controller and
mouse, then disconnect and reconnect normally. No benchmark is required.
