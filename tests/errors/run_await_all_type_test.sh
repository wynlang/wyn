#!/bin/bash
# await_all element typing gate: await_all([f1,f2]) is typed [T] where T is the
# futures' common result type. A MIXED-type array of futures must be a clean
# `wyn check` error (rc=1) - it used to pass check and miscompile (every result
# silently typed int; string results died with "Unknown method ... for type
# 'int'" at codegen). The value paths (string/float/struct results decode as T)
# live in tests/regression/test_await_all_{string,float,struct}_results.wyn.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

expect_check_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 1 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}

expect_check_ok() {
    local name="$1"; local file="$2"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}

# --- Mixed-type array-of-futures literal must be rejected at check time ------
cat > "$TMP/mixed_literal.wyn" <<'EOF'
fn shout(s: string) -> string { return s.upper() }
fn double_it(x: int) -> int { return x * 2 }
fn main() {
    var f1 = spawn shout("a")
    var f2 = spawn double_it(2)
    results = await_all([f1, f2])
    print(results[0])
}
EOF
expect_check_error "await_all mixed-type future literal rejected" \
    "$TMP/mixed_literal.wyn" "consistent types"

# --- Mixed-type push-built futures array must be rejected at check time ------
cat > "$TMP/mixed_push.wyn" <<'EOF'
fn shout(s: string) -> string { return s.upper() }
fn double_it(x: int) -> int { return x * 2 }
fn main() {
    var futures = []
    futures.push(spawn double_it(1))
    futures.push(spawn shout("a"))
    results = await_all(futures)
    print(results[0])
}
EOF
expect_check_error "await_all mixed-type future push rejected" \
    "$TMP/mixed_push.wyn" "Cannot push"

# --- Homogeneous string / float / struct futures must still check clean ------
cat > "$TMP/ok_string.wyn" <<'EOF'
fn shout(s: string) -> string { return s.upper() }
fn main() {
    var f1 = spawn shout("a")
    var f2 = spawn shout("b")
    results = await_all([f1, f2])
    print(results[0].len())
    print(results[0])
}
EOF
expect_check_ok "await_all [string] results check clean (.len() allowed)" "$TMP/ok_string.wyn"

cat > "$TMP/ok_float.wyn" <<'EOF'
fn half(x: int) -> float { return x.to_float() / 2.0 }
fn main() {
    results = await_all([spawn half(3), spawn half(5)])
    print(results[0] + results[1])
}
EOF
expect_check_ok "await_all [float] results check clean" "$TMP/ok_float.wyn"

cat > "$TMP/ok_struct.wyn" <<'EOF'
struct Point { x: int, y: int }
fn mk(a: int) -> Point { return Point{ x: a, y: a + 1 } }
fn main() {
    results = await_all([spawn mk(1), spawn mk(2)])
    print(results[0].x)
}
EOF
expect_check_ok "await_all [struct] results check clean (field access allowed)" "$TMP/ok_struct.wyn"

echo ""
echo "await_all-type: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
