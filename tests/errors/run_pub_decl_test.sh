#!/usr/bin/env bash
# `pub const`, `pub var` and `pub impl` must parse.
#
# THE DEFECT
#
# The top-level `pub` branch in parser.c handled only `pub struct` and `pub enum`
# before falling through to function(), which then rejected everything else with
# "Expected 'fn'". All three of the forms below are in the language spec:
#
#     pub const MAX = 100        -> Error: Expected 'fn'
#     pub var counter = 5        -> Error: Expected 'fn'
#     pub impl P { ... }         -> Error: Expected 'fn'
#
# `pub impl` is the one that mattered most: without it, impl-method syntax was
# unavailable to library code entirely, because a module wanting to export
# methods could not write the block at all.
#
# There was also a reporting bug behind it. In a MODULE the parse error was
# printed to stderr while `wyn check` still exited saying "no errors" -- so the
# module silently contributed nothing and the failure surfaced later as a missing
# symbol. The last assertion below pins that down: a valid `pub const` in a
# module must be readable by the importer, which it cannot be if the module fails
# to parse.
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

cat > c.wyn <<'WYN'
pub const MAX = 100
fn main() -> int {
    print(MAX)
    return 0
}
WYN
check "pub const parses and its value is readable" \
    "$("$WYN_ABS" run c.wyn 2>/dev/null | tail -1)" "100"

cat > v.wyn <<'WYN'
pub var counter = 5
fn main() -> int {
    print(counter)
    return 0
}
WYN
check "pub var parses and its value is readable" \
    "$("$WYN_ABS" run v.wyn 2>/dev/null | tail -1)" "5"

cat > i.wyn <<'WYN'
struct P { x: int }
pub impl P {
    fn get(self) -> int { return self.x }
}
fn main() -> int {
    var p = P { x: 4 }
    print(p.get())
    return 0
}
WYN
check "pub impl parses and its method is callable" \
    "$("$WYN_ABS" run i.wyn 2>/dev/null | tail -1)" "4"

# The module case, which is the reason these forms exist: a `pub const` in a
# module must be reachable as mod.NAME. This also covers the reporting bug --
# the module used to fail to parse while `wyn check` said "no errors".
cat > src/cfg.wyn <<'WYN'
pub const LIMIT = 42
pub var seen = 7
WYN
cat > m.wyn <<'WYN'
import cfg
fn main() -> int {
    print(cfg.LIMIT)
    return 0
}
WYN
check "a module's pub const is readable by the importer" \
    "$("$WYN_ABS" run m.wyn 2>/dev/null | tail -1)" "42"

# A module carrying these forms must type-check cleanly, not merely run.
check "a module with pub const/var type-checks" \
    "$("$WYN_ABS" check m.wyn 2>&1 | grep -ciE "expected 'fn'|parse error")" "0"

# Guard the forms that already worked, so this branch cannot regress them.
cat > s.wyn <<'WYN'
pub struct Q { y: int }
pub enum E { A, B }
pub fn twice(n: int) -> int { return n * 2 }
fn main() -> int {
    var q = Q { y: 3 }
    print(twice(q.y))
    return 0
}
WYN
check "pub struct / pub enum / pub fn still parse" \
    "$("$WYN_ABS" run s.wyn 2>/dev/null | tail -1)" "6"

echo
echo "pub declarations: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
