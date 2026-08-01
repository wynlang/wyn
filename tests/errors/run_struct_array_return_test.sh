#!/bin/bash
# A function may RETURN an array of structs (2026-08).
#
#     struct S { name: string }
#     fn make() -> [S] { ... }
#     for s in make() { print(s.name) }
#
# `wyn check` passed cleanly and the C compiler then rejected the program:
#
#     error: member reference base type 'long long' is not a structure or union
#
# The checker's STMT_FN return-type arm for `[T]` inlined a four-way
# int/string/float/bool chain, so a STRUCT or ENUM element name matched nothing and
# element_type was left NULL. Every consumer downstream then fell back to the int
# default and bound the loop variable as `long long`.
#
# Returning an array of records is the natural way to structure any program that has
# records in it - a parser returning tokens, a scanner returning hits, a config loader
# returning entries. Three separate sample apps hit this independently and each worked
# around it differently, one by inlining a 12-row table into main() to avoid a
# `-> [Rule]` return.
#
# WHY IT LOOKED LIKE SOMETHING ELSE: an explicit annotation worked -
#
#     var xs: [S] = make()      // fine
#     var xs = make()           // long long
#
# because the ANNOTATION path already called resolve_array_elem_annotation(). That
# asymmetry made the bug read as a rule about type annotations rather than a gap in
# the return-type resolver. The fix routes the return type through that same resolver,
# which also brings nested arrays and map elements along for free.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- the exact shape that failed -------------------------------------------

cat > "$TMP/a.wyn" <<'EOF'
struct S { name: string, n: int }

fn make() -> [S] {
    var xs = []
    xs.push(S { name: "a", n: 1 })
    xs.push(S { name: "b", n: 2 })
    return xs
}

fn main() {
    // iterate the call DIRECTLY - no intermediate variable to carry a type
    for s in make() { print("direct ${s.name}=${s.n}") }
    // and through an unannotated variable, which is the spelling people write
    var ys = make()
    for s in ys { print("var ${s.name}=${s.n}") }
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" build a.wyn 2>&1); code=$?
if [ $code -eq 0 ]; then
  ok "a function returning [Struct] BUILDS"
else
  bad "build failed"; echo "$out" | grep -E 'error:' | head -3 | sed 's/^/        /'
fi
if echo "$out" | grep -q "is not a structure or union"; then
  bad "the loop variable was bound as long long again"
else
  ok "no 'not a structure or union' error"
fi

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='direct a=1
direct b=2
var a=1
var b=2'
got=$(printf '%s' "$out" | grep -E '^(direct|var) ')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "fields read correctly, both directly and through a variable"
else
  bad "wrong output"; printf '%s\n' "$got" | head -5 | sed 's/^/        /'
fi

# The failure mode named specifically: an empty/zero field read is what a
# long long-bound loop variable produces, so assert the values are really there.
if printf '%s' "$out" | grep -qE '^(direct|var) =' ; then
  bad "a name came back EMPTY - the element type is unresolved again"
else
  ok "no empty struct fields"
fi

# ---- an ENUM element, the other type the old chain could not name ----------
cat > "$TMP/b.wyn" <<'EOF'
enum Level { Info, Warn, Error }

fn levels() -> [Level] {
    var xs = []
    xs.push(Level.Warn)
    xs.push(Level.Error)
    return xs
}

fn tag(l: Level) -> string => match l {
    Level.Info => "INFO",
    Level.Warn => "WARN",
    Level.Error => "ERROR"
}

fn main() {
    for l in levels() { print("lvl ${tag(l)}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^lvl WARN$' &&
   printf '%s' "$out" | grep -q '^lvl ERROR$'; then
  ok "a function returning [Enum] works, and match on the element resolves"
else
  bad "enum-element case failed"; printf '%s\n' "$out" | head -4 | sed 's/^/        /'
fi

# ---- passed onward, and used with array methods ----------------------------
# The resolved element type must survive being handed to another function and into
# the array methods, not just the for-loop that first exposed it.
cat > "$TMP/c.wyn" <<'EOF'
struct Hit { file: string, line: int }

fn scan() -> [Hit] {
    var hs = []
    hs.push(Hit { file: "a.wyn", line: 10 })
    hs.push(Hit { file: "b.wyn", line: 3 })
    hs.push(Hit { file: "c.wyn", line: 7 })
    return hs
}

fn report(hs: [Hit]) -> int {
    var total = 0
    for h in hs { total += h.line }
    return total
}

fn main() {
    var hs = scan()
    print("count ${hs.len()}")
    print("total ${report(hs)}")
    print("total-direct ${report(scan())}")
    var sorted = hs.sort_by((h) => h.line)
    for h in sorted { print("sorted ${h.file}:${h.line}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^count 3$' &&
   printf '%s' "$out" | grep -q '^total 20$' &&
   printf '%s' "$out" | grep -q '^total-direct 20$' &&
   printf '%s' "$out" | grep -q '^sorted b.wyn:3$'; then
  ok "the returned array flows into another fn and into sort_by"
else
  bad "downstream use failed"; printf '%s\n' "$out" | head -7 | sed 's/^/        /'
fi

# ---- and the element types that ALREADY worked are unchanged ---------------
# The fix REPLACES a four-way builtin chain with the shared resolver, so the builtins
# are the control group: if the resolver disagreed with the old chain on any of them,
# this catches it.
#
# `[bool]` is deliberately NOT asserted here. `for b in [true, false] { if b {...} }`
# counts zero for an INLINE array literal too, with or without this change, so it is a
# separate pre-existing defect in how bool elements are bound - not something this
# return-type fix caused or can fix. Asserting it here would make this test fail for a
# reason it does not own.
cat > "$TMP/d.wyn" <<'EOF'
fn ints() -> [int] { return [1, 2, 3] }
fn strs() -> [string] { return ["a", "b"] }
fn floats() -> [float] { return [1.5, 2.5] }

fn main() {
    var si = 0
    for i in ints() { si += i }
    print("ints ${si}")
    var ss = ""
    for s in strs() { ss = ss + s }
    print("strs ${ss}")
    var sf = 0.0
    for f in floats() { sf = sf + f }
    print("floats ${sf}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^ints 6$' &&
   printf '%s' "$out" | grep -q '^strs ab$' &&
   printf '%s' "$out" | grep -qE '^floats 4(\.0)?$'; then
  ok "[int] [string] [float] returns all behave exactly as before"
else
  bad "a builtin element type regressed"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

# The explicit-annotation spelling that people used as the WORKAROUND must keep
# working - it went through the resolver already and must not double-resolve.
cat > "$TMP/e.wyn" <<'EOF'
struct S { name: string }
fn make() -> [S] {
    var xs = []
    xs.push(S { name: "annotated" })
    return xs
}
fn main() {
    var xs: [S] = make()
    for s in xs { print("still ${s.name}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run e.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^still annotated$'; then
  ok "the explicit [S] annotation workaround still works"
else
  bad "annotated spelling broke"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "struct-array-return: $PASS pass, 0 fail"
  exit 0
fi
echo "struct-array-return: $PASS pass, $FAIL fail"
exit 1
