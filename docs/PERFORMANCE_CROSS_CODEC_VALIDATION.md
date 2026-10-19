# Cross-codec audit validation

Audit base: `4944dd5364a8285a03d0da24cf44cf88bb3ac971` (merged PR #50).
Scope: documentation only. Runtime, tests, workflow definitions and dependency
pins are unchanged. Phase 1B remains paused; draft PR #46 is untouched.

## Local verification

On 2026-10-19, compiled the existing GPU-free suites with Cygwin GCC 14.4.0,
C++17 / `-O2` (C99 for negotiation), on the Windows host. Applied the repository's
existing P1a patch to the exact common-c gitlink for negotiation/live-frame tests.
This is the normal build-time source patch, not a new dependency revision.

Results:

- Five Vulkan policy suites: Stage 5, probe policy, shared-device policy,
  shared diagnostics and Stage 4: PASS.
- PyroWave compatibility/records parser, live-frame/transport, legacy offline
  parser, presentation patterns, queue/statistics and negotiation: PASS.
- Fixed-storage performance core: PASS.
- Python performance analysis regression suite (11 tests): PASS.
- Synthetic capture validation and `git diff --check`: PASS.

Reproduction, from the repository root with GCC on PATH:

```text
g++ -std=c++17 -O2 -Wall -Wextra -pthread -Iapp/streaming/video tests/pyrowave/vulkan/stage5_policy_tests.cpp -o stage5-policy.exe
./stage5-policy.exe
```

Repeat for `policy_tests.cpp`, `shared_policy_tests.cpp`,
`shared_diagnostic_tests.cpp` and `stage4_policy_tests.cpp` in that directory.
The five policy targets are also defined in `tests/pyrowave/vulkan/CMakeLists.txt`.
For the parser/live/negotiation/presentation/queue suite, use the existing
`tests/pyrowave/CMakeLists.txt`; for performance core, `tests/performance`.

```text
python -m unittest discover -s tests/performance -p "test_*.py" -v
python scripts/performance.py validate tests/performance/synthetic.json
git diff --check
```

These runs use no Vulkan loader, hardware decode or real swapchain. They do not
prove Surface performance, D3D11 bind-mode compatibility or codec HDR output.
The local machine has no configured MSVC/Qt application toolchain; complete
x64/ARM64 application compilation is delegated to repository CI.

## CI and portable build

Pending creation of the draft PR. Completed run links, results and exact tested
head will be recorded in the PR description so updating this record does not
invalidate the tested commit. No pending, skipped or unavailable job is a pass.

Required hardware distinctions remain explicit: real AV1 Main8/Main10,
HEVC Main/Main10, H.264 and native/legacy PyroWave playback are untested here.
Use the short owner procedure in the architecture audit after the ordinary
candidate ARM64 artifact is available. This is not a release or a request for
Phase 1B benchmarking.
