#!/bin/bash
# StringBuilder.to_string() must return an OWNED COPY, not the live buffer.
#
# It returned sb_pool[handle].data directly, so every snapshot ALIASED the
# builder and appending mutated strings the caller already held:
#
#     sb.append("a");  s1 = sb.to_string()   # "a"
#     sb.append("b");  s2 = sb.to_string()   # "ab"
#     # ... and s1 is NOW "ab" too
#
# Measured s1=ab s2=ab where s1=a is correct: a silent wrong answer at exit 0,
# the worst failure class. And it is not merely stale - the pointer DANGLES the
# moment append() has to realloc, so the value can become garbage rather than
# just newer text. That is why the growth case below is asserted separately: a
# small string may live in the original allocation and appear to work.
#
# NOTE FOR ANYONE RE-RUNNING THIS: to_string lives in the RUNTIME, which is
# precompiled into runtime/libwyn_rt.a. After editing wyn_runtime.h you must
# `rm -f runtime/obj/*.o && make runtime`, or compiled programs keep the old
# behaviour and this test will appear to fail against a correct source tree.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

run_expect() {   # $1=name $2=expected-line ; source on stdin
    local f="$TMP/t.wyn"; cat > "$f"
    local out rc
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$f" 2>&1); rc=$?
    if [ $rc -ne 0 ]; then
        bad "$1 (rc=$rc)"; echo "$out" | grep -E 'error:|^Error' | head -2 | sed 's/^/        /'; return
    fi
    if echo "$out" | grep -qxF "$2"; then ok "$1"
    else bad "$1 (want '$2', got: $(echo "$out" | grep -vE 'Compiled in|^$|Warning:' | tr '\n' '|'))"; fi
}

# --- the aliasing bug ------------------------------------------------------
run_expect "an earlier snapshot is not changed by a later append" "s1=a s2=ab" <<'WYN'
fn main() {
    sb = StringBuilder.new()
    sb.append("a")
    s1 = sb.to_string()
    sb.append("b")
    s2 = sb.to_string()
    print("s1=${s1} s2=${s2}")
}
WYN

# --- growth past the initial allocation -----------------------------------
# A short string can sit inside the original buffer, so the alias may look
# harmless. Append enough to force a realloc: with the bug the first snapshot
# points at freed memory, so this catches the dangling case as well as staleness.
run_expect "a snapshot survives a reallocating append" "first=x len=1" <<'WYN'
fn main() {
    sb = StringBuilder.new()
    sb.append("x")
    first = sb.to_string()
    i = 0
    while i < 500 {
        sb.append("0123456789")
        i = i + 1
    }
    print("first=${first} len=${first.len()}")
}
WYN

# --- clear() must not corrupt an existing snapshot -------------------------
run_expect "a snapshot survives clear()" "kept=hello now=" <<'WYN'
fn main() {
    sb = StringBuilder.new()
    sb.append("hello")
    kept = sb.to_string()
    sb.clear()
    now = sb.to_string()
    print("kept=${kept} now=${now}")
}
WYN

# --- and the normal use must still be correct ------------------------------
run_expect "ordinary accumulation still works" "abc 3" <<'WYN'
fn main() {
    sb = StringBuilder.new()
    sb.append("a")
    sb.append("b")
    sb.append("c")
    s = sb.to_string()
    print("${s} ${sb.len()}")
}
WYN

# --- two builders must not share --------------------------------------------
run_expect "two builders stay independent" "one two" <<'WYN'
fn main() {
    a = StringBuilder.new()
    b = StringBuilder.new()
    a.append("one")
    b.append("two")
    print("${a.to_string()} ${b.to_string()}")
}
WYN

echo ""; echo "stringbuilder: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
