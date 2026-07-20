#!/bin/bash
# Collection type-safety: mismatched channel sends, array pushes (method
# form), and map/array index stores used to PASS wyn check and corrupt at
# runtime (string pointer read as int, floats truncated). All must be clean
# check-time errors now. Positive cases: tests/expect/
# test_channel_typed_payloads.wyn.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

expect_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 1 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}
expect_pass() {
    local name="$1"; local file="$2"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then ok "$name"; else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}

# 1. Channel: string after int send.
printf 'fn main() {\n    ch = channel(4)\n    ch.send(1)\n    ch.send("nope")\n}\n' > "$TMP/a.wyn"
expect_error "channel send type mismatch" "$TMP/a.wyn" "Cannot send string through a channel of int"

# 2. Method-form push: string into int array (function form already errored).
printf 'fn main() {\n    a = [1, 2, 3]\n    a.push("oops")\n    println(a[3])\n}\n' > "$TMP/b.wyn"
expect_error "hetero push (method form)" "$TMP/b.wyn" "Cannot push string into array of int"

# 3. Map index store: string value into int-valued map.
printf 'fn main() {\n    m = {"x": 1}\n    m["y"] = "s"\n    println(m["y"])\n}\n' > "$TMP/c.wyn"
expect_error "map store mismatch" "$TMP/c.wyn" "Cannot store string in map"

# 4. Array element store: string into int array.
printf 'fn main() {\n    a = [1, 2, 3]\n    a[0] = "s"\n    println(a[0])\n}\n' > "$TMP/d.wyn"
expect_error "array element store mismatch" "$TMP/d.wyn" "Cannot store string in array"

# 5. Positive: homogeneous ops, int->float coercions, empty-array inference,
#    int channel through a spawn worker param all still check clean.
printf 'fn w(ch: int) {\n    ch.send(9)\n}\nfn main() {\n    a = [1]\n    a.push(2)\n    f = [1.5]\n    f.push(2)\n    m = {"x": 1}\n    m["y"] = 2\n    e = []\n    e.push("s")\n    ch = channel(2)\n    spawn w(ch)\n    v = ch.recv()\n    println(v)\n}\n' > "$TMP/e.wyn"
expect_pass "homogeneous ops still pass" "$TMP/e.wyn"

echo ""; echo "collection-types: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
