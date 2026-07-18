#!/bin/bash
# Imported-module codegen family (M1-M4, 2026-07-18, found building wynlang/web):
#  M1 sibling call to a fn named `path` (a common_locals name) lost its module prefix
#  M2 for-in loop var: declaration emitted bare, uses got module-prefixed
#  M3 pre-return release fired on locals REFERENCED by the return expression
#     (use-after-free: `return "<y>" + t + "</y>"` read freed t → empty output)
#  M4 unannotated void module fn defaulted to long long (prototype vs def conflict)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

mkdir -p "$TMP/proj/src" "$TMP/proj/tests"
cat > "$TMP/proj/src/m.wyn" <<'EOF'
pub fn esc(s: string) -> string {
    var out = s.replace_all("<", "&lt;")
    return out
}
pub fn wrap(title: string) -> string {
    var t = esc(title)
    return "<y>" + t + "</y>"
}
pub fn interp(title: string) -> string {
    var t = esc(title)
    return "w ${t} w"
}
pub fn loopy(qs: string) -> string {
    var pairs = qs.split("&")
    for pair in pairs {
        if pair.contains("=") { return pair }
    }
    return ""
}
pub fn voidy(s: string) {
    if s.len() == 0 { return }
    println(s)
}
pub fn path(s: string) -> string { return s + "/p" }
pub fn outer(s: string) -> string { return path(s) }
EOF
ln -sf ../src/m.wyn "$TMP/proj/tests/m.wyn"
cat > "$TMP/proj/tests/test_m.wyn" <<'EOF'
import m
fn main() {
    Test.init("module-codegen")
    Test.assert_eq_str(m.outer("q"), "q/p", "M1: sibling call to fn named path")
    Test.assert_eq_str(m.loopy("a=1&b"), "a=1", "M2: for-in loop var in module fn")
    Test.assert_eq_str(m.wrap("A<B"), "<y>A&lt;B</y>", "M3: concat of sibling-call result")
    Test.assert_eq_str(m.interp("A<B"), "w A&lt;B w", "M3: interp of sibling-call result")
    m.voidy("ok")
    Test.assert(1 == 1, "M4: void module fn with early return compiled")
    Test.summary()
}
EOF

cd "$TMP/proj"
out=$(perl -e 'alarm 90; exec @ARGV' "$WYN_ABS" test 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "1 passed, 0 failed"; then
  ok "module-codegen M1-M4 all green"
else
  bad "module test: code=$code"
  echo "$out" | tail -8
fi

echo ""; echo "module-codegen: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
