#!/bin/bash
# DEEPLY NESTED JSON MUST BE REJECTED, NOT CRASH THE PROCESS.
#
# THE BUG (#478): the JSON parser recursed once per nesting level with no bound, so
# deeply nested input exhausted the C stack and the process died of SIGSEGV. Reachable
# from untrusted input wherever a program parses a request body or a downloaded
# document - which is the advertised use case for the HTTP client and server.
#
# THE FIX is a depth bound (WYN_JSON_MAX_DEPTH, 200) checked before each level is
# allocated. Exceeding it sets json_parse_failed, which Json_parse already turns into
# an arena rollback and a -1 handle, testable with `Json.is_valid(j)`. No new failure
# channel was needed because the right one already existed.
#
# Arms:
#   1  ordinary nesting still parses                          (the bound is not too low)
#   2  nesting just under the bound still parses              (the boundary, from below)
#   3  nesting just over the bound is REJECTED, process alive  (the boundary, from above)
#   4  pathological nesting is rejected, process alive         (the reported attack)
#
# ARM 4's DEPTH IS CHOSEN TO EXCEED THE STACK, not merely the bound. At 50,000 levels
# an unbounded build parses the document correctly - 50k frames fit in an 8 MB stack -
# so an arm at that depth proves the bound fires but proves nothing about the crash.
# At 150,000 an unbounded build dies: the SIGSEGV handler in src/wyn_wrapper.c:63-75
# recognises a fault in the guard page and reports "panic: stack overflow (recursion
# too deep?)" before _exit(139). So with the fix removed this arm fails by SIGNAL,
# which is the behaviour the bound exists to prevent. Found by mutation-testing the
# first version, which used 50,000 and reported only a wrong answer.
#   5  the rejection is clean: is_valid is false and the handle is unusable, rather
#      than a truncated document that looks parsed
#
# Arms 2 and 3 exist as a PAIR. A single "deep input is rejected" arm would also pass
# if the bound were accidentally 1, which would reject every real document; asserting
# both sides of the boundary is what makes the bound itself the thing under test.
#
# Depth is counted in BRACKETS, matching what the parser recurses on, and the documents
# are generated here rather than committed so the numbers can move with the bound.
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== deeply nested JSON must be rejected, not crash (#478) ==="

# The bound the runtime was built with, read from the source so this gate cannot
# silently drift away from it.
LIMIT=$(sed -n 's/^#define WYN_JSON_MAX_DEPTH \([0-9]*\).*/\1/p' src/wyn_runtime.h | head -1)
if [ -z "$LIMIT" ]; then
    bad "WYN_JSON_MAX_DEPTH is defined in src/wyn_runtime.h"
    echo ""; echo "json-depth: $PASS pass, $FAIL fail"; exit 1
fi
echo "  WYN_JSON_MAX_DEPTH = $LIMIT"

gen() {  # $1 depth -> nested arrays, e.g. [[[1]]]
    python3 -c 'import sys; n=int(sys.argv[1]); sys.stdout.write("["*n + "1" + "]"*n)' "$1"
}

# A program that parses whatever is on stdin and says whether it was valid. One binary,
# reused for every depth, so the arms differ only in the input.
cat > "$TMP/p.wyn" <<'EOF'
fn main() -> int {
    var raw = read_all()
    var j = Json.parse(raw)
    if Json.is_valid(j) {
        println("valid")
    } else {
        println("rejected")
    }
    return 0
}
EOF
if ! TMPDIR="$TMP" perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$TMP/p.wyn" -o "$TMP/p.out" > "$TMP/p.build" 2>&1; then
    # read_all may not exist under that name; fall back to a file-reading shape.
    cat > "$TMP/p.wyn" <<'EOF'
fn main() -> int {
    var raw = File.read(Env.get("DOC"))
    var j = Json.parse(raw)
    if Json.is_valid(j) {
        println("valid")
    } else {
        println("rejected")
    }
    return 0
}
EOF
    if ! TMPDIR="$TMP" perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$TMP/p.wyn" -o "$TMP/p.out" > "$TMP/p.build" 2>&1; then
        bad "the probe program builds"
        grep -iE 'error' "$TMP/p.build" | head -4 | sed 's/^/          /'
        echo ""; echo "json-depth: $PASS pass, $FAIL fail"; exit 1
    fi
fi
ok "the probe program builds"

# $1 label  $2 depth  $3 expected ('valid'|'rejected')
check_depth() {
    local label="$1" depth="$2" want="$3"
    gen "$depth" > "$TMP/$label.json"
    local out rc
    out=$(DOC="$TMP/$label.json" TMPDIR="$TMP" perl -e 'alarm(60); exec @ARGV' -- "$TMP/p.out" < "$TMP/$label.json" 2>&1)
    rc=$?
    if [ "$rc" -ge 128 ]; then
        bad "$label (depth $depth): the process was KILLED by signal $((rc-128)) — it must reject the document and live"
        return
    fi
    if [ "$rc" -ne 0 ]; then
        bad "$label (depth $depth): process exited cleanly (rc=$rc) [$out]"
        return
    fi
    if printf '%s' "$out" | grep -q "^$want$"; then
        ok "$label (depth $depth): $want"
    else
        bad "$label (depth $depth): expected '$want', got [$out]"
    fi
}

# --- 1/2. the bound is not too low ------------------------------------------
check_depth "ordinary"   10                  "valid"
check_depth "just_under" "$((LIMIT - 1))"    "valid"

# --- 3/4. over the bound is rejected, and the process lives -----------------
check_depth "just_over"  "$((LIMIT + 2))"    "rejected"
check_depth "pathologic" 150000              "rejected"

# --- 5. the rejection is clean, not a truncated document --------------------
# Json.parse must hand back -1 (never a valid index) rather than a handle onto a
# partially-built document, which would read as a successful parse of the wrong thing.
cat > "$TMP/h.wyn" <<'EOF'
fn main() -> int {
    var raw = File.read(Env.get("DOC"))
    var j = Json.parse(raw)
    println("handle=${j} valid=${Json.is_valid(j)}")
    return 0
}
EOF
if ! TMPDIR="$TMP" perl -e 'alarm(180); exec @ARGV' -- "$WYN" build "$TMP/h.wyn" -o "$TMP/h.out" > "$TMP/h.build" 2>&1; then
    bad "the handle-inspection program builds"
    grep -iE 'error' "$TMP/h.build" | head -3 | sed 's/^/          /'
else
    gen 150000 > "$TMP/deep.json"
    out=$(DOC="$TMP/deep.json" TMPDIR="$TMP" perl -e 'alarm(60); exec @ARGV' -- "$TMP/h.out" 2>&1)
    if printf '%s' "$out" | grep -q 'handle=-1'; then
        ok "an over-deep document yields the -1 handle, not a partial document"
    else
        bad "an over-deep document yields the -1 handle (got [$out])"
    fi
fi

echo ""
echo "json-depth: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
