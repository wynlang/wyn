#!/bin/bash
# Bug batch 2 (2026-07-18, from the syntax audit + bug sweep):
#  1. statement-position `mut` used to HANG the parser forever
#  2. multi-arg / zero-arg println used to ICE (checker passed it)
#  3. Option == None / != none used to ICE or give an unhelpful error
#  4. println(await_all(futs)) used to ICE (await_all was typed int)
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
# portable timeout: perl alarm
wyn_to(){ perl -e 'alarm 10; exec @ARGV' "$WYN" "$@" 2>&1; }

# 1. mut statement: clean error, NO hang (the alarm is the real assertion)
printf 'fn main() {\n    mut m = 1\n    println(m)\n}\n' > "$TMP/m.wyn"
out=$(wyn_to check "$TMP/m.wyn"); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "only valid on function parameters"; then
  ok "statement mut: clean error, no hang"
else bad "statement mut: code=$code [$out]"; fi

# 2. println multi-arg + zero-arg run like print
printf 'fn main() {\n    println("a", 1, true)\n    println()\n    println("b")\n}\n' > "$TMP/p.wyn"
out=$("$WYN" build "$TMP/p.wyn" >/dev/null 2>&1 && "$TMP/p" 2>&1; rm -f "$TMP/p" "$TMP/p.wyn.c")
if [ "$out" = "a 1 true

b" ]; then ok "println multi/zero-arg"; else bad "println multi-arg: [$out]"; fi

# 3. Option == None → targeted .is_none() suggestion at CHECK time
printf 'fn f(x: int) -> int? {\n    if x > 0 { return x }\n    return none\n}\nfn main() {\n    var r = f(-1)\n    if r == None { println(1) }\n}\n' > "$TMP/n.wyn"
out=$(wyn_to check "$TMP/n.wyn"); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "is_none"; then
  ok "Option == None: suggests .is_none()"
else bad "Option == None: code=$code [$out]"; fi

# 4. println(await_all(futs)) prints the result array
printf 'fn w(n: int) -> int { return n * n }\nfn main() {\n    var futs = []\n    for i in 1..4 { futs.push(spawn w(i)) }\n    println(await_all(futs))\n}\n' > "$TMP/a.wyn"
out=$("$WYN" build "$TMP/a.wyn" >/dev/null 2>&1 && perl -e 'alarm 10; exec @ARGV' "$TMP/a" 2>&1; rm -f "$TMP/a" "$TMP/a.wyn.c")
if [ "$out" = "[1, 4, 9]" ]; then ok "println(await_all)"; else bad "println(await_all): [$out]"; fi

echo ""; echo "bug-batch-2: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
