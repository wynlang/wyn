#!/bin/bash
# print() atomicity gate.
#
# print() used to emit one libc call per argument plus one for the terminator.
# libc locks a single printf per-FILE but NOT a sequence of them, so concurrent
# spawns interleaved mid-line. Measured on v1.21.0, 8 spawns x 200 lines of 40
# chars, 5 runs: 362 / 690 / 604 / 644 / 682 malformed lines out of 1600 - while
# single-argument println() gave 0 in 5 of 5, because println_* already emits
# value+newline in one call. print() now buffers into a WynOut and emits one
# fwrite.
#
# Why this is not in tests/regression/ as an EXPECT test: run_bdd.sh compares
# output line N against EXPECT line N, and hundreds of writer lines shift every
# position. The semantics of print() (separators, end:, sep:, every scalar type,
# arrays, await in an argument) are pinned by
# tests/regression/test_print_atomic.wyn; this file asserts the interleaving.
#
# The old failure was PROBABILISTIC, so a single run could pass by luck. Run the
# whole program several times and require EVERY run clean - with the old code the
# per-run failure rate was ~100% at this width, so this is not a flaky gate in
# the other direction either.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

WRITERS=8
LINES=200
WIDTH=40
EXPECT_LINES=$((WRITERS * LINES))
ATTEMPTS=5

# Each writer emits LINES lines of exactly WIDTH identical characters. Any
# interleaving shows up as a line whose length is not WIDTH.
cat > "$TMP/atomic.wyn" <<EOF
fn writer(id: int) -> int {
    var i = 0
    while i < $LINES {
        print("$(printf 'A%.0s' $(seq 1 $WIDTH))")
        i = i + 1
    }
    return 0
}
fn main() {
    var fs = []
    for w in 0..$WRITERS { fs.push(spawn writer(w)) }
    await_all(fs)
}
EOF

out=$(perl -e 'alarm(120); exec @ARGV' -- "$WYN" build --release "$TMP/atomic.wyn" -o "$TMP/atomic.bin" 2>&1)
if [ ! -x "$TMP/atomic.bin" ]; then
    bad "build the concurrent print program [$(echo "$out" | grep -m1 -iE 'error' | cut -c1-70)]"
    echo ""; echo "print-atomicity: $PASS pass, $FAIL fail"; exit 1
fi
ok "build the concurrent print program"

worst_bad=0
worst_lines=""
for a in $(seq 1 $ATTEMPTS); do
    perl -e 'alarm(60); exec @ARGV' -- "$TMP/atomic.bin" > "$TMP/out.txt" 2>/dev/null
    total=$(wc -l < "$TMP/out.txt" | tr -d ' ')
    # tr -d '\r' so this is not a false failure on Windows CRLF stdout.
    malformed=$(tr -d '\r' < "$TMP/out.txt" | awk -v w="$WIDTH" 'length($0)!=w' | wc -l | tr -d ' ')
    [ "$malformed" -gt "$worst_bad" ] && worst_bad=$malformed
    if [ "$total" != "$EXPECT_LINES" ]; then worst_lines="$worst_lines got=$total"; fi
done

if [ -z "$worst_lines" ]; then
    ok "all $ATTEMPTS runs emitted exactly $EXPECT_LINES lines"
else
    bad "line count wrong in some run (want $EXPECT_LINES,$worst_lines)"
fi

if [ "$worst_bad" -eq 0 ]; then
    ok "no interleaved line in $ATTEMPTS x $EXPECT_LINES lines from $WRITERS concurrent writers"
else
    bad "interleaving: worst run had $worst_bad malformed lines of $EXPECT_LINES"
fi

# Non-vacuity: the harness must be able to SEE corruption. A deliberately
# two-call sequence (print with end:"" then a separate print) is the shape the
# fix removed, so it should still interleave - if this comes out clean the
# detector is broken and the assertions above prove nothing.
cat > "$TMP/vac.wyn" <<EOF
fn writer(id: int) -> int {
    var i = 0
    while i < $LINES {
        print("$(printf 'A%.0s' $(seq 1 $WIDTH))", end: "")
        print("")
        i = i + 1
    }
    return 0
}
fn main() {
    var fs = []
    for w in 0..$WRITERS { fs.push(spawn writer(w)) }
    await_all(fs)
}
EOF
if perl -e 'alarm(120); exec @ARGV' -- "$WYN" build --release "$TMP/vac.wyn" -o "$TMP/vac.bin" >/dev/null 2>&1; then
    seen=0
    for a in 1 2 3; do
        perl -e 'alarm(60); exec @ARGV' -- "$TMP/vac.bin" > "$TMP/vout.txt" 2>/dev/null
        m=$(tr -d '\r' < "$TMP/vout.txt" | awk -v w="$WIDTH" 'length($0)!=w' | wc -l | tr -d ' ')
        [ "$m" -gt 0 ] && seen=1 && break
    done
    if [ "$seen" -eq 1 ]; then ok "detector is non-vacuous (two-call shape still interleaves)"
    else echo "  ~     detector saw no interleaving in the two-call shape (machine may be too idle to race)"; fi
else
    echo "  ~     could not build the non-vacuity probe (skipped)"
fi

echo ""; echo "print-atomicity: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
