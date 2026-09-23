#!/bin/bash
# V-35: a non-string HashSet element must be rejected AT CHECK TIME, not passed to
# the runtime as if it were a pointer.
#
#   s = {:1, 2}          # v1.21.0-dev: `wyn check` PASSED, then SIGSEGV (exit 139)
#                        # with the set never even used.
#
# WHY IT CRASHED, AND WHY THE ANSWER IS "REJECT" RATHER THAN "SUPPORT INTS"
#
# The runtime set is string-keyed by construction: `hashset_add(WynHashSet*, const
# char* key)` stores `strdup(key)` and compares with `strcmp`. Codegen emitted the
# element expression straight into that parameter, so `{:1, 2}` became
# `hashset_add(set, 1)` - the integer 1 used as an address, dereferenced by strcmp.
# Every non-string element on every spelling did this: the literal, a variable
# holding an int, `.add`/`.insert`/`.contains`/`.remove`, and the
# `HashSet.add(s, x)` namespace form. A float element missed the segfault only by
# failing in the C compiler instead.
#
# Making ints WORK cheaply would mean stringifying the element into that same
# table, which collapses `{:1}` and `{:"1"}` into one set - a silently wrong
# answer, worse than the crash. A genuinely typed set needs an element type on
# TYPE_SET (which today carries none, while type_to_string already prints the
# unearned `HashSet<int>`), so it is a feature, logged separately. Until it exists,
# a HashSet is a set of strings and says so.
#
# HashMap already had this exact rule - `{1: "one"}` is refused with "HashMap keys
# must be strings" - so this is one rule that had one copy, not a new restriction.
#
# WHAT THE ALLOW ARMS ARE FOR. TYPE_INT is the checker's fallback for any
# expression it could not resolve (#372), so a rule keyed on the element's type
# risks rejecting a string the checker merely lost. The canary is the
# StringBuilder-derived variable: `v = sb.to_string(); {:v}` builds and runs today,
# and must keep doing so. The `.to_string()` arms pin the remedy the error message
# itself recommends - a message that names a fix has to be a fix that works. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <source>
# `wyn check` must fail AND name the rule. Checking the message and not just the
# exit code is deliberate: a crash in the checker, or an unrelated parse error,
# would also be non-zero.
reject(){
  printf '%b\n' "$2" > "$TMP/r.wyn"
  out=$("$WYNABS" check "$TMP/r.wyn" 2>&1); code=$?
  if [ $code -ne 0 ] && echo "$out" | grep -q "HashSet stores strings"; then
    ok "reject: $1"
  else bad "reject: $1 (code=$code) [$(echo "$out" | tr '\n' '|')]"; fi
}

# allow <label> <source> <expected-stdout>
# Checks clean AND runs to the right answer - a rule that merely stops rejecting
# something it never should have rejected has proved nothing about the runtime.
allow(){
  d="$TMP/a$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$2" > "$d/a.wyn"
  cout=$("$WYNABS" check "$d/a.wyn" 2>&1); ccode=$?
  if [ $ccode -ne 0 ]; then
    bad "allow: $1 - check rejected it [$(echo "$cout" | tr '\n' '|')]"; return
  fi
  got=$("$WYNABS" run "$d/a.wyn" 2>&1 | grep -v 'Compiled in' | grep -v '^Warning:' | grep -v 'unused variable')
  if [ "$got" = "$3" ]; then ok "allow: $1"
  else bad "allow: $1 - got [$(echo "$got" | tr '\n' '|')] want [$(echo "$3" | tr '\n' '|')]"; fi
}

echo "-- rejected: a non-string element in the {:...} literal"
reject "int literal set"        'fn main() {\n  s = {:1, 2}\n  print(s.len())\n}'
reject "single int element"     'fn main() {\n  s = {:1}\n  print(s.len())\n}'
reject "int in SECOND position" 'fn main() {\n  s = {:"a", 1}\n  print(s.len())\n}'
reject "negative int element"   'fn main() {\n  s = {:-1}\n  print(s.len())\n}'
reject "float element"          'fn main() {\n  s = {:1.5}\n  print(s.len())\n}'
reject "bool element"           'fn main() {\n  s = {:true}\n  print(s.len())\n}'
reject "int VARIABLE element"   'fn main() {\n  x = 1\n  s = {:x}\n  print(s.len())\n}'

echo "-- rejected: every set method that takes an element"
reject "s.add(int)"      'fn main() {\n  s = {:"a"}\n  s.add(2)\n  print(s.len())\n}'
reject "s.insert(int)"   'fn main() {\n  s = {:"a"}\n  s.insert(2)\n  print(s.len())\n}'
reject "s.contains(int)" 'fn main() {\n  s = {:"a"}\n  print(s.contains(1))\n}'
reject "s.remove(int)"   'fn main() {\n  s = {:"a"}\n  s.remove(1)\n  print(s.len())\n}'
reject "s.add(bool var)" 'fn main() {\n  b = true\n  s = {:"a"}\n  s.add(b)\n  print(s.len())\n}'
# The shapes dogfooding actually produces, all three verified to SIGSEGV before the rule:
# a set consulted with the loop index, an element read out of an int array, and an int
# function parameter.
reject "s.contains(loop var)"   'fn main() {\n  s = {:"a"}\n  for i in 0..2 {\n    print(s.contains(i))\n  }\n}'
reject "s.add(int array elem)"  'fn main() {\n  a = [1,2]\n  s = {:"x"}\n  s.add(a[0])\n  print(s.len())\n}'
reject "s.add(int parameter)"   'fn f(k: int) {\n  s = {:"a"}\n  s.add(k)\n}\nfn main() { f(1) }'

echo "-- rejected: the HashSet.<method>(set, element) namespace form"
# Element is the LAST argument in both shapes, which is why one rule covers them.
reject "HashSet.add(s, int)"      'fn main() {\n  s = HashSet.new()\n  HashSet.add(s, 1)\n  print("x")\n}'
reject "HashSet.contains(s, int)" 'fn main() {\n  s = HashSet.new()\n  print(HashSet.contains(s, 1))\n}'
reject "HashSet.remove(s, int)"   'fn main() {\n  s = {:"a"}\n  HashSet.remove(s, 1)\n  print("x")\n}'
# `HashSet.insert` is refused by a DIFFERENT authority - the namespace rule (#369),
# because the namespace form has no `insert` at all. Asserted with its own message so
# this gate records which rule owns it, rather than quietly counting it as this one's.
# (`s.insert(1)`, the method form, IS this rule's and is covered above.)
printf '%b\n' 'fn main() {\n  s = {:"a"}\n  HashSet.insert(s, 1)\n  print("x")\n}' > "$TMP/ins.wyn"
out=$("$WYNABS" check "$TMP/ins.wyn" 2>&1)
if [ $? -ne 0 ] && echo "$out" | grep -q "unknown method 'HashSet.insert'"; then
  ok "reject: HashSet.insert(s, int) - by the namespace rule"
else bad "reject: HashSet.insert(s, int) [$(echo "$out" | tr '\n' '|')]"; fi

echo "-- rejected: add_int / contains_int, whose C symbols do not exist"
# src/types.c advertised these two, lowering them to wyn_hashset_add_int and
# wyn_hashset_contains_int - symbols no runtime source defines. They are the reason
# someone would believe int elements are supported, so the same rule answers them
# instead of leaving an internal codegen error as the language's reply.
reject "s.add_int(1)"      'fn main() {\n  s = {:"a"}\n  s.add_int(1)\n  print(s.len())\n}'
reject "s.contains_int(1)" 'fn main() {\n  s = {:"a"}\n  print(s.contains_int(1))\n}'

echo "-- allowed: the string set, whole API, still works"
allow "literal + contains" 'fn main() {\n  s = {:"a","b"}\n  print(s.contains("a"))\n  print(s.contains("zz"))\n}' 'true
false'
allow "empty {:} + len" 'fn main() {\n  s = {:}\n  print(s.len())\n}' '0'
allow "add / insert / remove / len / is_empty" 'fn main() {\n  s = {:"a","b"}\n  s.add("c")\n  s.insert("d")\n  s.remove("a")\n  print(s.len())\n  print(s.is_empty())\n  print(s.contains("d"))\n}' '3
false
true'
allow "HashSet.new + namespace add/contains" 'fn main() {\n  s = HashSet.new()\n  HashSet.add(s, "x")\n  print(HashSet.contains(s, "x"))\n}' 'true'

echo "-- allowed: string elements the checker has to look through to see"
allow "string variable element" 'fn main() {\n  k = "a"\n  s = {:k}\n  print(s.contains(k))\n}' 'true'
allow "concatenation element"   'fn main() {\n  s = {:"a" + "b"}\n  print(s.contains("ab"))\n}' 'true'
allow "interpolated element"    'fn main() {\n  x = 3\n  s = {:"n${x}"}\n  print(s.contains("n3"))\n}' 'true'
allow "fn-returning-string element" 'fn f() -> string { return "q" }\nfn main() {\n  s = {:f()}\n  print(s.contains("q"))\n}' 'true'
# THE CANARY. `v` is a string the checker had to resolve THROUGH an unresolved
# StringBuilder receiver. If this arm ever fails, the rule has started trusting the
# checker's int fallback and is rejecting working code.
allow "StringBuilder-derived element" 'fn main() {\n  var sb = StringBuilder.new()\n  sb.append("hi")\n  v = sb.to_string()\n  s = {:v}\n  print(s.contains("hi"))\n}' 'true'
# The same question asked of every way a string ARRIVES rather than is written. These are
# the arms that would red if the rule started keying on anything coarser than the type.
allow "string-index element"    'fn main() {\n  t = "abc"\n  s = {:"a"}\n  s.add(t[0])\n  print(s.contains("a"))\n}' 'true'
allow "string-array element"    'fn main() {\n  a = ["x","y"]\n  s = {:"x"}\n  print(s.contains(a[0]))\n}' 'true'
allow "string-map-value element" 'fn main() {\n  m = {"k": "v"}\n  s = {:"v"}\n  print(s.contains(m["k"]))\n}' 'true'
allow "string parameter element" 'fn f(k: string) -> bool {\n  s = {:"b"}\n  return s.contains(k)\n}\nfn main() { print(f("b")) }' 'true'

echo "-- allowed: the remedy the error message recommends"
allow "to_string() in the literal" 'fn main() {\n  x = 1\n  s = {:x.to_string()}\n  print(s.contains("1"))\n}' 'true'
allow "to_string() in .add()"      'fn main() {\n  x = 7\n  s = {:"a"}\n  s.add(x.to_string())\n  print(s.contains("7"))\n}' 'true'

echo "-- the set's type prints its real element type"
# `HashSet<int>` was printed for a set whose element is the string "a" - naming an
# element type the language has never had, in the same breath as a rule that refuses it.
printf '%b\n' 'fn f() -> int {\n  return {:"a"}\n}\nfn main() { print(f()) }' > "$TMP/tn.wyn"
out=$("$WYNABS" check "$TMP/tn.wyn" 2>&1)
if echo "$out" | grep -q "HashSet<string>" && ! echo "$out" | grep -q "HashSet<int>"; then
  ok "type name: HashSet<string>"
else bad "type name: HashSet<string> [$(echo "$out" | tr '\n' '|')]"; fi

echo "-- allowed: the neighbouring literals this rule must not touch"
allow "int array literal"   'fn main() {\n  a = [1, 2, 3]\n  print(a.len())\n}' '3'
allow "int-valued hashmap"  'fn main() {\n  m = {"a": 1}\n  print(m["a"])\n}' '1'

echo ""; echo "set-element-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
