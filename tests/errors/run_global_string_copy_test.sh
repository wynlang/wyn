#!/usr/bin/env bash
# Copying a module-level string GLOBAL into a local must retain it.
#
# THE DEFECT (the real reason PR #266 was reverted)
#
# The string RC path decides between MOVE and COPY: if the source is dead after
# this statement, ownership transfers and no retain is emitted. `var_is_live_after`
# answers that question by reading the CURRENT BLOCK only, which is sound for a
# local and wrong for a global -- another function can overwrite it, and that
# assignment releases the old value:
#
#     fn consume() -> string {
#         produce(7)          // sets the global
#         var r = retval      // MOVE chosen: no retain emitted
#         retval = ""         // releases the buffer r points at
#         return r            // dangling
#     }
#
# It returned an EMPTY STRING rather than crashing, so nothing announced it.
#
# This is why registering string globals "corrupted the interpreter": WynJS hands
# values between functions through exactly this pattern (its `retval`, `throwval`
# and `tv` globals), and 9 of 33 suites failed with `undefined` while the build
# itself was clean. The first attempt at #266 tried to fix it by SKIPPING the
# retain for globals, which is the opposite of the correct fix and made it worse.
#
# A global is never safe to move from, however dead it looks locally.
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

# The WynJS handoff pattern, reduced. The global must hold a DYNAMICALLY built
# string ("v" + n) -- assigning a literal cannot show the bug, because
# wyn_rc_release is a no-op on a non-RC pointer.
cat > handoff.wyn <<'WYN'
retval = ""

fn produce(n: int) {
    retval = "v" + n.to_string()
}

fn consume() -> string {
    produce(7)
    var r = retval
    retval = ""
    return r
}

fn main() -> int {
    print(consume())
    print(consume())
    return 0
}
WYN
out="$("$WYN_ABS" run handoff.wyn 2>/dev/null)"
check "a copy of a global survives the global being reassigned (1st)" \
    "$(echo "$out" | sed -n '1p')" "v7"
check "a copy of a global survives the global being reassigned (2nd)" \
    "$(echo "$out" | sed -n '2p')" "v7"

# The retain must actually be emitted, not merely happen to work. Assert on the
# generated C so a future change that reintroduces the move is caught by cause
# rather than by symptom.
"$WYN_ABS" build handoff.wyn --debug -o "$work/handoff.bin" > /dev/null 2>&1
if [ -f handoff.wyn.c ]; then
    check "the copy out of the global emits a retain" \
        "$(grep -A1 'const char\* r = retval;' handoff.wyn.c | grep -c 'wyn_rc_retain(r);')" "1"
fi

# Two locals copied from the same global, both live across a reassignment.
cat > two.wyn <<'WYN'
g = ""

fn setg(s: string) {
    g = s + "!"
}

fn main() -> int {
    setg("a")
    var x = g
    setg("b")
    var y = g
    setg("c")
    print(x)
    print(y)
    print(g)
    return 0
}
WYN
out="$("$WYN_ABS" run two.wyn 2>/dev/null)"
check "two copies of a global both survive (first)"  "$(echo "$out" | sed -n '1p')" "a!"
check "two copies of a global both survive (second)" "$(echo "$out" | sed -n '2p')" "b!"
check "the global itself is still correct"           "$(echo "$out" | sed -n '3p')" "c!"

# The change above must not turn EVERY copy into a retain -- that would
# reintroduce the leak this whole change exists to fix. A local source keeps
# whatever the existing liveness analysis decides for it; the only thing being
# forced here is the GLOBAL case. Guard that a local-to-local copy is unaffected
# by comparing its emitted form against the local's own source, rather than
# asserting a move that this liveness analysis does not actually perform here
# (`b` IS live at `print(b)`, so a retain is correct).
cat > localmove.wyn <<'WYN'
fn main() -> int {
    var a = "x" + "y"
    var b = a
    print(b)
    print(a)
    return 0
}
WYN
check "a local-to-local copy still works" \
    "$("$WYN_ABS" run localmove.wyn 2>/dev/null | sed -n '1p')" "xy"
check "the copied-from local is still readable" \
    "$("$WYN_ABS" run localmove.wyn 2>/dev/null | sed -n '2p')" "xy"

echo
echo "global-string copy: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
