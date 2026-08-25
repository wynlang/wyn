#!/bin/bash
# `for x in <string>` must be a CHECK-TIME error, not a build-time ICE.
#
# Codegen's for-in fallthrough assigns the iterable to a `WynArray` unconditionally, so
# a string emitted invalid C. The program passed `wyn check` with "✓ no errors" and then
# died as a bare "internal codegen error" naming neither the problem nor a fix - the
# exact soundness rule v1.21 is built around: what checks must build.
#
# Found by writing cli-tools/depgraph, whose loop was `for f in File::walk_dir(dir)`.
# `File::walk_dir` returns ONE newline-joined string rather than an array (unlike
# `File::list_dir`), so that loop needed `.split("\n")` all along - and the ICE said
# nothing about it.
#
# Rejected rather than given a meaning: iterating a string could yield characters,
# bytes, or grapheme clusters, and picking one silently would smuggle a language
# decision in as a bug fix.
#
# The POSITIVE half - that every suggested form works and other iterables are
# unaffected - is tests/regression/test_for_in_string_rejected.wyn. This file covers
# what a check-time error makes untestable there: the program never runs, so it can
# print no EXPECT line.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. A string variable. `wyn check` must REJECT it (rc != 0) and say so in words.
printf 'fn main() -> int {\n    var s = "ab"\n    for c in s { println(c) }\n    return 0\n}\n' > "$TMP/var.wyn"
out=$("$WYN" check "$TMP/var.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "cannot iterate a string directly"; then
  ok "a string variable is rejected at check time"
else bad "string var not rejected (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 2. The message must be ACTIONABLE - it has to name a form that works. A rejection
#    that only says "no" moves the user from a confusing ICE to a confusing error.
if echo "$out" | grep -q 'split' && echo "$out" | grep -q '0\.\.'; then
  ok "the error suggests .split(...) and an index loop"
else bad "error is not actionable [$out]"; fi

# 3. It must not have been merely deferred to the C compiler. Asserts the REAL message
#    is present as well as the ICE being absent: an absence-only assertion passes when
#    the guard is removed (the ICE text goes elsewhere), which a mutation run caught.
out2=$("$WYN" build "$TMP/var.wyn" 2>&1); rc2=$?
if [ $rc2 -ne 0 ] && echo "$out2" | grep -q "cannot iterate a string directly" \
   && ! echo "$out2" | grep -qi "internal codegen error"; then
  ok "build fails with the same real error, not an internal codegen error"
else bad "build ICE'd or succeeded (rc=$rc2) [$(echo "$out2" | tail -1)]"; fi

# 4. A string-returning CALL iterated directly - the reported shape. The check must key
#    off the expression's TYPE, not off the iterable being a bare identifier.
printf 'fn g() -> string { return "a" }\nfn main() -> int {\n    for c in g() { println(c) }\n    return 0\n}\n' > "$TMP/call.wyn"
out=$("$WYN" check "$TMP/call.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "cannot iterate a string directly"; then
  ok "a string-returning call is rejected too"
else bad "string-returning call not rejected (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 5. POSITIVE CONTROL. The valid iterables must still CHECK cleanly - the guard sits in
#    the same branch that types an array element, so an over-broad test would break
#    every ordinary for-in. If this fails, the fix is worse than the bug.
printf 'fn main() -> int {\n    xs = ["a"]\n    for x in xs { println(x) }\n    ns = [1]\n    for n in ns { println("${n}") }\n    var m = {"k": 1}\n    for k in m { println(k) }\n    for i in 0..2 { println("${i}") }\n    var s = "ab"\n    for c in s.split("") { println(c) }\n    return 0\n}\n' > "$TMP/ok.wyn"
out=$("$WYN" check "$TMP/ok.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "arrays, maps, ranges and .split() still check clean"
else bad "valid iterables broken (rc=$rc) [$(echo "$out" | tail -1)]"; fi

echo ""
echo "for-in-string: $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ] || exit 1
