#!/usr/bin/env bash
# `mut self` on an impl method must actually mutate the receiver.
#
# THE DEFECT
#
#     impl Counter {
#         fn bump(mut self) { self.n = self.n + 1 }
#     }
#     c = Counter { n: 1 }
#     c.bump(); c.bump(); print(c.get())    -> 1, expected 3
#
# exit 0, `wyn check` clean. The parser recorded the modifier
# (fn.param_mutable) and the checker honoured it, so nothing upstream
# disagreed -- only codegen did, in three separate places:
#
#   1. the impl-method DEFINITION emitted `Counter_bump(Counter self)`, by
#      value. It was the only one of five parameter emitters that did not read
#      param_mutable;
#   2. its FORWARD DECLARATION had the same omission (a prototype that
#      disagrees with its definition is a C error, so both had to move);
#   3. the CALL SITE never took the address, so even with a pointer parameter
#      the caller passed a struct.
#
# A fourth, found while fixing it: the impl loop never called
# clear_parameters() between methods, so a `mut self` registered by one method
# stayed registered for the next -- making a non-mut `fn get(self)` emit
# `(*self).n` against a by-value parameter, which does not compile. The test
# below therefore always pairs a mut method with a non-mut one.
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

# The base case. `get` is deliberately NOT mut, so the parameter-registry leak
# described above would break the compile rather than pass quietly.
cat > basic.wyn <<'WYN'
struct Counter { n: int }

impl Counter {
    fn bump(mut self) {
        self.n = self.n + 1
    }
    fn get(self) -> int { return self.n }
}

fn main() -> int {
    var c = Counter { n: 1 }
    c.bump()
    c.bump()
    print(c.get())
    return 0
}
WYN
check "two mut-self calls both mutate the receiver" \
    "$("$WYN_ABS" run basic.wyn 2>/dev/null | tail -1)" "3"

# The definition, the prototype and the call site must all agree. Assert on the
# generated C so a future change that moves only one of the three is caught by
# cause rather than by a wrong number.
"$WYN_ABS" build basic.wyn --debug -o "$work/basic.bin" > /dev/null 2>&1
if [ -f basic.wyn.c ]; then
    check "the definition takes a pointer" \
        "$(grep -c 'Counter_bump(Counter \*self) {' basic.wyn.c)" "1"
    check "the prototype takes a pointer" \
        "$(grep -c 'Counter_bump(Counter \*self);' basic.wyn.c)" "1"
    check "the call site takes the address" \
        "$(grep -c 'Counter_bump(&c);' basic.wyn.c)" "2"
    # The NON-mut method must be untouched by all of the above.
    check "a non-mut method still takes its receiver by value" \
        "$(grep -c 'Counter_get(Counter self) {' basic.wyn.c)" "1"
    check "a non-mut method body does not dereference" \
        "$(grep -c 'return (\*self).n;' basic.wyn.c)" "0"
fi

# A mut method with additional parameters: the receiver is pointer, the rest are
# unchanged.
cat > args.wyn <<'WYN'
struct Acc { total: int }

impl Acc {
    fn add(mut self, k: int) {
        self.total = self.total + k
    }
    fn total_of(self) -> int { return self.total }
}

fn main() -> int {
    var a = Acc { total: 0 }
    a.add(5)
    a.add(7)
    print(a.total_of())
    return 0
}
WYN
check "a mut-self method with extra args mutates correctly" \
    "$("$WYN_ABS" run args.wyn 2>/dev/null | tail -1)" "12"

# Mutating a STRING field, which travels a different codegen path from int.
cat > strf.wyn <<'WYN'
struct Buf { s: string }

impl Buf {
    fn push(mut self, part: string) {
        self.s = self.s + part
    }
    fn value(self) -> string { return self.s }
}

fn main() -> int {
    var b = Buf { s: "a" }
    b.push("b")
    b.push("c")
    print(b.value())
    return 0
}
WYN
check "a mut-self method mutates a string field" \
    "$("$WYN_ABS" run strf.wyn 2>/dev/null | tail -1)" "abc"

# Two mut methods in one impl block, to catch a registry that is cleared once
# rather than per method.
cat > two.wyn <<'WYN'
struct Pt { x: int, y: int }

impl Pt {
    fn right(mut self) { self.x = self.x + 1 }
    fn up(mut self) { self.y = self.y + 2 }
    fn sum(self) -> int { return self.x + self.y }
}

fn main() -> int {
    var p = Pt { x: 0, y: 0 }
    p.right()
    p.up()
    p.right()
    print(p.sum())
    return 0
}
WYN
check "two mut methods in one impl block both work" \
    "$("$WYN_ABS" run two.wyn 2>/dev/null | tail -1)" "4"

# A non-mut method must NOT mutate -- the fix must not make every receiver a
# pointer, or `self` becomes silently mutable everywhere.
cat > nonmut.wyn <<'WYN'
struct V { n: int }

impl V {
    fn try_change(self) -> int {
        return self.n + 100
    }
    fn get(self) -> int { return self.n }
}

fn main() -> int {
    var v = V { n: 7 }
    print(v.try_change())
    print(v.get())
    return 0
}
WYN
out="$("$WYN_ABS" run nonmut.wyn 2>/dev/null)"
check "a non-mut method computes without mutating (returned)" \
    "$(echo "$out" | sed -n '1p')" "107"
check "a non-mut method leaves the receiver alone" \
    "$(echo "$out" | sed -n '2p')" "7"

echo
echo "mut self: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
