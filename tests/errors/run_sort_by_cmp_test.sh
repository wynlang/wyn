#!/bin/bash
# `sort_by` with a COMPARATOR lambda - the missing half of sorting.
#
# THE DEFECT THIS GATES (measured on dev @ 675880a5):
#
#     gs.sort_by((x, y) => y.n - x.n)
#       -> Error: compilation failed (internal codegen error)
#       -> (WYN_DEBUG=1) "member reference base type 'long long' is not a
#          structure or union"  at  return (y.n - x.n);
#
# NOT an unknown method: the checker already knew `sort_by` and typed it as the
# receiver array, and the one-parameter KEY form (`xs.sort_by((p) => p.age)`)
# has worked since the week-one stdlib batch. What was missing was the
# two-parameter comparator form:
#
#   * the checker's lambda-parameter seed only fired for param_count == 1, so
#     both comparator parameters fell back to the `int` default and every field
#     access on them failed in the C compiler, and
#   * codegen routed anything that was not a one-param key fn to the legacy
#     `wyn_array_sort_by(WynArray*, long long(*)(long long, long long))`, which
#     can only compare the int slot of a WynValue - so a struct array could not
#     work even if the parameters HAD been typed.
#
# That is why every report-style program was blocked: `.sort()` is correct and
# complete for int and string arrays, and there was no way to order an array of
# structs by a field at all - nor to sort DESCENDING by anything, since a key
# function has no direction. V-19.
#
# HOW IT IS FIXED
# The comparator form is monomorphized exactly like the key form next to it:
# the comparator keeps its native C ABI (`long long (*)(G, G)`, or `double
# (*)(...)` when the comparator returns a float), and the sort moves whole
# WynValue slots. No void* boxing, and no second lambda-invocation mechanism -
# the legacy int-only runtime helper is no longer the comparator path.
#
# THE SORT IS STABLE, and this gate asserts it (the EQUAL-KEYS arm). It is an
# insertion sort that shifts only while the comparator answers strictly greater
# than zero, so elements the comparator calls equal keep their input order. That
# is a promise, not an accident: a report sorted by one column and then another
# is only correct if the second sort is stable.
#
# COMPARATOR CONTRACT (recorded here because it is a judgement call):
#   cmp(x, y) < 0  ->  x comes first        (C qsort / JS Array.sort order)
#   cmp(x, y) > 0  ->  y comes first
#   cmp(x, y) == 0 ->  input order is kept
# So descending by an int field is `(x, y) => y.n - x.n`, which is the spelling
# in the ticket and the one every other language's user would try first.
#
# NOTE FOR ANYONE RE-RUNNING THIS: this is pure codegen + checker, no runtime
# library change, so `make` alone is enough - but the last arms still run
# `wyn build` and `wyn run --release` because the emitted C differs between the
# full and slim runtime headers, and `wyn run`'s <file>.out cache key does NOT
# include --release (so the --release arm needs its own source file or it
# silently re-runs the non-release binary).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# --- the whole surface in one program, one tagged line per arm ---------------
cat > "$TMP/sorts.wyn" <<'WYN'
struct G { n: int, name: string }
struct Item { name: string, price: float }
struct S { k: int, tag: string }
struct P { name: string, age: int }

fn names(gs: [G]) -> string {
    var out = ""
    for g in gs { out = out + g.name }
    return out
}
fn desc_cmp(a: int, b: int) -> int { if a > b { return -1 } if a < b { return 1 } return 0 }

fn main() {
    // 1. the ticket's exact spelling: descending by an int field
    var gs = [G { n: 3, name: "c" }, G { n: 1, name: "a" }, G { n: 2, name: "b" }]
    gs.sort_by((x, y) => y.n - x.n)
    print("DESC ${names(gs)}")

    // 2. ascending by the same field - the comparator's sign is the direction
    var gs2 = [G { n: 3, name: "c" }, G { n: 1, name: "a" }, G { n: 2, name: "b" }]
    gs2.sort_by((x, y) => x.n - y.n)
    print("ASC ${names(gs2)}")

    // 3. a STRING field, ordered with the string comparison operators
    var gs3 = [G { n: 0, name: "c" }, G { n: 0, name: "a" }, G { n: 0, name: "b" }]
    gs3.sort_by((x, y) => if x.name < y.name { -1 } else { 1 })
    print("STR ${names(gs3)}")

    // 4. a FLOAT field, with a comparator that RETURNS a float (the difference,
    //    not a -1/0/1 sign) - the comparator's own return type has to survive
    var items = [Item { name: "b", price: 2.5 }, Item { name: "c", price: 9.75 }, Item { name: "a", price: 1.25 }]
    items.sort_by((p, q) => p.price - q.price)
    print("FLT ${items[0].name}${items[1].name}${items[2].name} ${items[0].price}")

    // 5. EQUAL KEYS: the sort is stable, so a, b and d keep their input order
    var ss = [S { k: 1, tag: "a" }, S { k: 1, tag: "b" }, S { k: 0, tag: "c" }, S { k: 1, tag: "d" }]
    ss.sort_by((p, q) => p.k - q.k)
    var st = ""
    for s in ss { st = st + s.tag }
    print("STABLE ${st}")

    // 6. plain int array with a comparator lambda (descending)
    var ns = [5, 1, 3, 2, 4]
    ns.sort_by((a, b) => b - a)
    print("INTS ${ns[0]}${ns[1]}${ns[2]}${ns[3]}${ns[4]}")

    // 7. plain string array with a comparator lambda
    var ws = ["c", "a", "b"]
    ws.sort_by((a, b) => if a < b { -1 } else { 1 })
    print("WORDS ${ws[0]}${ws[1]}${ws[2]}")

    // 8. BACK-COMPAT: a named two-argument comparator fn still sorts (this is
    //    what the legacy runtime helper was for; it must not regress)
    var legacy = [5, 1, 3, 2, 4]
    legacy.sort_by(desc_cmp)
    print("LEGACY ${legacy[0]}${legacy[4]}")

    // 9. NON-REGRESSION: the one-parameter KEY form is untouched
    var people = [P { name: "bob", age: 30 }, P { name: "al", age: 25 }, P { name: "cy", age: 41 }]
    people.sort_by((p) => p.age)
    print("KEYFN ${people[0].name}${people[1].name}${people[2].name}")

    // 10. a receiver that is not a named variable (no lvalue to sort in place):
    //     the sorted copy is the value of the expression
    print("TEMP ${[3, 1, 2].sort_by((a, b) => a - b)}")
}
WYN

out=$(cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" run sorts.wyn 2>&1); rc=$?
printf '%s\n' "$out" > "$TMP/sorts.out"
if [ "$rc" -eq 0 ]; then
    ok "the whole sort_by surface compiles and runs at exit 0"
else
    bad "sort_by surface must compile and run (rc=$rc)"
    sed -n '1,14p' "$TMP/sorts.out"
fi

field() { awk -v k="$1" 'index($0, k" ")==1 {print substr($0, length(k)+2); exit}' "$TMP/sorts.out"; }
expect() { # expect <tag> <expected> <why>
    local g; g=$(field "$1")
    if [ "$g" = "$2" ]; then ok "$3"
    else bad "$3 -- $1 want '$2' got '$g'"; fi
}

expect DESC   "cba"          'descending by an int field: gs.sort_by((x, y) => y.n - x.n) - the ticket verbatim'
expect ASC    "abc"          'ascending by the same field: the comparator sign IS the direction'
expect STR    "abc"          'a STRING field ordered with < through the comparator'
expect FLT    "abc 1.25"     'a FLOAT field, with a float-returning comparator (the raw difference)'
expect STABLE "cabd"         'EQUAL KEYS keep input order - the sort is STABLE (a,b,d stay in that order)'
expect INTS   "54321"        'a plain int array with a comparator lambda'
expect WORDS  "abc"          'a plain string array with a comparator lambda'
expect LEGACY "51"           'BACK-COMPAT: a named 2-arg comparator fn still sorts'
expect KEYFN  "albobcy"      'NON-REGRESSION: the 1-arg key form still sorts (al, bob, cy by age)'
expect TEMP   "[1, 2, 3]"    'a non-lvalue receiver yields the sorted copy as the expression value'

# --- the pre-fix failure mode, spelled out ----------------------------------
# Before the fix this exact program did not reach the sort at all: the C
# compiler rejected `y.n` because the comparator's parameters were typed
# `long long`. Assert that no diagnostic of that shape survives.
if printf '%s' "$out" | grep -q 'internal codegen error'; then
    bad "sort_by with a comparator must not produce an internal codegen error"
else
    ok "no internal codegen error (the pre-fix symptom) anywhere in the run"
fi
# WYN_DEBUG=1, because the underlying C diagnostic is HIDDEN without it - the
# plain run only prints "internal codegen error", so an arm that greps the normal
# output for this message would pass whether the parameters were typed or not.
dbg=$(cd "$TMP" && WYN_DEBUG=1 perl -e 'alarm(180); exec @ARGV' -- "$WYN" run sorts.wyn 2>&1)
if printf '%s' "$dbg" | grep -q 'is not a structure or union'; then
    bad "the comparator's parameters must be the ELEMENT type, not long long"
else
    ok "the comparator's parameters are typed as the element (no 'not a structure' error, WYN_DEBUG=1)"
fi

# --- wyn build and wyn run --release must agree with wyn run ----------------
cat > "$TMP/three.wyn" <<'WYN'
struct G { n: int, name: string }
fn main() {
    var gs = [G { n: 3, name: "c" }, G { n: 1, name: "a" }, G { n: 2, name: "b" }]
    gs.sort_by((x, y) => y.n - x.n)
    print("${gs[0].name}${gs[1].name}${gs[2].name}")
}
WYN
WANT3='cba'

out=$(cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" run three.wyn 2>&1); rc=$?
got=$(printf '%s' "$out" | grep -v 'Compiled in')
if [ "$rc" -eq 0 ] && [ "$got" = "$WANT3" ]; then ok "wyn run: $WANT3"
else bad "wyn run (rc=$rc, want '$WANT3', got '$got')"; fi

# Its OWN source file: `wyn run`'s <file>.out cache key does not include
# --release, so reusing three.wyn would re-run the non-release binary and this
# arm would prove nothing about the slim runtime header.
cp "$TMP/three.wyn" "$TMP/three_rel.wyn"
out=$(cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" run --release three_rel.wyn 2>&1); rc=$?
got=$(printf '%s' "$out" | grep -v 'Compiled in')
if [ "$rc" -eq 0 ] && [ "$got" = "$WANT3" ]; then ok "wyn run --release (slim runtime header) agrees"
else bad "wyn run --release (rc=$rc, want '$WANT3', got '$got')"; fi

out=$(cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" build three.wyn 2>&1); rc=$?
if [ "$rc" -ne 0 ]; then
    bad "wyn build succeeds (rc=$rc, out='$out')"
elif [ ! -x "$TMP/three" ]; then
    bad "wyn build produced no binary at $TMP/three"
else
    got=$(perl -e 'alarm(60); exec @ARGV' -- "$TMP/three" 2>&1)
    if [ "$got" = "$WANT3" ]; then ok "wyn build (prebuilt runtime lib) agrees"
    else bad "wyn build (want '$WANT3', got '$got')"; fi
fi

echo ""; echo "sort-by-cmp: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
