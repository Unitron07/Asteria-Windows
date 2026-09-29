# Pinned source provenance and recovery

`pyrowave-pinned.bundle` is an unmodified Git bundle of the codec repository
cached during Asteria's PR #14 source audit. It contains commit
`f6fb84eb0d8538f43f6f54e58d2040d101c8676c` and its available history, not built binaries.
The original source is [joemossjr16/pyrowave](https://github.com/joemossjr16/pyrowave)
(`https://github.com/joemossjr16/pyrowave.git`), derived from
[Themaister/pyrowave](https://github.com/Themaister/pyrowave). The fork returned
GitHub 404 on 2026-09-28 and upstream could not serve this exact object.
The snapshot was taken from the clean audited checkout at the pinned commit.
It preserves source content and available Git history for reproducibility and
provenance; it is not a compiled runtime or an arbitrary binary dependency.

Bundle SHA-256:
`e4387ce6b691724aa342d6df7677f51efffe30e98e3949415718e7b2b955c56e`.

The build helper, also used by optional CI, first attempts the original fork.
Prefer that source again if it becomes available. Only when that fetch fails
does the helper verify the bundle checksum and Git bundle, fetch its history,
check out the exact commit above, and verify HEAD and clean tracked content.
A missing/mismatched bundle or revision fails the build. Never silently
substitute moving HEAD, another PyroWave revision, or unrelated binaries.

Verify with `git bundle verify scripts/pyrowave/pyrowave-pinned.bundle` inside a
Git repository. `dependencies.json` records the bundle hash and all source pins.
The source's MIT `LICENSE` is preserved in the bundle and installed evidence.
Vulkan-Headers and volk are still fetched at Granite's original gitlinks.
