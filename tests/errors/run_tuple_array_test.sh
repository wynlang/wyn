#!/bin/bash
# #415: an array of tuples is rejected at CHECK time, not left to fail in the generated C.
#
# WHAT THIS REPLACES. `wyn check` exited 0 and `wyn build` then died inside the generated C:
#
#   rows = [("ada", 120), ("alan", 240)]
#   for name, amount in rows { print("${name}=${amount}") }
#
#   error: operand of type 'struct (unnamed struct at ...)' where arithmetic or pointer
#          type is required
#
# - an error pointing at generated code instead of at the author's line, after check said
# the program was fine. That is the check-passes / build-fails class the soundness work
# exists to close.
#
# WHY THE RULE IS AT THE ARRAY LITERAL AND NOT IN `for`. The issue reported the `for`
# destructuring, but that is not the boundary. Measured against the previous build, of the
# seven things you can do with `rows = [("ada", 120)]`, SEVEN fail:
#
#   rows.len()              internal codegen error
#   rows[0]                 internal codegen error
#   rows.push(("x", 1))     internal codegen error
#   for r in rows           internal codegen error
#   for a, b in rows        internal codegen error
#   declaring it and NEVER USING IT AT ALL   internal codegen error
#   a, b = rows[0]          (already had its own error)
#
# The type system has no tuple type - there is no TYPE_TUPLE anywhere - so the array's
# element type degrades to the int default while codegen emits the real anonymous struct.
# Nothing written with one can compile, so the literal is the defect and one rule there
# replaces an arm per use site. The "declare and never use" arm below is the one that
# proves this: no use site is involved in it at all.
#
# THE MESSAGE IS ASSERTED, not just the exit code, and specifically the part about
# `for a, b in xs` meaning INDEX and VALUE in Wyn rather than Python's element unpacking
# (`for i, v in [10, 20]` gives `0:10` then `1:20`). A Python reader's model is wrong in a
# way the old C error could never convey, and would still be wrong if the message only said
# "not supported". The suggested struct replacement is asserted to actually RUN, so the
# help text cannot drift into advice that does not work.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <source> <message-substring>
# Asserts check AND run AND build all fail, and that no internal codegen error leaks.
reject(){
  d=$(mktemp -d); printf '%b\n' "$2" > "$d/p.wyn"
  out=$(TMPDIR="$d" perl -e 'alarm(30); exec @ARGV' -- "$WYNABS" check "$d/p.wyn" 2>&1); ccode=$?
  out=$(printf '%s' "$out" | sed 's/\x1b\[[0-9;]*m//g')
  TMPDIR="$d" perl -e 'alarm(60); exec @ARGV' -- "$WYNABS" run "$d/p.wyn" >"$d/r.out" 2>&1; rcode=$?
  TMPDIR="$d" perl -e 'alarm(60); exec @ARGV' -- "$WYNABS" build "$d/p.wyn" >/dev/null 2>&1; bcode=$?
  rm -rf "$d"
  if [ $ccode -eq 0 ]; then bad "reject: $1 - wyn check EXITED 0"; return; fi
  if [ $rcode -eq 0 ]; then bad "reject: $1 - wyn run EXITED 0"; return; fi
  if [ $bcode -eq 0 ]; then bad "reject: $1 - wyn build EXITED 0"; return; fi
  if ! echo "$out" | grep -q "$3"; then
    bad "reject: $1 - message missing [$3] got [$(echo "$out" | tr '\n' '|' | cut -c1-120)]"; return
  fi
  if grep -q "internal codegen error" "$d/r.out" 2>/dev/null; then
    bad "reject: $1 - leaked 'internal codegen error'"; return
  fi
  ok "reject: $1"
}

# accept <label> <source> <expected-stdout>
accept(){
  d=$(mktemp -d); printf '%b\n' "$2" > "$d/p.wyn"
  got=$(TMPDIR="$d" perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d/p.wyn" 2>&1)
  got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
  rm -rf "$d"
  if [ "$got" = "$3" ]; then ok "accept: $1"
  else bad "accept: $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-110)] want [$(echo "$3" | tr '\n' '|')]"; fi
}

echo "=== #415: an array of tuples is a check-time error ==="

reject "the reported repro: for name, amount in rows" \
  'fn main() {\n    rows = [("ada", 120), ("alan", 240)]\n    for name, amount in rows { print("${name}=${amount}") }\n}' \
  "an array of tuples is not supported"

# The arm that shows the literal is the defect: no use site at all.
reject "declared and never used" \
  'fn main() {\n    rows = [("ada", 120)]\n    print("ok")\n}' \
  "an array of tuples is not supported"

reject "single-variable for over a tuple array" \
  'fn main() {\n    rows = [("ada", 120)]\n    for r in rows { print("x") }\n}' \
  "an array of tuples is not supported"

reject ".len() on a tuple array" \
  'fn main() {\n    rows = [("ada", 120)]\n    print("${rows.len()}")\n}' \
  "an array of tuples is not supported"

reject "indexing a tuple array" \
  'fn main() {\n    rows = [("ada", 120)]\n    r = rows[0]\n    print("ok")\n}' \
  "an array of tuples is not supported"

reject "a one-element tuple array" \
  'fn main() {\n    rows = [("only", 1)]\n    print("ok")\n}' \
  "an array of tuples is not supported"

reject "a 3-field tuple element" \
  'fn main() {\n    rows = [("a", 1, true)]\n    print("ok")\n}' \
  "an array of tuples is not supported"

# --------------------------------------------------- the message must carry its guidance
reject "the message explains Wyn's two-variable for" \
  'fn main() {\n    rows = [("ada", 120)]\n    for a, b in rows { print("x") }\n}' \
  "INDEX and VALUE in Wyn"

# The reported LINE is asserted, because a tuple node carries no line of its own: the
# first draft printed "Error at line 0" and show_source_line rendered nothing. Without
# this arm the fallback chain is untested and could silently rot back to 0.
reject "the error names the real source line, not line 0" \
  'fn main() {\n    print("filler")\n    rows = [("ada", 120)]\n    print("ok")\n}' \
  "Error at line 3: an array of tuples"

reject "the message names the struct alternative" \
  'fn main() {\n    rows = [("ada", 120)]\n    print("ok")\n}' \
  "struct Row { name: string, amount: int }"

# =========================================================== ACCEPT ARMS
echo "--- accept: the alternative the message recommends must actually run"

accept "the suggested struct replacement works" \
  'struct Row { name: string, amount: int }\nfn main() {\n    rows = [Row { name: "ada", amount: 120 }, Row { name: "alan", amount: 240 }]\n    for r in rows { print("${r.name}=${r.amount}") }\n}' \
  'ada=120
alan=240'

accept "two parallel arrays work" \
  'fn main() {\n    names = ["ada", "alan"]\n    amounts = [120, 240]\n    for i, n in names { print("${n}=${amounts[i]}") }\n}' \
  'ada=120
alan=240'

echo "--- accept: tuples themselves are untouched - it is arrays OF them that are refused"

accept "bare tuple unpacking still works" \
  'fn main() {\n    a, b = 1, 2\n    print("${a} ${b}")\n}' \
  '1 2'

# Field access on a tuple VARIABLE, which is what actually works. Destructuring one
# (`a, b = t`) does NOT - it answers "multi-assignment has 2 targets but 1 values" - and
# that is PRE-EXISTING, identical on the build before this change, so it is asserted as-is
# here rather than quietly wished away. Only the literal form `a, b = 1, 2` destructures.
accept "tuple field access on a variable still works" \
  'fn main() {\n    t = ("ada", 120)\n    print("${t.0}=${t.1}")\n}' \
  'ada=120'

reject "destructuring a tuple VARIABLE is still refused (pre-existing)" \
  'fn main() {\n    t = ("ada", 120)\n    a, b = t\n    print("${a}=${b}")\n}' \
  "multi-assignment has 2 targets but 1 values"

echo "--- accept: ordinary arrays are untouched"

accept "array of ints" \
  'fn main() {\n    xs = [1, 2, 3]\n    print("${xs.len()}")\n}' \
  '3'

accept "array of strings" \
  'fn main() {\n    xs = ["a", "b"]\n    print("${xs.join("-")}")\n}' \
  'a-b'

accept "array of structs" \
  'struct P { x: int }\nfn main() {\n    xs = [P { x: 1 }, P { x: 2 }]\n    print("${xs[1].x}")\n}' \
  '2'

accept "nested array of arrays" \
  'fn main() {\n    xs = [[1, 2], [3]]\n    print("${xs.len()}")\n}' \
  '2'

accept "the two-variable for on an ordinary array is index+value" \
  'fn main() {\n    for i, v in [10, 20] { print("${i}:${v}") }\n}' \
  '0:10
1:20'

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
