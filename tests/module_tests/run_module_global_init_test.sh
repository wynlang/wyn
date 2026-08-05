#!/usr/bin/env bash
# A module-level global must keep its initializer.
#
# THE DEFECT
#
# One Wyn `var` became TWO C globals. merge_module_exports splices the module's
# statement into the importer's program, which emits an UNPREFIXED copy that does
# get its initializer (in __wyn_init_globals). The module's own emission then
# emits a PREFIXED copy (`store_items`) -- and that is the one the module's
# function bodies actually reference, because identifiers inside a module fn are
# prefixed with the module name. For an array literal or a HashMap.new() the
# prefixed copy was DECLARED AND NEVER INITIALIZED: the initializer was simply
# dropped, because neither is a constant expression that can be written at file
# scope and there was no module constructor to put it in.
#
#     src/store.wyn:  var items = ["seed"]
#                     pub fn add(s: string) { items.push(s) }
#                     pub fn count() -> int { return items.len() }
#     main.wyn:       store.add("b"); print(store.count())   -> 1, want 2
#
# `wyn check` was clean. With an array of STRUCTS it segfaulted instead, because
# count() read element 0 of a zeroed WynArray. With HashMap.new() it returned 0.
#
# WHY IT MATTERS BEYOND THE WRONG NUMBER
#
# This is one of the defects that forced WynCanvas's data model into parallel
# array columns plus one-line accessor functions: a module could not own a
# populated collection at all.
#
# A SECOND NOTE ON NAMES: a driver file must not share its basename with the
# module it imports (`st.wyn` importing `src/st.wyn`), or resolution finds the
# driver first and reports "Circular import detected: st -> st". Hence use_*.wyn.
#
# A NOTE ON CHOOSING NAMES IN THIS TEST
#
# The identifier-prefixing heuristic has a bail-out list of common local names
# ("i", "j", "m", "p", "q", "s", "t", "content", "path", ...). A module global
# whose name is on that list is NOT prefixed, so it reaches the initialized
# unprefixed copy and works BY ACCIDENT. Every global here is deliberately named
# off that list -- `items`, `rows`, `cache` -- or the test would pass against the
# unfixed compiler and prove nothing.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"

pass=0
fail=0
check() {
    if [ "$2" = "$3" ]; then
        echo "  ok    $1"
        pass=$((pass+1))
    else
        echo "  FAIL  $1"
        echo "          expected: $3"
        echo "          actual:   $2"
        fail=$((fail+1))
    fi
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1
mkdir -p src

# 1. An array literal: the reported case.
cat > src/store.wyn <<'WYN'
var items = ["seed"]

pub fn add(s: string) {
    items.push(s)
}
pub fn count() -> int {
    return items.len()
}
pub fn at(i: int) -> string {
    return items[i]
}
WYN
cat > arr.wyn <<'WYN'
import store
fn main() -> int {
    store.add("b")
    print(store.count())
    print(store.at(0))
    print(store.at(1))
    return 0
}
WYN
out="$("$WYN_ABS" run arr.wyn 2>/dev/null)"
check "a module array global keeps its initializer (length)" \
    "$(echo "$out" | sed -n '1p')" "2"
check "the initializer element is the FIRST element" \
    "$(echo "$out" | sed -n '2p')" "seed"
check "the pushed element follows it" \
    "$(echo "$out" | sed -n '3p')" "b"

# 2. An array of STRUCTS, which segfaulted rather than answering wrongly.
cat > src/st.wyn <<'WYN'
pub struct P { x: int, name: string }
var rows = [P { x: 1, name: "seed" }]

pub fn count() -> int { return rows.len() }
pub fn first_name() -> string {
    var e = rows[0]
    return e.name
}
WYN
cat > use_st.wyn <<'WYN'
import st
fn main() -> int {
    print(st.count())
    print(st.first_name())
    return 0
}
WYN
out="$("$WYN_ABS" run use_st.wyn 2>/dev/null)"
check "a module array-of-structs global is populated (no segfault)" \
    "$(echo "$out" | sed -n '1p')" "1"
check "its struct field is readable across the boundary" \
    "$(echo "$out" | sed -n '2p')" "seed"

# 3. HashMap.new(), the other non-constant initializer on that code path.
cat > src/mp.wyn <<'WYN'
var cache = HashMap.new()
pub fn put(k: string, v: int) { cache.set(k, v) }
pub fn size() -> int { return cache.len() }
WYN
cat > use_mp.wyn <<'WYN'
import mp
fn main() -> int {
    mp.put("a", 1)
    mp.put("b", 2)
    print(mp.size())
    return 0
}
WYN
check "a module HashMap global is constructed" \
    "$("$WYN_ABS" run use_mp.wyn 2>/dev/null | tail -1)" "2"

# 4. An empty array literal must still work -- the constructor must not assume
#    there is something to push.
cat > src/emptymod.wyn <<'WYN'
var queue = []
pub fn push_one(n: int) { queue.push(n) }
pub fn depth() -> int { return queue.len() }
WYN
cat > empty.wyn <<'WYN'
import emptymod
fn main() -> int {
    print(emptymod.depth())
    emptymod.push_one(9)
    print(emptymod.depth())
    return 0
}
WYN
out="$("$WYN_ABS" run empty.wyn 2>/dev/null)"
check "an empty module array global starts empty" "$(echo "$out" | sed -n '1p')" "0"
check "and is still usable"                       "$(echo "$out" | sed -n '2p')" "1"

# 5. Scalar and string module globals already worked (they ARE constant
#    expressions and were emitted inline). Guard them, since the branch above
#    sits alongside theirs.
cat > src/scal.wyn <<'WYN'
var label = "hello"
var total = 41
pub fn bump() { total = total + 1 }
pub fn report() -> string { return label }
pub fn value() -> int { return total }
WYN
cat > use_scal.wyn <<'WYN'
import scal
fn main() -> int {
    scal.bump()
    print(scal.report())
    print(scal.value())
    return 0
}
WYN
out="$("$WYN_ABS" run use_scal.wyn 2>/dev/null)"
check "a module string global is unaffected" "$(echo "$out" | sed -n '1p')" "hello"
check "a module int global is unaffected"    "$(echo "$out" | sed -n '2p')" "42"

echo
echo "module global init: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
