#!/bin/bash
# SYNTAX GATE FOR scripts/ - the directory nothing used to look at.
#
# WHY THIS EXISTS
# ---------------
# The measurement harness (scripts/suite.sh, scripts/sweep_*.py) lived outside any
# git repo for months. Nothing built it, nothing ran it, nothing reviewed it - so it
# rotted: hardcoded absolute paths into a scratch directory a cleanup later deleted,
# a verdict line that could not exit nonzero, a regex that silently matched the wrong
# unit. Moving those files into the repo fixes reviewability but not rot: `make test`
# ran 146 `bash tests/...` gates before this one, and not one of them would notice a
# stray `fi` in a shell script or a bad indent in a sweep. The gate that produced a
# release decision became unrunnable precisely because no gate was watching it.
#
# A full behavioural gate on the sweeps is out of reach here - they need TWO built
# compilers - but a SYNTAX check needs nothing and costs under a second, so there is
# no excuse for its absence. This runs `bash -n` over every scripts/*.sh and
# `py_compile` over every scripts/*.py.
#
# TWO THINGS MAKE IT NON-VACUOUS, and both arms are here deliberately:
#
#   1 DISCOVERY FLOORS. A gate that iterates a glob and finds nothing passes with a
#     perfect score. So it asserts how many files it actually DISCOVERED, as a FLOOR
#     (not an equality): deleting the scripts reds this, adding one does not.
#   2 NEGATIVE CONTROLS. It feeds `bash -n` and `py_compile` a file that is KNOWN
#     broken and requires each to reject it. If a future environment turns the
#     checkers into no-ops, the positive arms above would still be green while
#     proving nothing - this is the arm that catches that.
#
# Nothing here asserts the ABSENCE of an error string: every arm asserts a presence
# or an exact status, because an error that moves to the other stream makes an
# absence-assertion pass.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

PASS=0; FAIL=0
ok()  { echo "  ok    $1"; PASS=$((PASS+1)); }
bad() { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Floors sized below today's counts (15 .sh, 5 .py as of 2026-10-05) so a correct
# addition cannot red this, while a thinning glob or a deleted harness does.
MIN_SH=${LINT_MIN_SH:-10}
MIN_PY=${LINT_MIN_PY:-4}

# py_compile writes next to the source by default; hand it an explicit cfile so a
# gate run never leaves __pycache__/ in the tree.
pycheck() {   # $1 = .py path
    python3 - "$1" "$TMP/out.pyc" <<'PY' 2>"$TMP/pyerr"
import py_compile, sys
py_compile.compile(sys.argv[1], cfile=sys.argv[2], doraise=True)
PY
}

# ------------------------------------------------------------------ shell scripts
N_SH=0
for f in "$ROOT"/scripts/*.sh; do
    [ -f "$f" ] || continue
    N_SH=$((N_SH + 1))
    if bash -n "$f" 2>"$TMP/sherr"; then
        :
    else
        bad "bash -n ${f#"$ROOT"/}"
        sed -n '1,5p' "$TMP/sherr" | sed 's/^/          /'
    fi
done
if [ "$N_SH" -ge "$MIN_SH" ]; then
    ok "discovered $N_SH scripts/*.sh (floor $MIN_SH) and every one parses"
else
    bad "discovered only $N_SH scripts/*.sh, floor is $MIN_SH - the glob found almost nothing, so the arms above proved almost nothing"
fi

# ----------------------------------------------------------------- python scripts
if ! command -v python3 >/dev/null 2>&1; then
    # Fail loudly rather than skip: every platform that runs `make test` has python3,
    # so its absence is a broken environment, not a portability case.
    bad "python3 is not on PATH - cannot syntax-check scripts/*.py"
    N_PY=0
else
    N_PY=0
    for f in "$ROOT"/scripts/*.py; do
        [ -f "$f" ] || continue
        N_PY=$((N_PY + 1))
        if pycheck "$f"; then
            :
        else
            bad "py_compile ${f#"$ROOT"/}"
            sed -n '1,6p' "$TMP/pyerr" | sed 's/^/          /'
        fi
    done
    if [ "$N_PY" -ge "$MIN_PY" ]; then
        ok "discovered $N_PY scripts/*.py (floor $MIN_PY) and every one compiles"
    else
        bad "discovered only $N_PY scripts/*.py, floor is $MIN_PY - the glob found almost nothing"
    fi
fi

# --------------------------------------------------------------- negative controls
# Prove the two checkers can still say no. Without these, a `bash -n` that silently
# became a no-op would leave every arm above green.
printf 'if true; then\n  echo unterminated\n' > "$TMP/broken.sh"
if bash -n "$TMP/broken.sh" 2>/dev/null; then
    bad "negative control: bash -n ACCEPTED a script with an unterminated \`if\` - the shell syntax arm is a no-op"
else
    ok "negative control: bash -n rejects a deliberately broken script"
fi

printf 'def f(:\n    pass\n' > "$TMP/broken.py"
if [ "$N_PY" -gt 0 ] && pycheck "$TMP/broken.py"; then
    bad "negative control: py_compile ACCEPTED a syntactically invalid module - the python arm is a no-op"
elif [ "$N_PY" -gt 0 ]; then
    ok "negative control: py_compile rejects a deliberately broken module"
fi

echo ""
echo "lint-scripts: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
