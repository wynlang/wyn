#!/bin/bash
# #429: the NAMESPACE spelling of a map store/read honours the map's VALUE TYPE.
#
# WHAT THIS PINS. `HashMap.set(m, k, v)` lowered to hashmap_set() unconditionally, and
# hashmap_set() is a thin alias for hashmap_insert_string() - so `HashMap.set(m, "k", 1)`
# handed the integer 1 where a `const char*` was expected and the runtime dereferenced
# it: SEGFAULT (exit 139) from documented syntax, on a program `wyn check` called clean.
# The paired read `HashMap.get(m, k)` had the same blindness in the other direction: it
# pinned to hashmap_get_string and decoded a tagged int as a `char*`, printing EMPTY at
# exit 0. This is the MAP half of what #391 fixed for sets.
#
# THE LOAD-BEARING ASSERTION is CROSS-SPELLING AGREEMENT, not just "the crash stopped".
# A map is written with one spelling and read with the OTHER, in both directions. That
# is the assertion the defect could not have passed at any point in its life, and it is
# the one that stays failing if a future change re-introduces a second copy of the
# value-type decision: two copies can agree with themselves and still disagree with
# each other. A same-spelling round trip cannot see that.
#
# WHY EVERY SCALAR KIND. The store side picks from a four-way family
# (hashmap_insert_{int,string,float,bool}) and the read side from a five-way one. Only
# `int` crashed; float and bool were equally unresolved and would have been left behind
# by a fix that keyed on "not a string". The string arm is here as the REGRESSION guard -
# it is the one shape the old code got right and the corpus already depends on
# (tests/acceptance/cli_tool.wyn stores strings this way).
#
# BOTH MODES throughout. `--release` emits wyn_runtime_slim.h (declarations only) and
# links libwyn_rt.a, so a getter declared in only one of the two headers passes in debug
# and fails release alone.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

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
    code=$?
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
    if [ "$got" = "$3" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 (code=$code) - got [$(echo "$got" | tr '\n' '|' | cut -c1-110)] want [$(echo "$3" | tr '\n' '|')]"; fi
  done
}

echo "=== #429: HashMap namespace spelling honours the value type ==="

# --- 1. THE CRASH. Each of these was exit 139 (or an empty read) before the fix.
expect "ns store + ns read: int" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.set(m, "k", 1)\n    print("${HashMap.get(m, "k")}")\n}' \
  '1'

expect "ns store + ns read: float" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.set(m, "k", 2.5)\n    print("${HashMap.get(m, "k")}")\n}' \
  '2.5'

expect "ns store + ns read: bool" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.set(m, "k", true)\n    print("${HashMap.get(m, "k")}")\n}' \
  'true'

# The REGRESSION guard: the one arm the old code got right, and the one the existing
# corpus uses. hashmap_set() was an alias for hashmap_insert_string(), so this shape
# must come out behaviourally identical.
expect "ns store + ns read: string (unchanged)" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.set(m, "k", "v")\n    print("${HashMap.get(m, "k")}")\n}' \
  'v'

# `insert` is the same store under a second name, and it was blind in the OPPOSITE
# direction: `HashMap.insert` lowered to hashmap_insert(), whose value parameter is an
# `int`, so a STRING value was stored as its pointer truncated to an int and read back
# as a number like 1321456 - silently, at exit 0. A fix that only chased the crash
# would have left this one, which is the harder of the two to notice.
expect "ns insert: string (was a truncated pointer)" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.insert(m, "k", "v")\n    print("${HashMap.get(m, "k")}")\n}' \
  'v'

expect "ns insert: int" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.insert(m, "k", 3)\n    print("${HashMap.get(m, "k")}")\n}' \
  '3'

# --- 2. CROSS-SPELLING AGREEMENT. Written with one spelling, read with the other.
# This is what a second copy of the value-type decision cannot pass.
expect "method store -> namespace read" \
  'fn main() {\n    m = HashMap.new()\n    m.set("k", 7)\n    print("${HashMap.get(m, "k")}")\n}' \
  '7'

expect "namespace store -> method read" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.set(m, "k", 9)\n    print("${m.get("k")}")\n}' \
  '9'

expect "index store -> namespace read" \
  'fn main() {\n    var m: HashMap<string, int> = {"k": 3}\n    m["k"] = 4\n    print("${HashMap.get(m, "k")}")\n}' \
  '4'

# --- 3. TWO MAPS WITH DIFFERENT VALUE TYPES IN ONE PROGRAM. The store teaches the
# map, so two maps written only through the namespace spelling must not share one
# answer (the #418 aliasing shape, asserted for this path).
expect "two ns-written maps keep separate value types" \
  'fn main() {\n    a = HashMap.new()\n    b = HashMap.new()\n    HashMap.set(a, "k", 5)\n    HashMap.set(b, "k", "five")\n    print("${HashMap.get(a, "k")} ${HashMap.get(b, "k")}")\n}' \
  '5 five'

# --- 4. A TYPED ANNOTATION reaches the same answer as an inferred one.
expect "annotated map, namespace store and read" \
  'fn main() {\n    var m: HashMap<string, int> = {}\n    HashMap.set(m, "k", 11)\n    print("${HashMap.get(m, "k")}")\n}' \
  '11'

# --- 5. has/remove/len on a non-string map still work - they take the KEY, which is
# a string either way, so they must be untouched by value-type dispatch.
expect "has/len on an int-valued map, namespace spelling" \
  'fn main() {\n    m = HashMap.new()\n    HashMap.set(m, "k", 1)\n    print("${HashMap.has(m, "k")} ${HashMap.len(m)}")\n}' \
  'true 1'

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
