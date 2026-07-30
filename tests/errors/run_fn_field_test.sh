#!/bin/bash
# Function-typed struct fields: `struct Button { on_click: fn(int) -> void }`,
# the callback/event-handler shape a GUI toolkit is built out of.
#
# These assert VALUES, not just that compilation succeeds, because every bug this
# feature had was a silent wrong answer at exit 0 rather than a crash:
#
#   - a bare function name stored into a WynClosure {fn, env} left .env
#     uninitialised while the call site passed it as argument 0 regardless, so
#     every real argument shifted one slot: `h.f(21)` printed 0, not 42.
#   - a captured `n = 5` printed 5 for `h.f(10)`, because the shifted call read
#     0 + 5 rather than 10 + 5. Nothing failed; the numbers were just wrong.
#
# So a test that only ran `wyn check` would have passed against both bugs.
#
# The three C shapes that must all work through ONE field (see the comment at the
# EXPR_METHOD_CALL fn-field branch in codegen_expr.c):
#   named fn          -> f(args...)             no env
#   non-capturing     -> __lambda_N(args...)    no env
#   returned closure  -> __lambda_N(void*, ...) env required
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Assert the program's OUTPUT, not merely its exit status.
expect_out() {
    local name="$1"; local file="$2"; local want="$3"
    local out rc
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$file" 2>&1); rc=$?
    if [ $rc -ne 0 ]; then
        bad "$name (rc=$rc) [$(echo "$out" | grep -v 'Compiled in' | head -2)]"; return
    fi
    if echo "$out" | grep -qxF "$want"; then ok "$name"
    else bad "$name (want '$want', got '$(echo "$out" | grep -v 'Compiled in' | tr '\n' '|')')"; fi
}

# --- no arguments: the plainest event handler ------------------------------
cat > "$TMP/noarg.wyn" <<'EOF'
struct Button { label: string, on_click: fn() -> void }
fn hit() { println("hit") }
fn main() {
    var b = Button { label: "OK", on_click: hit }
    b.on_click()
}
EOF
expect_out "named fn in a fn-typed field, no args" "$TMP/noarg.wyn" "hit"

# --- one argument: the argument-shift regression ---------------------------
cat > "$TMP/named_arg.wyn" <<'EOF'
struct H { f: fn(int) -> int }
fn dbl(x: int) -> int { return x * 2 }
fn main() {
    var h = H { f: dbl }
    println(h.f(21))
}
EOF
expect_out "named fn with an argument returns the right value" "$TMP/named_arg.wyn" "42"

# --- a lambda in a struct initializer -------------------------------------
# scan_expr_for_lambdas had no EXPR_STRUCT_INIT arm, so this lambda was never
# visited and no top-level function was emitted, while the initializer still
# referenced it: "use of undeclared identifier '__lambda_1'".
cat > "$TMP/lambda.wyn" <<'EOF'
struct H { f: fn(int) -> int }
fn main() {
    var h = H { f: (x) => x * 2 }
    println(h.f(21))
}
EOF
expect_out "non-capturing lambda in a fn-typed field" "$TMP/lambda.wyn" "42"

# --- a CAPTURING lambda: must see its captured value AND its argument -----
cat > "$TMP/capture.wyn" <<'EOF'
struct H { f: fn(int) -> int }
fn main() {
    var n = 5
    var h = H { f: (x) => x + n }
    println(h.f(10))
}
EOF
expect_out "capturing lambda sees both capture and argument" "$TMP/capture.wyn" "15"

# --- void return WITH arguments ------------------------------------------
# Distinct from the no-arg void case: the call is emitted as a ternary over
# env, and a void-typed ternary is only legal in some positions.
cat > "$TMP/void_args.wyn" <<'EOF'
struct Btn { on_click: fn(int, int) -> void }
fn moved(x: int, y: int) { println("at " + x.to_string() + "," + y.to_string()) }
fn main() {
    var b = Btn { on_click: moved }
    b.on_click(3, 4)
}
EOF
expect_out "void return with two arguments" "$TMP/void_args.wyn" "at 3,4"

# --- float: the ABI trap -------------------------------------------------
# The closure-VARIABLE path forced the int ABI and returned garbage bits for
# `fn(float) -> float`. This field path must read its C types from the
# annotation instead, so assert a fractional value that an int ABI would destroy.
cat > "$TMP/float.wyn" <<'EOF'
struct M { scale: fn(float) -> float }
fn half(v: float) -> float { return v / 2.0 }
fn main() {
    var m = M { scale: half }
    println(m.scale(9.0))
}
EOF
expect_out "float parameter and return survive the call" "$TMP/float.wyn" "4.5"

# --- string ---------------------------------------------------------------
cat > "$TMP/string.wyn" <<'EOF'
struct G { fmt: fn(string) -> string }
fn shout(s: string) -> string { return s + "!" }
fn main() {
    var g = G { fmt: shout }
    println(g.fmt("hey"))
}
EOF
expect_out "string parameter and return" "$TMP/string.wyn" "hey!"

# --- two callback fields, plus a non-callback field, in one struct --------
# Guards the field lookup: it must match by NAME, not by position.
cat > "$TMP/multi.wyn" <<'EOF'
struct W { label: string, on_click: fn() -> void, on_key: fn(int) -> void }
fn clicked() { println("click") }
fn keyed(k: int) { println("key " + k.to_string()) }
fn main() {
    var w = W { label: "b", on_click: clicked, on_key: keyed }
    w.on_click()
    w.on_key(65)
}
EOF
expect_out "two callback fields dispatch independently" "$TMP/multi.wyn" "key 65"

# --- a real method on a struct that ALSO has a callback field -------------
# The fn-field branch runs BEFORE method dispatch, so it must not swallow a
# genuine method call on the same struct.
cat > "$TMP/method.wyn" <<'EOF'
struct C { n: int, on_tick: fn() -> void }
fn C.describe(self) -> string { return "n=" + self.n.to_string() }
fn ticked() { println("tick") }
fn main() {
    var c = C { n: 7, on_tick: ticked }
    c.on_tick()
    println(c.describe())
}
EOF
expect_out "a real method still dispatches on a struct with a callback field" "$TMP/method.wyn" "n=7"

echo ""; echo "fn-field: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
