#!/bin/bash
# pub visibility enforcement (2026-07): calling a module fn that is not
# marked `pub` (or `export`) from outside its module is a check-time error.
# Same-module calls, main-file-local fns, stdlib namespaces, and selective
# imports of pub fns are unaffected.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

cat > "$TMP/dep.wyn" <<'EOF'
fn whisper() {
    println("secret")
}

pub fn shout() {
    println("HELLO")
}
EOF

cat > "$TMP/expdep.wyn" <<'EOF'
export fn exported_fn() -> int {
    return 7
}
EOF

# 1. Private dot call rejected, with the exact actionable message
cat > "$TMP/a.wyn" <<'EOF'
import dep

fn main() {
    dep.whisper()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check a.wyn 2>&1); code=$?
if [ $code -ne 0 ] &&
   echo "$out" | grep -q "Error at line 4: function 'whisper' in module 'dep' is private" &&
   echo "$out" | grep -q "Add 'pub' to 'fn whisper' in dep.wyn to export it."; then
  ok "private dot call rejected with actionable message"
else
  bad "private dot call: code=$code"; echo "$out" | head -4
fi

# 2. Private qualified (::) call rejected too
cat > "$TMP/b.wyn" <<'EOF'
import dep

fn main() {
    dep::whisper()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check b.wyn 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "function 'whisper' in module 'dep' is private"; then
  ok "private :: call rejected"
else
  bad "private :: call: code=$code"; echo "$out" | head -4
fi

# 3. pub call allowed and runs
cat > "$TMP/c.wyn" <<'EOF'
import dep

fn main() {
    dep.shout()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "HELLO"; then
  ok "pub call allowed"
else
  bad "pub call: code=$code"; echo "$out" | head -4
fi

# 4. Same-module private call unaffected (pub fn calls its private sibling)
cat > "$TMP/dep2.wyn" <<'EOF'
fn helper() -> int {
    return 41
}
pub fn answer() -> int {
    return helper() + 1
}
EOF
cat > "$TMP/d.wyn" <<'EOF'
import dep2

fn main() {
    println(dep2.answer())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "42"; then
  ok "same-module private call ok"
else
  bad "same-module call: code=$code"; echo "$out" | head -4
fi

# 5. export fn is public (export and pub both export)
cat > "$TMP/e.wyn" <<'EOF'
import expdep

fn main() {
    println(expdep.exported_fn())
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run e.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "7"; then
  ok "export fn callable cross-module"
else
  bad "export fn: code=$code"; echo "$out" | head -4
fi

# 6. Selective import of a private fn rejected
cat > "$TMP/f.wyn" <<'EOF'
import { whisper } from dep

fn main() {
    whisper()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check f.wyn 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "function 'whisper' in module 'dep' is private"; then
  ok "selective import of private fn rejected"
else
  bad "selective private import: code=$code"; echo "$out" | head -4
fi

# 7. Selective import of a pub fn still works
cat > "$TMP/g.wyn" <<'EOF'
import { shout } from dep

fn main() {
    shout()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run g.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "HELLO"; then
  ok "selective import of pub fn ok"
else
  bad "selective pub import: code=$code"; echo "$out" | head -4
fi

# 8. Aliased import (`import dep as d`) enforces too
cat > "$TMP/h.wyn" <<'EOF'
import dep as d

fn main() {
    d.whisper()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check h.wyn 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "function 'whisper' in module 'dep' is private"; then
  ok "aliased private call rejected"
else
  bad "aliased private call: code=$code"; echo "$out" | head -4
fi

# 9. Module-to-module: a module cannot call another module's private fn
cat > "$TMP/mid.wyn" <<'EOF'
import dep

pub fn relay() {
    dep.whisper()
}
EOF
cat > "$TMP/i.wyn" <<'EOF'
import mid

fn main() {
    mid.relay()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" check i.wyn 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "function 'whisper' in module 'dep' is private"; then
  ok "module-to-module private call rejected"
else
  bad "module-to-module: code=$code"; echo "$out" | head -4
fi

# 10. Stdlib namespaces unaffected
cat > "$TMP/j.wyn" <<'EOF'
fn main() {
    println(Math.abs(0 - 5))
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run j.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "5"; then
  ok "stdlib namespace calls unaffected"
else
  bad "stdlib call: code=$code"; echo "$out" | head -4
fi

# 11. A local variable shadowing a module name is not treated as a module
cat > "$TMP/k.wyn" <<'EOF'
import dep

fn main() {
    var s = "abc"
    println(s.upper())
    dep.shout()
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run k.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "ABC"; then
  ok "local var method calls unaffected"
else
  bad "shadowing: code=$code"; echo "$out" | head -4
fi

echo ""; echo "pub-visibility: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
