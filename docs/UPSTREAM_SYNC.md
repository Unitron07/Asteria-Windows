# Moonlight upstream maintenance

Asteria preserves Moonlight history. This sync merges all 17 commits after the
original import `e3fd29e4d7dc5723d8d0da7d19e2698daec74456` through **v6.2.0**
(`de2467e433821664cdd2224aad8c89a625be1ad9`). Git confirmed that import was the
merge base. The merge was clean; Asteria's intentional identity, PyroWave,
codec-aware bitrate, ARM64 and portable-package changes remain in place.
New-install resolution defaults are 1920x1080; persisted dimensions still win.

The full v6.2.0 baseline includes the corresponding Moonlight security fixes
listed for CVE-2026-33546, CVE-2026-33547 and CVE-2026-41210 in the
[upstream release notes](https://github.com/moonlight-stream/moonlight-qt/releases/tag/v6.2.0).
Those notes describe malicious-host connection crashes; this is not a broader
security assurance. Some fixes were already inherited before the 17-commit delta.
The common-c gitlink already matched v6.2.0 (`f900dd476...`); its reviewed
PyroWave patch still applies. Windows dependencies now use verified v19 archives
for both architectures, including upstream FFmpeg/SDL/OpenSSL fixes.

## Weekly proposals

`.upstream/moonlight.json` is the machine-readable state, not README prose.
The Monday schedule and `workflow_dispatch` watch canonical upstream **master**.
The baseline advances only on the proposed branch until its PR is accepted.

`scripts/sync-moonlight.py` fetches full upstream history and verifies ancestry.
An unchanged upstream exits successfully. A new upstream head is merged normally
into current Asteria main on `automation/moonlight-upstream-sync`; no conflict
sides are selected. A clean merge records the proposed baseline in the same
merge commit, validates it, then opens or updates one PR. A matching existing
proposal containing current main is reused. Only the disposable proposal branch
can be replaced, using an explicit force-with-lease; main/history are never rewritten.

Conflicts abort the merge and list files plus the old/new range in the workflow
summary and a stable maintenance issue. Validation failures also create/update
that issue and do not publish a successful proposal. Previously opened proposals
are not automatically closed or declared current after a failed attempt: consult
the summary/issue before reviewing them. Close the maintenance issue after manual
integration is verified. Upstream rewinds and invalid baselines fail for manual review.

Weekly validation runs diff whitespace checks, PowerShell dependency/package
guards, CMake configuration and GPU-free PyroWave parser/negotiation/frame/
presentation-pattern/queue regressions. It deliberately omits Qt/qmake, full
client builds, runtime/GPU and physical input qualification.

## Review and full qualification

The built-in `GITHUB_TOKEN` needs contents, pull-request and issue write access;
repository Actions settings must allow it to create PRs. It can never push commits
that change `.github/workflows/`, and upstream ranges sometimes do (for example
`build-appimage.yml` in the `dc99771` range; see #73).

### Optional `UPSTREAM_SYNC_TOKEN`

When the repository secret `UPSTREAM_SYNC_TOKEN` exists, the workflow uses it for
checkout, push and GitHub API calls instead of `GITHUB_TOKEN`. Create a
**fine-grained personal access token** limited to this repository with read and
write access to **Contents**, **Workflows**, **Pull requests** and **Issues**, then
add it under *Settings → Secrets and variables → Actions*. Give it an expiry and
rotate it; an expired secret fails the push the same way as no secret.

Without the secret, the job behaves as before. If GitHub rejects the push, the
maintenance issue now records the rejection and these setup steps instead of the
job ending silently. A maintainer can also push the validated merge manually to a
`sync/moonlight-<sha>` branch and open the PR.

**`GITHUB_TOKEN`-created PRs do not trigger ordinary PR workflows.** In that case,
manually dispatch `Windows baseline` and `Optional PyroWave P1a and offline
regressions` selecting the proposal branch. PRs opened with `UPSTREAM_SYNC_TOKEN`
trigger them normally; the proposal body says which applies. Review both
architecture results/packages before merging.
Weekly checks do not substitute for settings UI, input or live hardware tests. The Windows baseline also runs a focused keyboard harness using production mapping/state expressions (Enter/Numpad Enter, extended keys, non-normalized keys and release-state encoding); it does not qualify physical event delivery or OS capture.

Review overlapping Asteria behavior intentionally: branding/application identity,
native ARM64, PyroWave provenance/stats, standard codec availability, Basic Settings
codec placement, 500/3000 Mbps caps and manual bitrate overrides take precedence
where divergence is deliberate. Update dependency hashes/build records when an
upstream dependency change requires it. Never auto-merge. Preserve merge history
when accepting the PR (use a merge commit rather than squash/rebase).

Local workflow tests: `python -m unittest discover -s tests/upstream -v`.
They cover no-change exit, actual clean two-parent merge, conflict abort,
create-versus-update PR routing, reporting a rejected workflow push (with and
without the token hint) and token-dependent CI guidance. They do not live-test scheduled GitHub behavior.

[Asteria v0.2.0](https://github.com/Unitron07/Asteria-Windows/releases/tag/v0.2.0) is released. This upstream-tracking workflow only proposes reviewed maintenance changes; it does not create releases, tags or release assets.
