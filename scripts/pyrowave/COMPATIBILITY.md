# Observed native MSVC ARM64 blockers

Unpatched pinned Granite was actually compiled on GitHub's native
`windows-11-arm` runner with MSVC 19.51.36257.0 / SDK 10.0.26100.0.
[Run 36513013000](https://github.com/Unitron07/Asteria-Windows/actions/runs/36513013000)
records `math/simd.hpp` C1189 (`Implement me`) and
`math/muglm/muglm.cpp` C2665/C2440 in the scalar `transpose_from_affine` fallback.
The first patched build exposed C4717: the scalar `transpose(mat4&,const mat4&)`
recurses through its one-argument overload. These are compile/runtime math
fallback defects; an ARM64 link blocker was not observed after fixing them.

`granite-msvc-arm64-portable-math.patch` changes only three scalar fallback bodies:

- Implement the six-plane AABB frustum test without SIMD assumptions.
- Transpose a mat4 explicitly, using a temporary so input/output may alias.
- Reconstruct all four affine columns with the homogeneous row before transposing.

The dependency build applies this patch only for the ARM64 target. It preserves
the exact original source commits and records separate patch/file hashes and
the actual diff in evidence. It does not redefine `_MSC_VER`, `_WIN64`,
`__ARM_NEON`, or `__aarch64__`, and does not enable x86 intrinsics on ARM64.
Existing SSE/NEON branches are unchanged. `-UnpatchedArm64` reproduces the raw
source build; use a fresh output root when changing patch mode.

At this tested MSVC version, `util/bitops.hpp` and its generic `_MSC_VER/_WIN64`
`__popcnt/__popcnt64` and bit-scan branches compile on native ARM64. The offline
compatibility tests execute them with zero/all-bit/high-bit values. No speculative
bitops patch is applied. Older MSVC versions are not qualified by this result.
MSVC ARM64 does not select Granite's GCC-style NEON branch at this pin; the
experiment uses the repaired portable math path instead of introducing a new
NEON implementation.

`granite-compatibility-tests` exercises affine conversion, alias-safe mat4
transpose, inside/outside frustum results and the actual target's bitops. These
tests link only Granite's static math library and need no Vulkan/GPU. They must
pass on native ARM64 before considering the patched build validated.
