#!/bin/bash
# A missing key on `m[k]` used to silently return 0/"" (indistinguishable from a
# stored 0). It must now PANIC like array OOB - fatal by default, naming the key
# and the source location. `.get`/`.has` keep the lenient path (checked in the
# positive regression map_missing_key_ok.wyn).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# A missing-key read must exit non-zero (rc in [1,127]) and print a panic that
# names the missing key.
cat > "$TMP/miss.wyn" <<'WYN'
fn main() {
    var m = {"a": 0, "b": 5}
    var k = "zzz"
    var v = m[k]
    println("${v}")
}
WYN
out=$(perl -e 'alarm(30); exec @ARGV' -- "$WYN" run "$TMP/miss.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -le 127 ] && echo "$out" | grep -q 'missing map key' && echo "$out" | grep -q 'zzz'; then
    ok "missing map key m[\"zzz\"] panics naming the key"
else
    bad "missing map key panic (rc=$rc) [$out]"
fi

echo ""; echo "map-missing-key: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
