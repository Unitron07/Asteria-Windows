# Maintained minimal delta against the unchanged upstream gitlink. Both baseline
# and experimental builds use this reviewed protocol core; only opt-in clients
# ever advertise PyroWave. No submodule flattening or network fetch during qmake.
COMMON_C_CHECKOUT = $$PWD/moonlight-common-c
COMMON_C_PATCH = $$PWD/../scripts/pyrowave/common-c-p1a.patch
exists($$COMMON_C_CHECKOUT/.git) {
    COMMON_C_PIN = $$system(git -C $$shell_quote($$COMMON_C_CHECKOUT) rev-parse HEAD)
    !equals(COMMON_C_PIN, f900dd4767759c7b9d0e93bcea666b55c69ea62f): error("Unexpected common-c revision; review/rebase the P1a patch first")
} else {
    # The corresponding-source archive contains the already patched submodule,
    # without Git metadata. Require that delta rather than resolving a parent HEAD.
    !system(git -C $$shell_quote($$COMMON_C_CHECKOUT) apply --reverse --check $$shell_quote($$COMMON_C_PATCH)): error("Source archive is missing the reviewed P1a common-c delta")
}
# Checking reverse first makes repeated debug/release qmake generation quiet.
!system(git -C $$shell_quote($$COMMON_C_CHECKOUT) apply --reverse --check $$shell_quote($$COMMON_C_PATCH)) {
    !system(git -C $$shell_quote($$COMMON_C_CHECKOUT) apply --check $$shell_quote($$COMMON_C_PATCH)): error("P1a common-c patch does not apply cleanly")
    !system(git -C $$shell_quote($$COMMON_C_CHECKOUT) apply $$shell_quote($$COMMON_C_PATCH)): error("Cannot apply P1a common-c patch")
}
