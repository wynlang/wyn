#!/bin/bash
# CLI/DX correctness: exit codes, false-green elimination, version integrity.
# Found by the DX audit (2026-07-20).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. `wyn check <dir>` must error, not print "no errors".
mkdir -p "$TMP/proj" && printf 'fn main() { broken }\n' > "$TMP/proj/bad.wyn"
out=$("$WYN" check "$TMP/proj" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "directory"; then ok "check <dir> errors"; else bad "check dir: rc=$rc [$out]"; fi

# 2. `wyn run <dir>` (non-project dir) must exit nonzero.
mkdir -p "$TMP/empty"
out=$("$WYN" run "$TMP/empty" 2>&1); rc=$?
[ $rc -ne 0 ] && ok "run <dir> exits nonzero" || bad "run dir: rc=$rc [$out]"

# 3. Version must NOT be the old hardcoded 1.10.0 fallback, from any cwd.
out=$(cd "$TMP" && "$WYN" version 2>&1)
if echo "$out" | grep -q "1.10.0"; then bad "version stuck at 1.10.0 fallback"; else ok "version correct from foreign cwd"; fi

# 4. Cached `wyn run` preserves the program's exit code.
printf 'fn main() {\n    System.exit(3)\n}\n' > "$TMP/ec.wyn"
"$WYN" run "$TMP/ec.wyn" >/dev/null 2>&1; first=$?
"$WYN" run "$TMP/ec.wyn" >/dev/null 2>&1; cached=$?
if [ "$first" = "3" ] && [ "$cached" = "3" ]; then ok "cached run keeps exit code"; else bad "exit codes: fresh=$first cached=$cached"; fi

# 5. `wyn test` with a filter matching nothing must exit nonzero.
mkdir -p "$TMP/tp/tests" && cd "$TMP/tp"
printf 'test "basic" {\n    assert_eq(1 + 1, 2)\n}\n' > tests/test_main.wyn
out=$("$WYN" test zzz_no_such_test 2>&1); rc=$?
[ $rc -ne 0 ] && ok "zero-match test filter fails" || bad "zero-match filter: rc=$rc [$out]"
cd - >/dev/null

# 6. Malformed wyn.toml line warns (once) instead of silent ignore.
# (wyn.toml is only parsed when the source needs it — extern fn triggers it.)
mkdir -p "$TMP/mt" && cd "$TMP/mt"
printf '[package]\nname = "t"\nthis is !! not toml garbage\n[ffi]\nlibs = "m"\n' > wyn.toml
printf 'extern fn sqrt(x: float) -> float;\nfn main() {\n    println(sqrt(4.0).to_string())\n}\n' > m.wyn
out=$("$WYN" run m.wyn 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -qi "warning.*wyn.toml"; then ok "malformed wyn.toml warns"; else bad "toml warn: rc=$rc [$(echo "$out" | head -2)]"; fi
cd - >/dev/null

echo ""; echo "cli-dx: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
