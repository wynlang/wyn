#!/usr/bin/env bash
# Build and test the three flagship Wyn apps against a given compiler.
#
# WHY THIS EXISTS: PR #266 passed `make test`, BDD 258/0, golden-C, examples AND
# the wyncanvas suite -- and broke WynJS's build, because no gate built WynJS.
# It was merged and reverted the same day. A compiler change must be built
# against ALL THREE flagship apps before merge, and it must be ONE command so
# that four parallel lanes cannot each forget a different step of an
# eleven-step sequence.
#
# It also closes the two documented false-alarm traps, both of which produce an
# identical wall of "compile error" that looks exactly like a real regression:
#   1. TMPDIR must EXIST. Pointing it at a missing directory makes clang fail
#      with "unable to make temporary file", surfacing as
#      "0 passed, 26 failed (compile error)" across every suite.
#   2. BOTH C shims must be built, gui FIRST: wyncanvas links ../../gui/libgui
#      per its wyn.toml, so the gui shim is a prerequisite even when only
#      wyncanvas is being gated.
#
# Usage:
#   scripts/flagship_apps_gate.sh [--quick]
#
#   WYN_ROOT   compiler to test (default: the repo this script lives in)
#   WYN_WS     workspace root holding repos/ (default: derived from WYN_ROOT)
#   TMPDIR     scratch dir; created if missing
#   --quick    skip the two slowest gates (node parity, VisualWyn) for the
#              edit loop. NOT a merge gate -- a merge needs the full run.
#
# Exit 0 only if every gate met its recorded baseline.

set -uo pipefail

QUICK=0
[ "${1:-}" = "--quick" ] && QUICK=1

# --- locate the compiler and the workspace ----------------------------------
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
WYN_ROOT="${WYN_ROOT:-$(dirname "$SCRIPT_DIR")}"
WYN_ROOT=$(cd "$WYN_ROOT" && pwd)
WYN="$WYN_ROOT/wyn"

if [ ! -x "$WYN" ]; then
    echo "FATAL: no compiler at $WYN -- run make in $WYN_ROOT first" >&2
    exit 2
fi

# The apps live in the workspace, not in the compiler worktree. A lane running
# from .claude/worktrees/<name> must still find repos/, so walk up looking for
# it rather than assuming ../../.
if [ -z "${WYN_WS:-}" ]; then
    d="$WYN_ROOT"
    while [ "$d" != "/" ]; do
        if [ -d "$d/repos/sample-apps" ] && [ -d "$d/repos/gui" ]; then
            WYN_WS="$d"; break
        fi
        d=$(dirname "$d")
    done
fi
if [ -z "${WYN_WS:-}" ] || [ ! -d "$WYN_WS/repos/sample-apps" ]; then
    echo "FATAL: cannot find the workspace (repos/sample-apps + repos/gui)." >&2
    echo "       Set WYN_WS=/path/to/wynlang explicitly." >&2
    exit 2
fi

GUI="$WYN_WS/repos/gui"
WYNJS="$WYN_WS/repos/sample-apps/wynjs"
WYNCANVAS="$WYN_WS/repos/sample-apps/wyncanvas"
WYNSTORM="$WYN_WS/repos/sample-apps/games/wynstorm"

# --- trap 1: TMPDIR must exist ---------------------------------------------
export TMPDIR="${TMPDIR:-/tmp/wyn-flagship-gate}"
mkdir -p "$TMPDIR" || { echo "FATAL: cannot create TMPDIR=$TMPDIR" >&2; exit 2; }
LOGDIR="$TMPDIR/flagship-gate"
rm -rf "$LOGDIR"; mkdir -p "$LOGDIR"

export WYN_ROOT
export WYN

echo "flagship apps gate"
echo "  compiler : $WYN ($(cd "$WYN_ROOT" && git rev-parse --short HEAD 2>/dev/null || echo '?'))"
echo "  workspace: $WYN_WS"
echo "  logs     : $LOGDIR"
echo

FAILED=0
declare -a RESULTS=()

record() { # name verdict detail
    RESULTS+=("$1|$2|$3")
    # Only FAIL fails the run. SKIP is a deliberate omission (--quick, or an app
    # that is not checked out) and must not be conflated with a failure -- a
    # gate that cries wolf on a skip is a gate people learn to ignore.
    if [ "$2" = "FAIL" ]; then FAILED=1; fi
    printf '  %-22s %-6s %s\n' "$1" "$2" "$3"
}

# --- trap 2: both C shims, gui FIRST ---------------------------------------
echo "building C shims (gui first -- wyncanvas links ../../gui/libgui)"
if (cd "$GUI" && ./csrc/build.sh) > "$LOGDIR/shim_gui.log" 2>&1; then
    record "shim:gui" PASS ""
else
    record "shim:gui" FAIL "see $LOGDIR/shim_gui.log"
    echo
    echo "The gui shim failed to build. Every downstream suite would now report"
    echo "'compile error' -- that is this trap, not a compiler regression."
    exit 1
fi
if (cd "$WYNCANVAS" && ./csrc/build.sh) > "$LOGDIR/shim_wyncanvas.log" 2>&1; then
    record "shim:wyncanvas" PASS ""
else
    record "shim:wyncanvas" FAIL "see $LOGDIR/shim_wyncanvas.log"
    exit 1
fi
echo

# ---------------------------------------------------------------------------
# WynJS -- the gate #266 lacked. Run it FIRST for exactly that reason: if a
# compiler change breaks one flagship app, this is the one it has broken before.
# ---------------------------------------------------------------------------
echo "WynJS"
log="$LOGDIR/wynjs_suite.log"
if (cd "$WYNJS" && WYN="$WYN" "$WYN" test tests/test_main.wyn) > "$log" 2>&1; then
    # NOTE: `wyn test` appends its OWN summary ("1 passed, 0 failed" -- one .wyn
    # file) AFTER the app's own "33 pass, 0 fail". Taking the LAST match reads
    # the runner's file count, not the app's assertion count, and would let a
    # 1/33-suite collapse report as a pass. Match the app's line specifically.
    n=$(grep -oE '^[0-9]+ pass' "$log" | head -1 | grep -oE '[0-9]+' || echo 0)
    f=$(grep -oE '[0-9]+ fail$' "$log" | head -1 | grep -oE '[0-9]+' || echo 0)
    # baseline 33 pass / 0 fail
    if [ "${f:-1}" -eq 0 ] && [ "${n:-0}" -ge 33 ]; then
        record "wynjs:suite" PASS "$n pass, $f fail"
    else
        record "wynjs:suite" FAIL "$n pass, $f fail (baseline 33/0) -- $log"
    fi
else
    # A BUILD failure lands here, and that is the #266 signature exactly.
    if grep -qE 'use of undeclared identifier|error:' "$log"; then
        record "wynjs:suite" FAIL "BUILD BROKE (the #266 signature) -- $log"
    else
        record "wynjs:suite" FAIL "see $log"
    fi
fi

if [ "$QUICK" -eq 0 ]; then
    log="$LOGDIR/wynjs_parity.log"
    if (cd "$WYNJS" && WYN="$WYN" bash tests/run_node_parity.sh) > "$log" 2>&1; then
        ident=$(grep -oE '[0-9]+ identical' "$log" | tail -1 | grep -oE '[0-9]+' || echo 0)
        unexp=$(grep -oE '[0-9]+ unexplained' "$log" | tail -1 | grep -oE '[0-9]+' || echo 0)
        if [ "${unexp:-1}" -eq 0 ] && [ "${ident:-0}" -ge 29 ]; then
            record "wynjs:parity" PASS "$ident identical, $unexp unexplained"
        else
            record "wynjs:parity" FAIL "$ident identical, $unexp unexplained (baseline 29/0) -- $log"
        fi
    else
        record "wynjs:parity" FAIL "see $log"
    fi
else
    record "wynjs:parity" SKIP "--quick"
fi
echo

# ---------------------------------------------------------------------------
# WynCanvas
# ---------------------------------------------------------------------------
echo "WynCanvas"
log="$LOGDIR/wyncanvas_suite.log"
if (cd "$WYNCANVAS" && "$WYN" test) > "$log" 2>&1; then
    p=$(grep -oE '[0-9]+ passed' "$log" | tail -1 | grep -oE '[0-9]+' || echo 0)
    f=$(grep -oE '[0-9]+ failed' "$log" | tail -1 | grep -oE '[0-9]+' || echo 0)
    if [ "${f:-1}" -eq 0 ] && [ "${p:-0}" -ge 26 ]; then
        record "wyncanvas:suite" PASS "$p suites, $f fail"
    else
        record "wyncanvas:suite" FAIL "$p passed, $f failed (baseline 26/0) -- $log"
    fi
else
    record "wyncanvas:suite" FAIL "see $log"
fi

# The UI gate. verify_ui.sh writes a BMP screenshot; the word "Layers" was
# absent from the window for weeks because 171 pixel assertions could not see
# a missing heading. Always report the path so a human can open it.
log="$LOGDIR/wyncanvas_ui.log"
uibin="$TMPDIR/wcui"
if (cd "$WYNCANVAS" && "$WYN" build src/ui.wyn -o "$uibin" && ./verify_ui.sh "$uibin") > "$log" 2>&1; then
    a=$(grep -oE '^[0-9]+ passed' "$log" | tail -1 | grep -oE '^[0-9]+' || echo 0)
    af=$(grep -oE '[0-9]+ failed' "$log" | tail -1 | grep -oE '^[0-9]+' || echo 0)
    if [ "${a:-0}" -ge 171 ] && [ "${af:-1}" -eq 0 ]; then
        record "wyncanvas:ui" PASS "$a assertions, $af failed"
    else
        record "wyncanvas:ui" FAIL "$a assertions, $af failed (baseline 171/0) -- $log"
    fi
else
    record "wyncanvas:ui" FAIL "see $log"
fi
[ -f /tmp/wc_verify.bmp ] && echo "    screenshot: /tmp/wc_verify.bmp  <- OPEN IT; pixel assertions cannot see a missing heading"
echo

# ---------------------------------------------------------------------------
# VisualWyn (repos/gui). MUST run from the repo ROOT: two suites generate .wyn
# files that `import widgets`, resolved via ./src/ relative to the CWD.
# ---------------------------------------------------------------------------
echo "VisualWyn"
if [ "$QUICK" -eq 0 ]; then
    log="$LOGDIR/visualwyn.log"
    if (cd "$GUI" && WYN_ROOT="$WYN_ROOT" ./tests/run.sh) > "$log" 2>&1; then
        p=$(grep -oE '[0-9]+ passed' "$log" | tail -1 | grep -oE '[0-9]+' || echo 0)
        f=$(grep -oE '[0-9]+ failed' "$log" | tail -1 | grep -oE '[0-9]+' || echo 0)
        if [ "${f:-1}" -eq 0 ] && [ "${p:-0}" -ge 15 ]; then
            record "visualwyn:suite" PASS "$p passed, $f failed"
        else
            record "visualwyn:suite" FAIL "$p passed, $f failed (baseline 15/0) -- $log"
        fi
    else
        record "visualwyn:suite" FAIL "see $log"
    fi
else
    record "visualwyn:suite" SKIP "--quick"
fi
echo

# ---------------------------------------------------------------------------
# WYNSTORM -- cheap, and the densest defect-per-line corpus of the four.
# ---------------------------------------------------------------------------
echo "WYNSTORM"
log="$LOGDIR/wynstorm.log"
if [ -d "$WYNSTORM" ]; then
    if (cd "$WYNSTORM" && "$WYN" test tests/test_engine.wyn) > "$log" 2>&1; then
        # As with WynJS: the app prints "32 tests passed", then `wyn test` adds
        # its own "1 passed" file count. Read the app's line, not the runner's.
        p=$(grep -oE '[0-9]+ tests passed' "$log" | head -1 | grep -oE '^[0-9]+' || echo 0)
        f=$(grep -oE '[0-9]+ (tests )?failed' "$log" | head -1 | grep -oE '^[0-9]+' || echo 0)
        if [ "${f:-1}" -eq 0 ] && [ "${p:-0}" -ge 32 ]; then
            record "wynstorm:engine" PASS "$p pass, $f fail"
        else
            record "wynstorm:engine" FAIL "$p pass, $f fail (baseline 32/0) -- $log"
        fi
    else
        record "wynstorm:engine" FAIL "see $log"
    fi
else
    record "wynstorm:engine" SKIP "not present"
fi

# --- summary ---------------------------------------------------------------
echo
echo "─────────────────────────────────────────────────────────────"
for r in "${RESULTS[@]}"; do
    IFS='|' read -r name verdict detail <<< "$r"
    printf '  %-22s %s %s\n' "$name" "$verdict" "$detail"
done
echo "─────────────────────────────────────────────────────────────"
if [ "$FAILED" -eq 0 ]; then
    if [ "$QUICK" -eq 1 ]; then
        echo "FLAGSHIP APPS: PASS (--quick: NOT a merge gate, rerun without --quick)"
    else
        echo "FLAGSHIP APPS: PASS -- safe to merge on this axis"
    fi
    exit 0
fi
echo "FLAGSHIP APPS: FAIL -- do NOT merge. Logs in $LOGDIR"
echo
echo "Before believing a regression, rule out the two look-alikes:"
echo "  * host contention: check 'uptime'. A defect does not move between"
echo "    suites; contention does (at load 12 the gui suite failed ~1 in 4)."
echo "  * stale shims / TMPDIR: both are handled above, but a hand-run gate"
echo "    outside this script is the usual source of a phantom wall of errors."
exit 1
