#!/bin/bash
# #425: an argument of the wrong TYPE is a check-time error.
#
# WHAT THIS REPLACES. `"42".pad_left("a", "a")` compiled, ran, EXITED 0 and printed a
# ~44MB garbage string. `pad_left` lowers to
# `string_pad_left(const char* s, int width, const char* pad)`, and nothing between the
# Wyn source and that call checked that argument 1 is an int - so a POINTER arrived as the
# width and was used as a length. A wrong type at a C boundary is not converted, it is
# REINTERPRETED, which is why this class produces garbage values rather than errors.
#
# #393's param_types column is what makes a rule possible here, and its reachability gate
# provably CANNOT catch this class: mutating pad_left's row to "string, string" still
# compiles, because reachability proves a row is callable, not that its declared types are
# the lowering's types. This is that column's first real reader.
#
# THE RULE IS DELIBERATELY COARSE - it compares CATEGORIES (numeric / string / bool /
# array / map / set / function) and fires only when both sides are known and the
# categories are disjoint. int and float are ONE category because Wyn coerces between them
# freely. The column's declared types were never verified against the lowerings, so an
# exact-match rule would reject working programs on the strength of data nobody had read.
#
# THE LOAD-BEARING ASSERTIONS ARE THE ACCEPT ARMS, not the rejections. A rule of this shape
# is trivial to make too strict, and too strict is worse than the bug. Two groups of them
# matter most:
#
#   1. CONTAINER RECEIVERS MUST NOT BE JUDGED AT ALL. method_signatures is keyed on the
#      receiver type as a STRING ("array", "map", "set"), so it cannot see an element or
#      value type - which makes the declared argument type of every element-taking
#      collection method a PLACEHOLDER, not a contract. `array.push` is written "int", and
#      `xs.push("s")` on a [string] array is correct code. A first draft judged them and
#      rejected NINE corpus files; each one was the row being a placeholder rather than the
#      program being wrong. Those exact shapes are pinned below.
#
#   2. int/float interchange must stay legal in both directions.
#
# Verified beyond this gate by a differential `wyn check` sweep of every .wyn file in the
# workspace: zero newly rejected, zero newly accepted.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <source> <message-substring>
# Asserts `wyn check` AND `wyn run` both fail, and that the program did not reach its
# sentinel - the original defect ran to completion and printed a garbage value at exit 0.
reject(){
  printf '%b\n' "$2" > "$TMP/r.wyn"
  out=$(perl -e 'alarm(30); exec @ARGV' -- "$WYNABS" check "$TMP/r.wyn" 2>&1); ccode=$?
  out=$(printf '%s' "$out" | sed 's/\x1b\[[0-9;]*m//g')
  perl -e 'alarm(60); exec @ARGV' -- "$WYNABS" run "$TMP/r.wyn" >"$TMP/run.out" 2>&1; rcode=$?
  if [ $ccode -eq 0 ]; then bad "reject: $1 - wyn check EXITED 0"; return; fi
  if [ $rcode -eq 0 ]; then bad "reject: $1 - wyn run EXITED 0"; return; fi
  if ! echo "$out" | grep -q "$3"; then
    bad "reject: $1 - message missing [$3] got [$(echo "$out" | tr '\n' '|' | cut -c1-140)]"; return
  fi
  if grep -q "SENTINEL_REACHED" "$TMP/run.out"; then
    bad "reject: $1 - program RAN TO COMPLETION"; return
  fi
  ok "reject: $1"
}

# accept <label> <source> <expected-stdout>  -- must still compile AND give the right value
accept(){
  printf '%b\n' "$2" > "$TMP/a.wyn"
  got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$TMP/a.wyn" 2>&1)
  got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
  if [ "$got" = "$3" ]; then ok "accept: $1"
  else bad "accept: $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-110)] want [$(echo "$3" | tr '\n' '|')]"; fi
}

echo "=== #425: an argument of the wrong type is a check-time error ==="

# ---------------------------------------------------------------- the reported defect
reject "the reported repro: pad_left(string, string)" \
  'fn main() {\n    s = "42".pad_left("a", "a")\n    print("SENTINEL_REACHED len=${s.len()}")\n}' \
  "'pad_left()' argument 1 must be int, not string"

reject "pad_right, the same shape on its sibling" \
  'fn main() {\n    s = "42".pad_right("a", "a")\n    print("SENTINEL_REACHED len=${s.len()}")\n}' \
  "argument 1 must be int, not string"

reject "a string where an int index is read" \
  'fn main() {\n    s = "hello".char_at("x")\n    print("SENTINEL_REACHED ${s}")\n}' \
  "argument 1 must be int"

reject "a bool where an int is read" \
  'fn main() {\n    s = "42".pad_left(true, "0")\n    print("SENTINEL_REACHED ${s}")\n}' \
  "argument 1 must be int, not bool"

reject "an int where a string is read" \
  'fn main() {\n    s = "a,b".split(5)\n    print("SENTINEL_REACHED ${s.len()}")\n}' \
  "must be string, not int"

# ------------------------------------------------ the diagnostic must name the position
reject "the message names the ARGUMENT NUMBER, not just the method" \
  'fn main() {\n    s = "42".pad_left(5, 9)\n    print("SENTINEL_REACHED ${s}")\n}' \
  "argument 2 must be string, not int"

# ================================================================== ACCEPT ARMS
echo "--- accept: correct usage, the control for every rejection above"

accept "pad_left used correctly" \
  'fn main() { print("[${"42".pad_left(5, "0")}]") }' \
  '[00042]'

accept "pad_right used correctly" \
  'fn main() { print("[${"42".pad_right(5, "0")}]") }' \
  '[42000]'

accept "char_at used correctly" \
  'fn main() { print("${"hello".char_at(1)}") }' \
  'e'

accept "split used correctly" \
  'fn main() { print("${"a,b".split(",").len()}") }' \
  '2'

echo "--- accept: int/float are ONE category, both directions"

accept "a float where int is declared" \
  'fn main() { print("${"42".pad_left(5.0, "0")}") }' \
  '00042'

accept "an int where float is declared (math)" \
  'fn main() { print("${(2.0).pow(3)}") }' \
  '8.0'

echo "--- accept: CONTAINER receivers are not judged (their rows are placeholders)"
# These four are the exact shapes a first draft rejected across nine corpus files.

accept "xs.push(string) on a [string] array" \
  'fn main() {\n    var xs: [string] = []\n    xs.push("a")\n    xs.push("b")\n    print("${xs.len()} ${xs[0]}")\n}' \
  '2 a'

accept "array.contains(string) on a string array" \
  'fn main() {\n    xs = ["a", "b"]\n    print("${xs.contains("a")} ${xs.contains("z")}")\n}' \
  'true false'

accept "map.insert(string, string) on a string-valued map" \
  'fn main() {\n    m = {"a": "x"}\n    m.insert("k", "v")\n    print("${m.get("k")} ${m.len()}")\n}' \
  'v 2'

accept "set.add(int) on an int set" \
  'fn main() {\n    s = {:1}\n    s.add(2)\n    print("${s.len()} ${s.contains(2)}")\n}' \
  '2 true'

accept "array.join(string) - a NON-element arg on a container" \
  'fn main() {\n    xs = ["a", "b"]\n    print("${xs.join("-")}")\n}' \
  'a-b'

echo "--- accept: a lambda argument, whose declared spec is fn(...) - not judged"

accept "array.map with a lambda" \
  'fn main() {\n    xs = [1, 2, 3]\n    print("${xs.map((n) => n * 2).len()}")\n}' \
  '3'

accept "array.filter with a lambda" \
  'fn main() {\n    xs = [1, 2, 3]\n    print("${xs.filter((n) => n > 1).len()}")\n}' \
  '2'

echo "--- accept: a STRUCT / Option / Result argument is never judged (category 0)"

accept "a struct argument reaches a method unjudged" \
  'struct P { x: int }\nfn main() {\n    var xs: [P] = []\n    xs.push(P { x: 1 })\n    print("${xs.len()}")\n}' \
  '1'

echo "--- accept: a variable (not a literal) carries its type into the rule"

accept "an int VARIABLE as the width" \
  'fn main() {\n    w = 5\n    print("[${"42".pad_left(w, "0")}]")\n}' \
  '[00042]'

reject "a string VARIABLE as the width is still caught" \
  'fn main() {\n    w = "x"\n    s = "42".pad_left(w, "0")\n    print("SENTINEL_REACHED ${s}")\n}' \
  "argument 1 must be int, not string"

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
