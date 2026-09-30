#!/bin/bash
# #426: an unknown method on a map or a set is a CHECK-TIME error that FAILS THE BUILD.
#
# WHAT THIS REPLACES. codegen had a last-resort diagnostic that printed
# "Error: Unknown method '<m>' for type '<t>'" and then emitted NOTHING, which produced
# two different wrong outcomes decided only by how the call was used:
#
#   m.definitely_not_a_method(3)        the empty emission is a whole statement, so the
#                                      call VANISHED and the program compiled, ran to
#                                      completion and EXITED 0.
#   t = s.symmetric_difference(u)       the empty emission lands in a value position, so
#                                      the generated C was invalid and the build died as
#                                      "compilation failed (internal codegen error)" -
#                                      the compiler reporting its own bug for what is
#                                      ordinary user error.
#
# The first is the serious half and it is why this gate exists: an error message that does
# not fail the build means `wyn run` and `wyn build` cannot be trusted as gates, so a
# green test harness is not evidence that the code ran at all.
#
# THE LOAD-BEARING ASSERTIONS ARE THE POSITIVE CANARIES, not the rejections.
#
# A rule of this shape is trivial to make too STRICT, and too strict is worse than the bug:
# it rejects working programs. The rule cannot be keyed on what the CHECKER knows, because
# five methods lower correctly in codegen while being absent from method_signatures -
# `m.has(k)`, `m.set_float(k,v)`, `m.set_bool(k,v)`, `m.free()` and `s.add(x)`. Every one
# of those was measured as a REAL false positive of a first draft of this rule, and each
# is pinned below BY NAME with its value asserted. `s.add(x)` in particular is the primary
# set method. If a future change to the rule breaks any of them, this gate says which.
#
# The rule therefore asks dispatch_method() - the table codegen actually emits from - and
# only rejects when the compiler genuinely has no lowering.
#
# HOW THE REJECTION ARM IS KEPT HONEST. Each rejected name below was FIRST confirmed to
# be one codegen cannot lower: under the previous build every one printed "Unknown method"
# and then exited 0 with the call dropped. `s.free`, `s.to_array` and `s.elements` are
# there because they look plausible and are not real - to_array and elements were removed
# from the registry by #393 for having no lowering, and nothing restored them.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <source> <message-substring>
# Asserts ALL THREE of: `wyn check` fails, `wyn run` fails, `wyn build` fails - because
# the whole point of this issue is that a printed error did not fail the build, and only
# `check` failing would leave `wyn run` still untrustworthy. Also asserts the message,
# since a checker crash is non-zero too.
reject(){
  printf '%b\n' "$2" > "$TMP/r.wyn"
  out=$(perl -e 'alarm(30); exec @ARGV' -- "$WYNABS" check "$TMP/r.wyn" 2>&1); ccode=$?
  out=$(printf '%s' "$out" | sed 's/\x1b\[[0-9;]*m//g')
  perl -e 'alarm(60); exec @ARGV' -- "$WYNABS" run "$TMP/r.wyn" >"$TMP/run.out" 2>&1; rcode=$?
  perl -e 'alarm(60); exec @ARGV' -- "$WYNABS" build "$TMP/r.wyn" >/dev/null 2>&1; bcode=$?
  if [ $ccode -eq 0 ]; then bad "reject: $1 - wyn check EXITED 0"; return; fi
  if [ $rcode -eq 0 ]; then bad "reject: $1 - wyn run EXITED 0 (call silently dropped)"; return; fi
  if [ $bcode -eq 0 ]; then bad "reject: $1 - wyn build EXITED 0"; return; fi
  if ! echo "$out" | grep -q "$3"; then
    bad "reject: $1 - message missing [$3] got [$(echo "$out" | tr '\n' '|' | cut -c1-140)]"; return
  fi
  # The program must NOT have run to completion. This is the assertion the original
  # defect fails: it printed the error AND the sentinel.
  if grep -q "SENTINEL_REACHED" "$TMP/run.out"; then
    bad "reject: $1 - program RAN TO COMPLETION despite the error"; return
  fi
  # And the user must never be shown the compiler's own internal-error text for what is
  # ordinary user error.
  if grep -q "internal codegen error" "$TMP/run.out"; then
    bad "reject: $1 - leaked 'internal codegen error'"; return
  fi
  ok "reject: $1"
}

# accept <label> <source> <expected-stdout>  -- a POSITIVE canary: must still compile AND
# produce the right value. Asserting the value, not just exit 0, is deliberate: a dropped
# call also exits 0, which is the very defect being fixed.
accept(){
  printf '%b\n' "$2" > "$TMP/a.wyn"
  got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$TMP/a.wyn" 2>&1)
  got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
  if [ "$got" = "$3" ]; then ok "accept: $1"
  else bad "accept: $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-110)] want [$(echo "$3" | tr '\n' '|')]"; fi
}

echo "=== #426: an unknown collection method fails the build at check time ==="

# ---------------------------------------------------------------- the two reproductions
reject "map: unknown method, was a dropped call at exit 0" \
  'fn main() {\n    m = {"a": 1}\n    m.definitely_not_a_method(3)\n    print("SENTINEL_REACHED")\n}' \
  "map has no method 'definitely_not_a_method'"

reject "set: unknown method, was an internal codegen error" \
  'fn main() {\n    s = {:"a"}\n    u = {:"b"}\n    t = s.symmetric_difference(u)\n    print("SENTINEL_REACHED ${t.len()}")\n}' \
  "set has no method 'symmetric_difference'"

# ------------------------------------------- plausible-but-absent names, all exit 0 before
reject "set.free (no lowering)" \
  'fn main() {\n    s = {:"a"}\n    s.free()\n    print("SENTINEL_REACHED")\n}' \
  "set has no method 'free'"

reject "set.to_array (removed by #393, never restored)" \
  'fn main() {\n    s = {:"a"}\n    xs = s.to_array()\n    print("SENTINEL_REACHED ${xs.len()}")\n}' \
  "set has no method 'to_array'"

reject "set.elements (no lowering)" \
  'fn main() {\n    s = {:"a"}\n    xs = s.elements()\n    print("SENTINEL_REACHED ${xs.len()}")\n}' \
  "set has no method 'elements'"

reject "map: unknown method in a VALUE position" \
  'fn main() {\n    m = {"a": 1}\n    v = m.no_such_reader("a")\n    print("SENTINEL_REACHED ${v}")\n}' \
  "map has no method 'no_such_reader'"

# The hint has to survive the move from codegen to the checker, or the author is left with
# a bare rejection where they used to get a list of what the receiver does have.
reject "the rejection still carries the HashMap hint" \
  'fn main() {\n    m = {"a": 1}\n    m.nope()\n    print("SENTINEL_REACHED")\n}' \
  "HashMap has .get(key)"

reject "the rejection still carries the HashSet hint" \
  'fn main() {\n    s = {:"a"}\n    s.nope()\n    print("SENTINEL_REACHED")\n}' \
  "HashSet has .add(item)"

# ------------------------------------------------------------------- POSITIVE CANARIES
# The five that a checker-knowledge-only rule rejected, each measured as working first.
echo "--- canaries: methods that LOWER but are absent from method_signatures"

accept "map.has" \
  'fn main() {\n    m = {"a": 1}\n    print("${m.has("a")} ${m.has("z")}")\n}' \
  'true false'

accept "map.free" \
  'fn main() {\n    m = {"a": 1}\n    m.free()\n    print("freed")\n}' \
  'freed'

accept "set.add - the primary set method" \
  'fn main() {\n    s = {:"a"}\n    s.add("b")\n    print("${s.len()} ${s.contains("b")}")\n}' \
  '2 true'

echo "--- canaries: the set_* family, two of whose four rows were missing"

accept "map.set_float" \
  'fn main() {\n    m = {"a": 1.5}\n    m.set_float("b", 2.5)\n    print("${m.get("b")}")\n}' \
  '2.5'

accept "map.set_bool" \
  'fn main() {\n    m = {"a": true}\n    m.set_bool("b", false)\n    print("${m.get("b")} ${m.len()}")\n}' \
  'false 2'

accept "map.set_int (already registered - pinned beside its siblings)" \
  'fn main() {\n    m = {"a": 1}\n    m.set_int("b", 7)\n    print("${m.get_int("b")}")\n}' \
  '7'

accept "map.set_string (already registered - pinned beside its siblings)" \
  'fn main() {\n    m = {"a": "x"}\n    m.set_string("b", "y")\n    print("${m.get_string("b")}")\n}' \
  'y'

echo "--- canaries: the ordinary methods, so the rule cannot swallow the common path"

accept "map: get/set/len/keys" \
  'fn main() {\n    m = {"a": 1}\n    m.set("b", 2)\n    print("${m.get("b")} ${m.len()} ${m.keys().len()}")\n}' \
  '2 2 2'

accept "map: remove/is_empty/clear" \
  'fn main() {\n    m = {"a": 1}\n    m.remove("a")\n    print("${m.is_empty()}")\n    m.clear()\n    print("${m.len()}")\n}' \
  'true
0'

accept "set: contains/remove/len/is_empty" \
  'fn main() {\n    s = {:"a"}\n    s.add("b")\n    s.remove("a")\n    print("${s.contains("a")} ${s.len()} ${s.is_empty()}")\n}' \
  'false 1 false'

accept "set: the four set-algebra methods that DO have lowerings" \
  'fn main() {\n    a = {:"x"}\n    b = {:"x"}\n    print("${a.union(b).len()} ${a.intersection(b).len()} ${a.difference(b).len()} ${a.is_subset(b)}")\n}' \
  '1 1 0 true'

# THE NAMESPACE SPELLING IS NOT A METHOD CALL, and this rule must stand aside for it.
# init_checker registers `HashMap` and `HashSet` as the collection TYPES, so the RECEIVER
# of `HashSet.add(s, x)` types TYPE_SET and arrives at the rule looking exactly like a
# method call on a value - while the collection is really the first ARGUMENT and the arity
# is one higher. A first draft rejected these, breaking three of the tree's own regression
# tests; a differential corpus sweep found it, not a reading of the code. Unknown methods
# on a namespace have their own rule, so nothing is lost by standing aside.
echo "--- canaries: the NAMESPACE spelling, which this rule must not touch"

accept "HashSet.add namespace spelling" \
  'fn main() {\n    s = {:"a"}\n    HashSet.add(s, "b")\n    print("${HashSet.contains(s, "b")}")\n}' \
  'true'

# Asserts `0`, not `false`, and that is deliberate. `HashMap.get_bool` prints the raw
# int - measured identical on v1.21.0, on this branch's base, and here - so it is a
# PRE-EXISTING gap in the bool-in-print authority (V-30) and NOT something this change
# touched. Pinning the value the compiler actually produces keeps the canary's real job
# (the call must not be REJECTED) without asserting a fix that does not exist; filed
# separately. If this line ever starts failing with `false`, that gap was fixed and this
# expectation should be updated rather than investigated.
accept "HashMap.get_bool namespace spelling (prints 0 - separate pre-existing gap)" \
  'fn main() {\n    m = {"a": true}\n    HashMap.set_bool(m, "b", false)\n    print("${HashMap.get_bool(m, "b")}")\n}' \
  '0'

accept "HashMap.set/get namespace spelling" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.set(m, "k", 5)\n    print("${HashMap.get(m, "k")} ${HashMap.len(m)}")\n}' \
  '5 1'

echo "--- canaries: string and array, whose arm of this rule predates #426"

accept "string methods still compile" \
  'fn main() {\n    print("${"ab".upper()} ${"ab".len()}")\n}' \
  'AB 2'

accept "array methods still compile" \
  'fn main() {\n    xs = [1, 2, 3]\n    print("${xs.len()} ${xs.pop()}")\n}' \
  '3 3'

reject "string: unknown method still rejected (pre-existing arm)" \
  'fn main() {\n    print("${"ab".no_such_string_method()}")\n    print("SENTINEL_REACHED")\n}' \
  "string has no method 'no_such_string_method'"

reject "array: unknown method still rejected (pre-existing arm)" \
  'fn main() {\n    xs = [1, 2]\n    xs.no_such_array_method()\n    print("SENTINEL_REACHED")\n}' \
  "array has no method 'no_such_array_method'"

# `array.find` MUST STAY REJECTED, and this is the arm that catches the subtlest way to
# get #426 wrong. dispatch_method() maps array.find to array_find_fn, which is real and
# in both runtime headers - so a rule that accepts anything dispatch_method can lower
# makes `a.find(8)` compile again. #393 REMOVED that row on purpose: array_find_fn
# returns a bare `long long`, so the call cannot be typed (typing it as OptionInt hands
# `.is_some()` a non-Option value), and rejecting it is the intended behaviour. A first
# draft of this fix consulted dispatch_method for every receiver and silently reversed
# that decision in 20 corpus files. "codegen could emit something" is not the same claim
# as "the language offers this method".
reject "array.find stays rejected - dispatch_method maps it, #393 removed it anyway" \
  'fn main() {\n    xs = [1, 2, 8]\n    print("${xs.find(8)}")\n    print("SENTINEL_REACHED")\n}' \
  "array has no method 'find'"

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
