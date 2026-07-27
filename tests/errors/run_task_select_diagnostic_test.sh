#!/bin/bash
# Task.select (no arity suffix) must be rejected AT CHECK TIME with a
# "did you mean" hint naming the real functions. Regression guard: it used to
# pass `wyn check` clean and die at C-compile with a bare
# `call to undeclared function 'Task_select'`. Only Task.select_2 /
# Task.select_3 exist. (2026-07)
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. Task.select (unsuffixed) → clean check error naming select_2/select_3.
printf 'fn main() {\n  var a = Task.channel(4)\n  var b = Task.channel(4)\n  Task.send(a, 1)\n  var r = Task.select(a, b)\n  print("${r}")\n}\n' > "$TMP/sel.wyn"
out=$("$WYN" check "$TMP/sel.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "Task.select" && echo "$out" | grep -q "select_2" && echo "$out" | grep -q "select_3"; then
  ok "Task.select rejected at check time with did-you-mean"
else bad "Task.select check error: code=$code [$out]"; fi

# 2. Must NOT leak the raw C error at build time.
out=$("$WYN" build "$TMP/sel.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && ! echo "$out" | grep -q "undeclared function"; then
  ok "Task.select does not leak raw C error at build"
else bad "Task.select build leaked C error: code=$code [$out]"; fi

# 3. Positive: Task.select_2 still checks AND runs correctly. select returns the
#    INDEX of the ready channel; a (index 0) has the value → prints 0.
printf 'fn main() {\n  var a = Task.channel(4)\n  var b = Task.channel(4)\n  Task.send(a, 42)\n  var r = Task.select_2(a, b)\n  print("${r}")\n}\n' > "$TMP/ok2.wyn"
out=$("$WYN" check "$TMP/ok2.wyn" 2>&1); code=$?
if [ $code -eq 0 ]; then ok "Task.select_2 checks clean"; else bad "Task.select_2 check: code=$code [$out]"; fi
out=$("$WYN" run "$TMP/ok2.wyn" 2>&1; rm -f "$TMP/ok2.wyn.c")
if echo "$out" | grep -qx "0"; then ok "Task.select_2 runs (ready index 0)"; else bad "Task.select_2 run: [$out]"; fi

# 4. Positive: Task.select_3 still checks AND runs. b (index 1) is ready → 1.
printf 'fn main() {\n  var a = Task.channel(4)\n  var b = Task.channel(4)\n  var c = Task.channel(4)\n  Task.send(b, 7)\n  var r = Task.select_3(a, b, c)\n  print("${r}")\n}\n' > "$TMP/ok3.wyn"
out=$("$WYN" check "$TMP/ok3.wyn" 2>&1); code=$?
if [ $code -eq 0 ]; then ok "Task.select_3 checks clean"; else bad "Task.select_3 check: code=$code [$out]"; fi
out=$("$WYN" run "$TMP/ok3.wyn" 2>&1; rm -f "$TMP/ok3.wyn.c")
if echo "$out" | grep -qx "1"; then ok "Task.select_3 runs (ready index 1)"; else bad "Task.select_3 run: [$out]"; fi

# 5. Other valid Task methods still check clean (no over-tightening).
printf 'fn main() {\n  var a = Task.channel(4)\n  Task.send(a, 1)\n  var r = Task.recv(a)\n  Task.close(a)\n  print("${r}")\n}\n' > "$TMP/valid.wyn"
out=$("$WYN" check "$TMP/valid.wyn" 2>&1); code=$?
if [ $code -eq 0 ]; then ok "valid Task methods (channel/send/recv/close) unaffected"; else bad "valid Task methods: code=$code [$out]"; fi

echo ""; echo "task-select-diagnostic: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
