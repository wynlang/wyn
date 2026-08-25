#!/bin/bash
# Dotted module calls are ARITY-CHECKED (2026-07).
#
# `m.foo()` reaches the checker as a METHOD_CALL, and the one site where it met
# its real signature adopted the return type without ever counting arguments. So
# a wrong argument count passed `wyn check` clean and then failed as a raw C
# compiler error - "too few arguments to function call", pointing at generated
# code the programmer never wrote. The `::` spelling of the identical call WAS
# checked, so the two syntaxes disagreed, and the dot form is the one every
# program and every doc uses.
#
# The risk in fixing it is over-reach: this same path carries builtin namespaces
# (Time.now_millis, System.args, Math.pow) whose registered signatures are not
# all faithful, plus defaulted parameters and overload sets. So the checks below
# are half "the error is now reported" and half "everything that used to compile
# still compiles" - the second half is the reason this file is long.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

cat > "$TMP/m.wyn" <<'EOF'
pub fn one_arg(t: int) -> string {
    if t == 0 { return "zero" }
    return "other"
}

pub fn two_args(a: int, b: int) -> int {
    return a + b
}

pub fn no_args() -> int {
    return 42
}

pub fn defaulted(a: int, b: int = 5) -> int {
    return a + b
}
EOF

# ---- the error is reported, at check time, for both directions --------------

cat > "$TMP/few.wyn" <<'EOF'
import m
fn main() {
    var s = m.one_arg()
    print(s)
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check few.wyn 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "Expected:" && echo "$out" | grep -q "1 argument"; then
  ok "too few arguments is a check-time error"
else
  bad "too few: code=$code"; echo "$out" | head -4
fi

# The message must name the function the way the programmer WROTE it. "one_arg"
# alone is ambiguous in a file that imports several modules.
if echo "$out" | grep -q "m.one_arg"; then
  ok "the diagnostic names the call as m.one_arg"
else
  bad "diagnostic does not name m.one_arg"; echo "$out" | head -4
fi

cat > "$TMP/many.wyn" <<'EOF'
import m
fn main() {
    var s = m.one_arg(1, 2, 3)
    print(s)
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check many.wyn 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "Expected:"; then
  ok "too many arguments is a check-time error"
else
  bad "too many: code=$code"; echo "$out" | head -4
fi

# Inside an interpolation too - that is where the bug was actually hit, building
# WynCanvas, and an expression context is a different parse path.
cat > "$TMP/interp.wyn" <<'EOF'
import m
fn main() {
    print("${m.one_arg()}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check interp.wyn 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "Expected:"; then
  ok "reported inside a \${} interpolation"
else
  bad "interpolation: code=$code"; echo "$out" | head -4
fi

# A zero-arg fn called with an argument.
cat > "$TMP/zero.wyn" <<'EOF'
import m
fn main() {
    print(m.no_args(1).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check zero.wyn 2>&1); code=$?
if [ $code -ne 0 ]; then
  ok "an argument passed to a no-arg module fn is rejected"
else
  bad "no_args(1) was accepted"
fi

# ---- and the C compiler is no longer where this surfaces -------------------
# The whole point: `check` used to pass and the BUILD then failed. Assert the
# error text is the checker's and not clang's.
out=$(cd "$TMP" && "$WYN_ABS" build few.wyn 2>&1); code=$?
if [ $code -ne 0 ] && ! echo "$out" | grep -qi "too few arguments to function call"; then
  ok "build fails in the checker, not with a raw C error"
else
  bad "build leaked a C-level diagnostic"; echo "$out" | head -6
fi

# ---- everything that used to compile still compiles ------------------------

cat > "$TMP/good.wyn" <<'EOF'
import m
fn main() {
    print(m.one_arg(0))
    print(m.two_args(1, 2).to_string())
    print(m.no_args().to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check good.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "correct calls still check clean"; else bad "correct calls broke"; echo "$out" | head -4; fi

# A DEFAULTED parameter may be omitted. This is the check that would fail if the
# fix compared against param_count instead of min_param_count.
cat > "$TMP/defs.wyn" <<'EOF'
import m
fn main() {
    print(m.defaulted(1).to_string())
    print(m.defaulted(1, 2).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check defs.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "a defaulted parameter may still be omitted"; else bad "defaulted param broke"; echo "$out" | head -4; fi

# Builtin namespaces travel the same code path. If the fix over-reached, these
# are what break - and they break in every program, not just one.
cat > "$TMP/builtins.wyn" <<'EOF'
fn main() {
    var t = Time.now_millis()
    print(t.to_string())
    var p = Math.pow(2.0, 3.0)
    print(p.to_string())
    var a = System.args()
    print(a.len().to_string())
    var s = "hello".upper()
    print(s)
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check builtins.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "builtin namespace calls unaffected"; else bad "a builtin namespace call broke"; echo "$out" | head -6; fi

# A method call on a VALUE must not be mistaken for a module call.
cat > "$TMP/methods.wyn" <<'EOF'
struct Box { n: int }

impl Box {
    fn double(self) -> int { return self.n * 2 }
}

fn main() {
    var b = Box { n: 21 }
    print(b.double().to_string())
    var xs = [1, 2, 3]
    print(xs.len().to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check methods.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "struct and array method calls unaffected"; else bad "a value method call broke"; echo "$out" | head -6; fi

# The `::` form must keep working exactly as before.
cat > "$TMP/colons.wyn" <<'EOF'
import m
fn main() {
    print(m::one_arg(0))
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check colons.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok ":: form still accepts a correct call"; else bad ":: form broke"; echo "$out" | head -4; fi

# ...and still rejects a wrong one (it always did; this guards the other side).
cat > "$TMP/colons_bad.wyn" <<'EOF'
import m
fn main() {
    print(m::one_arg())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check colons_bad.wyn 2>&1); code=$?
if [ $code -ne 0 ]; then ok ":: form still rejects a wrong count"; else bad ":: form stopped rejecting"; fi

# A module calling its OWN sibling flat must not be affected. `use_helper` calls
# `helper` unqualified from inside the module, which is a different resolution
# path from the dotted call this change touches.
cat > "$TMP/inner.wyn" <<'EOF'
pub fn helper(x: int) -> int { return x + 1 }
pub fn use_helper(x: int) -> int { return helper(x) }
EOF
cat > "$TMP/selfcall.wyn" <<'EOF'
import inner
fn main() {
    print(inner.use_helper(1).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check selfcall.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "a module's internal flat call unaffected"; else bad "internal flat call broke"; echo "$out" | head -4; fi

# The runtime answer must still be right - a checker change that altered the
# emitted call would pass every check above and produce wrong output.
out=$(cd "$TMP" && "$WYN_ABS" run good.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "zero" && echo "$out" | grep -q "^3$" && echo "$out" | grep -q "42"; then
  ok "correct calls still produce the right values at runtime"
else
  bad "runtime output changed"; echo "$out" | head -6
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "module-arity: $PASS pass, 0 fail"
  exit 0
fi
echo "module-arity: $PASS pass, $FAIL fail"
exit 1
