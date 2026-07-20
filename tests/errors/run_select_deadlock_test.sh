#!/bin/bash
# select with no ready arm and NOTHING in flight used to spin forever —
# a silent hang on a plain logic error. The runtime now detects the
# deadlock (main thread, zero in-flight tasks, no ready/closed channel)
# and exits with an error instead.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. Two empty channels, no producers: must ERROR (exit 1) within the alarm,
#    not hang (alarm → rc >= 128).
printf 'fn main() {\n    a = channel(2)\n    b = channel(2)\n    select {\n        v = a.recv() => println(v)\n        w = b.recv() => println(w)\n    }\n}\n' > "$TMP/a.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/a.wyn" 2>&1); rc=$?
if [ $rc -ge 1 ] && [ $rc -lt 128 ] && echo "$out" | grep -q "deadlock"; then ok "empty select errors, no hang"
else bad "empty select (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 2. Closed channels: select returns -1, no arm runs, program continues.
printf 'fn main() {\n    a = channel(2)\n    b = channel(2)\n    a.close()\n    b.close()\n    select {\n        v = a.recv() => println(v)\n        w = b.recv() => println(w)\n    }\n    println("after")\n}\n' > "$TMP/b.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/b.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "after"; then ok "all-closed select falls through"
else bad "closed select (rc=$rc) [$(echo "$out" | tail -1)]"; fi

echo ""; echo "select-deadlock: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
