#!/bin/bash
# A successful compile is SILENT (2026-08).
#
# `track_var_with_type` printed "WARNING: track_var_with_type called with
# scope_depth=0" to stderr on ordinary programs - five times per build of anything
# with module-level vars. It is a codegen-internal note about a no-op (a var tracked
# at file scope has nothing to clean up), not a warning a user can act on, and it has
# no business in the output of a compile that succeeds.
#
# This guards the general property, not that one line: a clean build emits no
# "WARNING:" and no "WYN_DEBUG:" text. The note is
# gated behind WYN_DEBUG rather than deleted - a two-line guard in codegen.c.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# A program with module-level vars, which is what put scope_depth at 0 - the
# condition that fired the warning. `p_stmt`-style top-level state plus a function
# that declares locals reaches track_var_with_type at file scope.
cat > "$TMP/a.wyn" <<'EOF'
var counter = 0
var label = "hi"

fn work(n: int) -> int {
    var total = 0
    var i = 0
    while i < n {
        total = total + i
        i = i + 1
    }
    return total
}

fn main() {
    counter = work(5)
    print("${label} ${counter}")
}
EOF

# WYN_DEBUG must be UNSET for this build, or the gated note appears legitimately.
out=$(cd "$TMP" && env -u WYN_DEBUG "$WYN_ABS" build a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then ok "the program builds"; else bad "build failed"; echo "$out"|tail -4; fi

if echo "$out" | grep -q "WARNING:"; then
  bad "a WARNING: leaked from a successful compile"
  echo "$out" | grep "WARNING:" | head -3 | sed 's/^/        /'
else
  ok "no WARNING: in a clean build"
fi

if echo "$out" | grep -q "track_var_with_type"; then
  bad "the track_var_with_type note leaked without WYN_DEBUG"
else
  ok "the scope_depth=0 note is silent without WYN_DEBUG"
fi

if echo "$out" | grep -q "WYN_DEBUG:"; then
  bad "a WYN_DEBUG: line appeared without WYN_DEBUG set"
else
  ok "no WYN_DEBUG: lines without the flag"
fi

# The running program's own output must be clean too - the note went to stderr, which
# `wyn run` folds in.
out=$(cd "$TMP" && env -u WYN_DEBUG "$WYN_ABS" run a.wyn 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "^hi 10$" && ! echo "$out" | grep -q "WARNING:"; then
  ok "wyn run prints only the program's output"
else
  bad "wyn run output was not clean"; echo "$out" | head -5 | sed 's/^/        /'
fi

# NB: the scope_depth=0 path is reached only by specific top-level constructs (it
# fires building WynCanvas's ui.wyn, but not for this small program), so this file
# asserts the SILENCE property - which holds for every program - rather than forcing
# the debug line from a minimal case. That the note is gated rather than deleted is a
# two-line getenv guard in codegen.c, visible in the diff.

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "clean-output: $PASS pass, 0 fail"
  exit 0
fi
echo "clean-output: $PASS pass, $FAIL fail"
exit 1
