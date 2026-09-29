# Pinned source recovery

`pyrowave-pinned.bundle` is an unmodified Git bundle of the codec repository
cached during Asteria's PR #14 source audit. It contains commit
`f6fb84eb0d8538f43f6f54e58d2040d101c8676c` and its available history, not built binaries.
The original fork returned GitHub 404 on 2026-09-28 and upstream could not serve
this object. The build helper first attempts that fork, then fetches the same
object from this SHA-256-verified bundle. It never substitutes upstream HEAD.

Verify with `git bundle verify scripts/pyrowave/pyrowave-pinned.bundle` inside a
Git repository. `dependencies.json` records the bundle hash and all source pins.
The source's MIT `LICENSE` is preserved in the bundle and installed evidence.
Vulkan-Headers and volk are still fetched at Granite's original gitlinks.
