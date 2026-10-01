# Exact active source and patch provenance

P0-R targets [Themaister/pyrowave](https://github.com/Themaister/pyrowave) commit
`186f0393b77f7755953b5ecde994bb1cec2e4155`, bitstream ID `186f0393`, C API 0.6.0.
[dependencies.json](dependencies.json) is the active build lock. Granite, volk
and Vulkan-Headers retain the already validated revisions. Every exact fetch,
HEAD check and clean-checkout check must pass; there is no HEAD substitution or
historical-bundle fallback. Builds own separate x64/ARM64 sources and outputs.

Verbatim Vibepollo patches are retained under `patches/` with immutable origin,
SHA-256 and application decisions in the lock. Every patch is hash-checked and
checked for applicability on both targets. Only `0003-decoder-reject-short-block`
is applied: it prevents malformed duplicate blocks from leaving the parse cursor
unchanged. `0001` is encoder buffer-pool performance work. `0002` is encoder-only
4:4:4 allocation safety and must be applied before future 4:4:4 fixtures. Neither
is needed for the single-image SDR 4:2:0 proof. None changes the bitstream.
See the [current contract](../../docs/PYROWAVE_VIBEPOLLO.md).

Evidence records unmodified commit/tree/source-archive hashes, codec patch hashes,
applied diffs, patched-file hashes, compiler/CMake/options and installed inventory.
The architecture-specific Granite portable math patch remains separate. API checks
exist in the dependency helper, compile-time runtime wrapper and restricted DLL load.
Normal release packages never acquire the experimental runtime.

## Historical recovery bundle (inactive)

`pyrowave-pinned.bundle` preserves the audited P0/PR #14 source history at
`f6fb84eb0d8538f43f6f54e58d2040d101c8676c`, from joemossjr16/pyrowave, derived
from Themaister/pyrowave. The original fork returned 404 on 2026-09-28 and upstream
could not serve that object. This historical clean-source snapshot is retained
with its MIT license, not as compiled binaries or an active compatibility target.

Historical bundle SHA-256:
`e4387ce6b691724aa342d6df7677f51efffe30e98e3949415718e7b2b955c56e`.
Verify manually with `git bundle verify scripts/pyrowave/pyrowave-pinned.bundle`.
The active helper never reads it. The old RTX 4070 Ti/Adreno results using this
revision remain historical and do not qualify the new codec/bitstream.
