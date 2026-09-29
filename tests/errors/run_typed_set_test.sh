#!/bin/bash
# V-38 (#391): a genuinely TYPED HashSet. TYPE_SET carries an element type, the runtime
# stores a TAGGED element, and an element-type mismatch is a check-time error.
#
# WHAT THIS REPLACES. TYPE_SET carried no element type at all, so a set was effectively
# untyped: `{:1, 2}` passed `wyn check` and SIGSEGV'd (the int 1 reached
# `hashset_add(WynHashSet*, const char*)` and strcmp dereferenced it), and the type
# printer named an element type the language did not have. #374 stopped the crash by
# REFUSING every non-string element, and said why the cheap fix was worse: stringifying
# an int into the same string table collapses `{:1}` and `{:"1"}` into one set, a
# silently wrong answer. This gate is that rejection rule's replacement, and it keeps
# every canary #374's gate pinned - so there is ONE gate for set element typing, not two
# lists of the same thing that have to agree.
#
# THE LOAD-BEARING ASSERTION is "tags keep the kinds apart": a `HashSet<int>` holding 1
# does NOT contain the string "1", and a `HashSet<string>` holding "1" does not contain
# the int 1. It is asserted through a bare `HashSet` parameter, because that is the one
# receiver whose element type is OPEN - so the probe dispatches on the ARGUMENT's type
# and really does ask the set the cross-kind question. Every other spelling is refused
# at check time by the mismatch rule, which is the point.
#
# BOTH MODES throughout. `--release` emits wyn_runtime_slim.h (declarations only) and
# links libwyn_rt.a, so a new runtime function declared in only one header breaks
# release alone - `hashset_elements` (the iteration bridge) is exactly that shape.
#
# ORDER IS NOT ASSERTED. Set iteration is bucket order, not insertion order (the same
# caveat hashmap_keys() carries), so every iteration arm reduces to an order-independent
# value: a sum, or a count.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <source> <message-substring>
# `wyn check` must FAIL and name the rule. Asserting the message and not just the exit
# code is deliberate: a checker crash, or an unrelated parse error, is also non-zero.
reject(){
  printf '%b\n' "$2" > "$TMP/r.wyn"
  out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYNABS" check "$TMP/r.wyn" 2>&1); code=$?
  if [ $code -ne 0 ] && echo "$out" | grep -q "$3"; then ok "reject: $1"
  else bad "reject: $1 (code=$code) [$(echo "$out" | tr '\n' '|' | cut -c1-140)]"; fi
}

# expect <label> <source> <expected-stdout>   -- runs in BOTH modes
expect(){
  d="$TMP/c$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  for mode in debug release; do
    if [ "$mode" = release ]; then
      got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run --release "$d/a.wyn" 2>&1)
    else
      got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d/a.wyn" 2>&1)
    fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
    if [ "$got" = "$3" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-110)] want [$(echo "$3" | tr '\n' '|')]"; fi
  done
}

# check_only <label> <source>   -- must type-check clean (no run)
check_only(){
  printf '%b\n' "$2" > "$TMP/p.wyn"
  out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYNABS" check "$TMP/p.wyn" 2>&1); code=$?
  if [ $code -eq 0 ]; then ok "check: $1"
  else bad "check: $1 (code=$code) [$(echo "$out" | tr '\n' '|' | cut -c1-140)]"; fi
}

echo "-- the whole point: an int set works, and is not a string set"
expect "int literal set" 'fn main() {\n  s = {:1, 2, 3}\n  print(s.len())\n  print(s.contains(2))\n  print(s.contains(9))\n}' '3
true
false'
expect "int add / remove / len" 'fn main() {\n  var s = {:1, 2}\n  s.add(3)\n  s.insert(4)\n  s.remove(1)\n  print(s.len())\n  print(s.contains(1))\n  print(s.contains(4))\n}' '3
false
true'
expect "duplicate ints collapse" 'fn main() {\n  s = {:1, 1, 1}\n  print(s.len())\n}' '1'
expect "negative and large ints" 'fn main() {\n  s = {:-7, 4611686018427387904}\n  print(s.len())\n  print(s.contains(-7))\n  print(s.contains(4611686018427387904))\n  print(s.contains(7))\n}' '2
true
true
false'
expect "float set" 'fn main() {\n  s = {:1.5, 2.5}\n  print(s.len())\n  print(s.contains(1.5))\n  print(s.contains(3.5))\n}' '2
true
false'
expect "bool set" 'fn main() {\n  s = {:true}\n  print(s.contains(true))\n  print(s.contains(false))\n  s.add(false)\n  print(s.len())\n}' 'true
false
2'
expect "string set (the pre-existing behaviour)" 'fn main() {\n  var s = {:"a","b"}\n  s.add("c")\n  s.insert("d")\n  s.remove("a")\n  print(s.len())\n  print(s.is_empty())\n  print(s.contains("d"))\n}' '3
false
true'

echo "-- TAGS keep the kinds apart: {:1} does not contain \"1\""
# THE reason the runtime element had to become tagged rather than stringified. The probe
# takes a bare `HashSet` (element type OPEN), so it dispatches on the ARGUMENT and really
# asks the cross-kind question; every other spelling is a check-time error by design.
expect "int set vs string probe, and back" 'fn probe_str(s: HashSet) -> bool { return HashSet.contains(s, "1") }\nfn probe_int(s: HashSet) -> bool { return HashSet.contains(s, 1) }\nfn main() {\n  a = {:1}\n  b = {:"1"}\n  print(probe_str(a))\n  print(probe_int(b))\n  print(a.contains(1))\n  print(b.contains("1"))\n}' 'false
false
true
true'
expect "bool true is not the int 1" 'fn probe_int(s: HashSet) -> bool { return HashSet.contains(s, 1) }\nfn main() {\n  b = {:true}\n  print(probe_int(b))\n  print(b.contains(true))\n}' 'false
true'

echo "-- the OPEN set: {:} and HashSet.new() are fixed by the first insert"
expect "empty literal, then add(int)" 'fn main() {\n  var s = {:}\n  print(s.len())\n  s.add(7)\n  s.add(7)\n  print(s.len())\n  print(s.contains(7))\n}' '0
1
true'
expect "empty literal, then add(string)" 'fn main() {\n  var s = {:}\n  s.add("k")\n  print(s.contains("k"))\n  print(s.contains("z"))\n}' 'true
false'
expect "HashSet.new() + namespace int API" 'fn main() {\n  s = HashSet.new()\n  HashSet.add(s, 42)\n  print(HashSet.contains(s, 42))\n  HashSet.remove(s, 42)\n  print(HashSet.contains(s, 42))\n}' 'true
false'
expect "HashSet.new() + namespace string API" 'fn main() {\n  s = HashSet.new()\n  HashSet.add(s, "x")\n  print(HashSet.contains(s, "x"))\n}' 'true'
# TWO `HashSet.new()` calls are TWO sets. The namespace path adopted the registered
# `HashSet_new` return-type node directly, so every set in a program was the SAME Type*
# and the first `.add()` fixed the element type of all of them - this program was refused
# with "'add()' on a HashSet<int> was given string" on the second, unrelated set. The
# `{:}` literal path already allocated per site.
expect "two HashSet.new() sets are independent" 'fn main() {\n  a = HashSet.new()\n  HashSet.add(a, 1)\n  c = HashSet.new()\n  HashSet.add(c, "y")\n  print(HashSet.contains(a, 1))\n  print(HashSet.contains(c, "y"))\n  print(HashSet.contains(a, 2))\n}' 'true
true
false'
# The MAP half of the same shared node, fixed in the same place because it is one
# authority. On the pre-fix compiler the string-valued map printed `0`.
expect "two HashMap.new() maps are independent" 'fn main() {\n  a = HashMap.new()\n  a.set("k", 1)\n  b = HashMap.new()\n  b.set("k", "s")\n  print(a["k"])\n  print(b["k"])\n}' '1
s'
# The `::` spelling is a THIRD path: the parser folds `HashSet::add` into one identifier,
# so it lowers through the ident branch, which sees no arguments. It emitted the
# string-keyed call and SEGFAULTED on an int - and slipped past #374's rule entirely,
# which only ever saw the `.` forms.
expect "HashSet:: namespace int API" 'fn main() {\n  s = HashSet::new()\n  HashSet::add(s, 1)\n  print(HashSet::contains(s, 1))\n  print(HashSet::contains(s, 2))\n}' 'true
false'
expect "HashSet:: namespace string API" 'fn main() {\n  s = HashSet::new()\n  HashSet::add(s, "a")\n  print(HashSet::contains(s, "a"))\n}' 'true'

echo "-- iteration binds the ELEMENT type (this used to be an internal codegen error)"
expect "iterate an int set (sum)" 'fn main() {\n  s = {:1, 2, 3}\n  var t = 0\n  for x in s {\n    t = t + x\n  }\n  print(t)\n}' '6'
expect "iterate a string set (total length)" 'fn main() {\n  s = {:"ab", "cde"}\n  var n = 0\n  for w in s {\n    n = n + w.len()\n  }\n  print(n)\n}' '5'
expect "iterate a float set (sum)" 'fn main() {\n  s = {:1.5, 2.5}\n  var t = 0.0\n  for x in s {\n    t = t + x\n  }\n  print(t)\n}' '4.0'
expect "iterate a bool set (count)" 'fn main() {\n  s = {:true, false}\n  var n = 0\n  for x in s {\n    n = n + 1\n  }\n  print(n)\n}' '2'
expect "iterate an empty set" 'fn main() {\n  s = {:}\n  var n = 0\n  for x in s {\n    n = n + 1\n  }\n  print(n)\n}' '0'

echo "-- set algebra carries the element type through its RESULT"
expect "int union / intersection / difference" 'fn main() {\n  a = {:1, 2, 3}\n  b = {:2, 3, 4}\n  print(a.union(b).len())\n  print(a.intersection(b).len())\n  print(a.difference(b).len())\n  print(a.intersection(b).contains(2))\n  print(a.difference(b).contains(1))\n  print(a.difference(b).contains(2))\n}' '4
2
1
true
true
false'
expect "int predicates" 'fn main() {\n  a = {:1}\n  b = {:1, 2}\n  print(a.is_subset(b))\n  print(b.is_superset(a))\n  print(a.is_disjoint(b))\n  print(a.is_disjoint({:9}))\n}' 'true
true
false
true'
expect "float union membership" 'fn main() {\n  a = {:1.5}\n  b = {:2.5}\n  c = a.union(b)\n  print(c.len())\n  print(c.contains(2.5))\n  print(c.contains(3.5))\n}' '2
true
false'
expect "string algebra (the pre-existing behaviour)" 'fn main() {\n  a = {:"x","y"}\n  b = {:"y"}\n  print(a.union(b).len())\n  print(a.intersection(b).contains("y"))\n  print(a.difference(b).contains("x"))\n}' '2
true
true'
expect "clear / is_empty on an int set" 'fn main() {\n  var c = {:1, 2}\n  c.clear()\n  print(c.len())\n  print(c.is_empty())\n}' '0
true'
# These two are what make the element type on the RESULT load-bearing rather than
# incidental. Membership on an algebra result does NOT need it (codegen falls back to
# the probe's own type), so mutating the propagation out reddened nothing until these
# were added: ITERATING a result needs the declared element type to pick the getter,
# and the MISMATCH rule needs it to have anything to compare against.
expect "iterate a union result" 'fn main() {\n  a = {:1, 2}\n  b = {:2, 3}\n  var t = 0\n  for x in a.union(b) {\n    t = t + x\n  }\n  print(t)\n}' '6'
expect "iterate a difference result (strings)" 'fn main() {\n  a = {:"ab","cde"}\n  b = {:"ab"}\n  var n = 0\n  for w in a.difference(b) {\n    n = n + w.len()\n  }\n  print(n)\n}' '3'
reject "union result .add(string)" 'fn main() {\n  a = {:1}\n  b = {:2}\n  var u = a.union(b)\n  u.add("x")\n  print(u.len())\n}' "'add()' on a HashSet<int> was given string"

echo "-- membership operators dispatch by element type"
expect "int `in` / `not in`" 'fn main() {\n  a = {:10, 20}\n  print(10 in a)\n  print(3 in a)\n  print(3 not in a)\n  print(10 not in a)\n}' 'true
false
true
false'
expect "string `in`" 'fn main() {\n  a = {:"p"}\n  print("p" in a)\n  print("q" in a)\n}' 'true
false'

echo "-- add_int / contains_int, whose C symbols did not previously exist"
# types.c advertised these two, lowering them to wyn_hashset_add_int /
# wyn_hashset_contains_int - symbols no runtime source defined, so the language's answer
# to the two methods that sounded like int support was an internal codegen error.
expect "add_int / contains_int" 'fn main() {\n  var z = {:}\n  z.add_int(9)\n  print(z.contains_int(9))\n  print(z.contains_int(8))\n  print(z.len())\n}' 'true
false
1'

echo "-- the element type on DECLARATIONS"
expect "HashSet<int> parameter" 'fn total(s: HashSet<int>) -> int {\n  var t = 0\n  for x in s {\n    t = t + x\n  }\n  return t\n}\nfn main() { print(total({:10, 20})) }' '30'
# The ITERATION is the load-bearing half: `m.contains(5)` passes even when the declared
# return type is OPEN (codegen falls back to the probe's type), so a `-> HashSet<int>`
# arm that only asserts membership does not test the annotation at all.
expect "-> HashSet<int> return" 'fn make() -> HashSet<int> {\n  var r = {:}\n  r.add(5)\n  r.add(6)\n  return r\n}\nfn main() {\n  m = make()\n  print(m.len())\n  print(m.contains(5))\n  var t = 0\n  for x in m {\n    t = t + x\n  }\n  print(t)\n}' '2
true
11'
expect "HashSet<string> parameter" 'fn has(s: HashSet<string>, k: string) -> bool { return s.contains(k) }\nfn main() {\n  print(has({:"a"}, "a"))\n  print(has({:"a"}, "b"))\n}' 'true
false'
check_only "bare HashSet parameter still checks (OPEN)" 'fn has(s: HashSet, k: string) -> bool { return HashSet.contains(s, k) }\nfn main() { print(has({:"a"}, "a")) }'

echo "-- rejected: a mixed literal"
reject "string then int"   'fn main() {\n  s = {:"a", 1}\n  print(s.len())\n}' "mixed element types"
reject "int then string"   'fn main() {\n  s = {:1, "a"}\n  print(s.len())\n}' "mixed element types"
reject "int then float"    'fn main() {\n  s = {:1, 2.5}\n  print(s.len())\n}' "mixed element types"
reject "bool then int"     'fn main() {\n  s = {:true, 1}\n  print(s.len())\n}' "mixed element types"

echo "-- rejected: an element that disagrees with the set's element type"
reject "int set .add(string)"      'fn main() {\n  var s = {:1}\n  s.add("x")\n  print(s.len())\n}' "'add()' on a HashSet<int> was given string"
reject "int set .insert(string)"   'fn main() {\n  var s = {:1}\n  s.insert("x")\n  print(s.len())\n}' "'insert()' on a HashSet<int> was given string"
reject "int set .contains(string)" 'fn main() {\n  s = {:1}\n  print(s.contains("x"))\n}' "'contains()' on a HashSet<int> was given string"
reject "int set .remove(string)"   'fn main() {\n  var s = {:1}\n  s.remove("x")\n  print(s.len())\n}' "'remove()' on a HashSet<int> was given string"
reject "int set .add(float)"       'fn main() {\n  var s = {:1}\n  s.add(1.5)\n  print(s.len())\n}' "'add()' on a HashSet<int> was given float"
reject "string set .add(int)"      'fn main() {\n  var s = {:"a"}\n  s.add(2)\n  print(s.len())\n}' "'add()' on a HashSet<string> was given int"
reject "string set .contains(int)" 'fn main() {\n  s = {:"a"}\n  print(s.contains(1))\n}' "'contains()' on a HashSet<string> was given int"
reject "string set .add(bool var)" 'fn main() {\n  b = true\n  var s = {:"a"}\n  s.add(b)\n  print(s.len())\n}' "'add()' on a HashSet<string> was given bool"
reject "float set .add(int)"       'fn main() {\n  var s = {:1.5}\n  s.add(2)\n  print(s.len())\n}' "'add()' on a HashSet<float> was given int"
reject "bool set .add(int)"        'fn main() {\n  var s = {:true}\n  s.add(1)\n  print(s.len())\n}' "'add()' on a HashSet<bool> was given int"
reject "add_int on a string set"   'fn main() {\n  var s = {:"a"}\n  s.add_int(1)\n  print(s.len())\n}' "'add_int()' on a HashSet<string> was given int"
# The shapes dogfooding actually produces, all three of which SIGSEGV'd before #374.
reject "string set .contains(loop var)"  'fn main() {\n  s = {:"a"}\n  for i in 0..2 {\n    print(s.contains(i))\n  }\n}' "'contains()' on a HashSet<string> was given int"
reject "string set .add(int array elem)" 'fn main() {\n  a = [1,2]\n  var s = {:"x"}\n  s.add(a[0])\n  print(s.len())\n}' "'add()' on a HashSet<string> was given int"
reject "string set .add(int parameter)"  'fn f(k: int) {\n  var s = {:"a"}\n  s.add(k)\n}\nfn main() { f(1) }' "'add()' on a HashSet<string> was given int"
# The NAMESPACE spelling, whose element is the LAST argument - one rule, both forms.
reject "HashSet.add(int set, string)"      'fn main() {\n  s = {:1}\n  HashSet.add(s, "a")\n  print(s.len())\n}' "'add()' on a HashSet<int> was given string"
reject "HashSet.contains(string set, int)" 'fn main() {\n  s = {:"a"}\n  print(HashSet.contains(s, 1))\n}' "'contains()' on a HashSet<string> was given int"
# An OPEN set is fixed by the first INSERT, so the SECOND insert of another kind is the
# error - the mismatch rule and the inference share one element type.
reject "open set, add(int) then add(string)" 'fn main() {\n  var s = {:}\n  s.add(1)\n  s.add("x")\n  print(s.len())\n}' "'add()' on a HashSet<int> was given string"
reject "HashSet<int> param given a string"   'fn f(s: HashSet<int>) {\n  s.add("x")\n}\nfn main() { f({:1}) }' "'add()' on a HashSet<int> was given string"

echo "-- rejected: an element the runtime cannot tag"
# The one genuinely NEW rejection in this change (#374 refused only int/float/bool, so a
# struct element passed check and miscompiled), and the reason a corpus sweep was run.
reject "struct element"  'struct P { x: int }\nfn main() {\n  p = P { x: 1 }\n  s = {:p}\n  print(s.len())\n}' "must be string, int, float or bool"
reject "array element"   'fn main() {\n  a = [1,2]\n  s = {:a}\n  print(s.len())\n}' "must be string, int, float or bool"
reject "nested set"      'fn main() {\n  i = {:1}\n  s = {:i}\n  print(s.len())\n}' "must be string, int, float or bool"
reject "map element"     'fn main() {\n  m = {"a": 1}\n  s = {:m}\n  print(s.len())\n}' "must be string, int, float or bool"
reject "struct via .add" 'struct P { x: int }\nfn main() {\n  p = P { x: 1 }\n  var s = {:"a"}\n  s.add(p)\n  print(s.len())\n}' "must be string, int, float or bool"

echo "-- rejected: set algebra across two different element types"
reject "int union string"          'fn main() {\n  a = {:1}\n  b = {:"x"}\n  print(a.union(b).len())\n}' "needs two sets of the same element type"
reject "string intersection int"  'fn main() {\n  a = {:"x"}\n  b = {:1}\n  print(a.intersection(b).len())\n}' "needs two sets of the same element type"
reject "int is_subset float"      'fn main() {\n  a = {:1}\n  b = {:1.5}\n  print(a.is_subset(b))\n}' "needs two sets of the same element type"
# NOT rejected: algebra where one side's element type is unknown. The checker's int
# fallback (#372) means a rule that demanded a resolved set on both sides would reject
# sets it merely failed to resolve.
check_only "algebra with an OPEN set is allowed" 'fn main() {\n  a = {:1}\n  b = {:}\n  print(a.union(b).len())\n}'

echo "-- the type printer names the REAL element type"
# It printed a hardcoded `HashSet<int>` while the set was string-only, then a hardcoded
# `HashSet<string>` (#374). Now neither is hardcoded.
for pair in "int:{:1}" "string:{:\"a\"}" "float:{:1.5}" "bool:{:true}"; do
  want=${pair%%:*}; lit=${pair#*:}
  printf '%b\n' "fn f() -> int {\n  return $lit\n}\nfn main() { print(f()) }" > "$TMP/tn.wyn"
  out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYNABS" check "$TMP/tn.wyn" 2>&1)
  if echo "$out" | grep -q "HashSet<$want>"; then ok "type name: HashSet<$want>"
  else bad "type name: HashSet<$want> [$(echo "$out" | tr '\n' '|' | cut -c1-120)]"; fi
done
# An OPEN set has no element type to name, and says so rather than guessing one.
printf '%b\n' 'fn f() -> int {\n  return {:}\n}\nfn main() { print(f()) }' > "$TMP/tno.wyn"
out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYNABS" check "$TMP/tno.wyn" 2>&1)
if echo "$out" | grep -q "HashSet<?>"; then ok "type name: HashSet<?> for an open set"
else bad "type name: HashSet<?> [$(echo "$out" | tr '\n' '|' | cut -c1-120)]"; fi

echo "-- CANARIES: string elements the checker has to look THROUGH to see"
# TYPE_INT is the checker's fallback for anything it could not resolve (#372). If any arm
# here reds, the element rules have started trusting that fallback and are mistyping - or
# rejecting - working code. Carried over verbatim from #374's gate, which is why there is
# one gate for this and not two.
expect "string variable element" 'fn main() {\n  k = "a"\n  s = {:k}\n  print(s.contains(k))\n}' 'true'
expect "concatenation element"   'fn main() {\n  s = {:"a" + "b"}\n  print(s.contains("ab"))\n}' 'true'
expect "interpolated element"    'fn main() {\n  x = 3\n  s = {:"n${x}"}\n  print(s.contains("n3"))\n}' 'true'
expect "fn-returning-string element" 'fn f() -> string { return "q" }\nfn main() {\n  s = {:f()}\n  print(s.contains("q"))\n}' 'true'
# THE canary: `v` is a string the checker had to resolve THROUGH an unresolved
# StringBuilder receiver.
expect "StringBuilder-derived element" 'fn main() {\n  var sb = StringBuilder.new()\n  sb.append("hi")\n  v = sb.to_string()\n  s = {:v}\n  print(s.contains("hi"))\n}' 'true'
expect "string-index element"    'fn main() {\n  t = "abc"\n  var s = {:"a"}\n  s.add(t[0])\n  print(s.contains("a"))\n}' 'true'
expect "string-array element"    'fn main() {\n  a = ["x","y"]\n  s = {:"x"}\n  print(s.contains(a[0]))\n}' 'true'
expect "string-map-value element" 'fn main() {\n  m = {"k": "v"}\n  s = {:"v"}\n  print(s.contains(m["k"]))\n}' 'true'
expect "string parameter element" 'fn f(k: string) -> bool {\n  s = {:"b"}\n  return s.contains(k)\n}\nfn main() { print(f("b")) }' 'true'

echo "-- the conversions the mismatch message recommends really work"
expect "to_string() into a string set" 'fn main() {\n  x = 1\n  s = {:x.to_string()}\n  print(s.contains("1"))\n}' 'true'
expect "to_string() in .add()"         'fn main() {\n  x = 7\n  var s = {:"a"}\n  s.add(x.to_string())\n  print(s.contains("7"))\n}' 'true'
expect "to_int() into an int set"      'fn main() {\n  var s = {:1}\n  s.add("5".to_int())\n  print(s.contains(5))\n}' 'true'

echo "-- neighbouring literals these rules must not touch (control)"
expect "int array literal"  'fn main() {\n  a = [1, 2, 3]\n  print(a.len())\n}' '3'
expect "int-valued hashmap" 'fn main() {\n  m = {"a": 1}\n  print(m["a"])\n}' '1'
expect "a set inside an array literal" 'fn main() {\n  s = {:1}\n  a = [s]\n  print(a.len())\n}' '1'

echo ""; echo "typed-set: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
