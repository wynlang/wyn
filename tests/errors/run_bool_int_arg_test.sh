#!/bin/bash
# A comparison passed straight to a `bool` parameter (2026-08).
#
# `wrap(f(x) == 1)` was REJECTED with "Expected: bool  Got: int", while the
# identical value hoisted into a local first was ACCEPTED:
#
#     fn wrap(ok: bool) -> bool { return ok }
#     wrap(f(x) == 1)                    # error
#     var t = f(x) == 1  wrap(t)         # fine
#
# So the same expression was legal or illegal depending on whether it passed
# through a variable, which is not a rule anyone can learn. Found writing ordinary
# Wyn: a one-line `return changed(sel_all(s) == 1)` wrapper in WynCanvas's
# selection module.
#
# The cause is that this compiler types a COMPARISON as int, not bool - see the
# `expr_type = builtin_int` ending the comparison branch in EXPR_BINARY, and the
# note on the and/or branch above it: the lambda and predicate runtime ABI
# (long long (*fn)(...)) depends on that choice. A `var` declaration special-cases
# the operator to declare bool, which is why the hoisted form worked.
#
# So the fix is in wyn_is_type_compatible, not in the comparison's type: bool and
# int are one representation with two spellings here (`if 1 { }` has always been
# legal), exactly like the enum<->int and channel<->int rules beside it. That
# function only decides whether a call is ACCEPTED, so no generated code changes.
#
# This file therefore checks both halves: the call is accepted, AND the value that
# arrives is still right - a compatibility rule that let the wrong bits through
# would pass an acceptance-only test.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- accepted, and the value is correct ------------------------------------

cat > "$TMP/a.wyn" <<'EOF'
fn plain(a: int) -> int { return a }
fn wrap(ok: bool) -> bool { return ok }

fn main() {
    // A comparison straight into a bool parameter, the shape that was rejected.
    print(wrap(plain(1) == 1).to_string())
    print(wrap(plain(1) == 2).to_string())
    // A literal comparison, the same path with no call involved.
    print(wrap(1 == 1).to_string())
    print(wrap(2 < 1).to_string())
    // and/or also yields int in this compiler, so it travels the same rule.
    print(wrap(1 == 1 and 2 == 2).to_string())
    print(wrap(1 == 1 and 2 == 3).to_string())
    // An int-typed VALUE into a bool parameter: one representation, two
    // spellings, matching `if 1 { }`.
    print(wrap(plain(0)).to_string())
    print(wrap(plain(7)).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "a comparison is accepted as a bool argument"; else bad "check failed"; echo "$out" | head -6; fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want="true
false
true
false
true
false
false
true"
got=$(printf '%s' "$out" | grep -E '^(true|false)$')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "every value arrives correctly (truth is preserved, not just accepted)"
else
  bad "runtime values wrong"; printf '%s\n' "$out" | head -12
fi

# The false cases matter on their own: a rule that coerced everything to `true`
# would satisfy an acceptance test and half of a values test.
if printf '%s' "$got" | grep -q '^false$'; then
  ok "a FALSE comparison stays false through the parameter"
else
  bad "no false value came back - suspect coercion to true"
fi

# ---- the hoisted form still works (it always did) --------------------------

cat > "$TMP/b.wyn" <<'EOF'
extern fn some_c_fn(a: int) -> int;
fn wrap(ok: bool) -> bool { return ok }
fn caller(h: int) -> bool { var t = some_c_fn(h) == 1  return wrap(t) }
fn main() { print("ok") }
EOF
out=$(cd "$TMP" && "$WYN_ABS" check b.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "the hoisted-into-a-local form still checks"; else bad "hoisted form broke"; echo "$out" | head -4; fi

# An extern fn's result compared inline - the exact WynCanvas shape.
cat > "$TMP/c.wyn" <<'EOF'
extern fn some_c_fn(a: int) -> int;
fn wrap(ok: bool) -> bool { return ok }
fn caller(h: int) -> bool { return wrap(some_c_fn(h) == 1) }
fn main() { print("ok") }
EOF
out=$(cd "$TMP" && "$WYN_ABS" check c.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "an extern fn's result compared inline is accepted"; else bad "extern inline broke"; echo "$out" | head -4; fi

# Through a MODULE boundary, which is where it was actually hit.
cat > "$TMP/m.wyn" <<'EOF'
fn changed(ok: bool) -> bool { return ok }
pub fn probe(n: int) -> bool { return changed(n == 1) }
EOF
cat > "$TMP/d.wyn" <<'EOF'
import m
fn main() {
    print(m.probe(1).to_string())
    print(m.probe(2).to_string())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^true$' && printf '%s' "$out" | grep -q '^false$'; then
  ok "a module fn wrapping a comparison works, both truth values"
else
  bad "module path wrong: code=$code"; printf '%s\n' "$out" | head -6
fi

# ---- and a REAL type error is still an error -------------------------------
# The risk in relaxing a compatibility rule is relaxing it too far. bool<->int is
# one representation; string->bool is not, and must still be refused.

cat > "$TMP/e.wyn" <<'EOF'
fn wrap(ok: bool) -> bool { return ok }
fn main() { print(wrap("yes").to_string()) }
EOF
out=$(cd "$TMP" && "$WYN_ABS" check e.wyn 2>&1); code=$?
if [ $code -ne 0 ]; then ok "a string is still refused for a bool parameter"; else bad "string was accepted as bool"; fi

cat > "$TMP/f.wyn" <<'EOF'
fn takes_int(n: int) -> int { return n }
fn main() { print(takes_int("no").to_string()) }
EOF
out=$(cd "$TMP" && "$WYN_ABS" check f.wyn 2>&1); code=$?
if [ $code -ne 0 ]; then ok "a string is still refused for an int parameter"; else bad "string was accepted as int"; fi

# A float into a bool parameter is not the same representation either.
cat > "$TMP/g.wyn" <<'EOF'
fn wrap(ok: bool) -> bool { return ok }
fn main() { print(wrap(1.5).to_string()) }
EOF
out=$(cd "$TMP" && "$WYN_ABS" check g.wyn 2>&1); code=$?
if [ $code -ne 0 ]; then ok "a float is still refused for a bool parameter"; else bad "float was accepted as bool"; fi

# ---- bool still works where it always did ---------------------------------

cat > "$TMP/h.wyn" <<'EOF'
fn wrap(ok: bool) -> bool { return ok }
fn main() {
    print(wrap(true).to_string())
    print(wrap(false).to_string())
    var b = true
    print(wrap(b).to_string())
    if wrap(1 == 1) { print("branch taken") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run h.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q 'branch taken'; then
  ok "literal bools and a bool-returning call in an if-condition still work"
else
  bad "plain bool usage broke"; printf '%s\n' "$out" | head -6
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "bool-int-arg: $PASS pass, 0 fail"
  exit 0
fi
echo "bool-int-arg: $PASS pass, $FAIL fail"
exit 1
