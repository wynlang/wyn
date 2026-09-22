#!/usr/bin/env bash
# `spawn` on a closure must be a CHECK error, not a silent 0.
#
# THE DEFECT (measured on dev @ 11637e24, 2026-09-22)
#
#     var n = 5
#     g = (() => n * 2)
#     print(g())          // 10   - the closure itself is fine
#     f = spawn (() => n * 2)
#     print(await f)      // 0    - check clean, build clean, exit 0, WRONG ANSWER
#
# The worst class of bug there is: nothing anywhere says the program is wrong.
# `spawn` hands the scheduler a function pointer, and a closure's captured
# environment is not carried across the task boundary, so the task reads zeroes.
#
# Measured, not assumed: a NON-capturing lambda (`spawn (() => 42)`) returns 0 too,
# and so does the immediately-invoked form, so the rule covers a lambda at a spawn
# site whether it captures or not. The fire-and-forget statement form
# (`spawn (() => print(...))`) died with "Internal codegen error: lambda at line 0
# was never registered" - loud, but still no use to the reader; it now gets the same
# clean message.
#
# NOT IN SCOPE: making closures genuinely spawnable. That needs the captured env
# boxed and refcounted across the boundary (internal-docs/PLAN_v1.22.md §5 lists it
# as out of scope for v1.22). This ships the clean error and the workaround.
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

# ---------------------------------------------------------------------------
# REJECTED: every spelling of "spawn a lambda".
# ---------------------------------------------------------------------------
cat > capture.wyn <<'EOF'
fn main() {
    var n = 5
    f = spawn (() => n * 2)
    print(await f)
}
EOF
out="$("$WYN_ABS" check capture.wyn 2>&1)"; rc=$?
check "a capturing closure is rejected at check time" \
    "$([ "$rc" -ne 0 ] && echo yes || echo no)" "yes"
check "the message names spawn and the closure" \
    "$(echo "$out" | grep -c "spawn.*cannot run a closure")" "1"
check "the message names the line"        "$(echo "$out" | grep -c "Error at line 3")" "1"
check "the message names the workaround"  "$(echo "$out" | grep -c "named function")" "1"
check "the message shows the rewrite"     "$(echo "$out" | grep -c "spawn work(n)")"   "1"

# A lambda with NO captures is just as broken (it returned 0 as well), so the rule
# must not be conditional on captured_count.
cat > nocapture.wyn <<'EOF'
fn main() {
    f = spawn (() => 42)
    print(await f)
}
EOF
"$WYN_ABS" check nocapture.wyn > /dev/null 2>&1
check "a NON-capturing lambda is rejected too" \
    "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

# Immediately invoked: `spawn (() => ...)()` is a call whose callee is the lambda.
cat > iife.wyn <<'EOF'
fn main() {
    var n = 5
    f = spawn (() => n * 2)()
    print(await f)
}
EOF
"$WYN_ABS" check iife.wyn > /dev/null 2>&1
check "an immediately-invoked lambda is rejected" \
    "$([ $? -ne 0 ] && echo yes || echo no)" "yes"

# Fire-and-forget statement form: same construct, same message.
cat > ff_lambda.wyn <<'EOF'
fn main() {
    var n = 5
    spawn (() => print("${n}"))
    print("done")
}
EOF
out="$("$WYN_ABS" check ff_lambda.wyn 2>&1)"; rc=$?
check "fire-and-forget spawn of a lambda is rejected" \
    "$([ "$rc" -ne 0 ] && echo yes || echo no)" "yes"
check "and gets the same message" \
    "$(echo "$out" | grep -c "cannot run a closure")" "1"
# Through `wyn run`, not `wyn check`: what this replaces is a CODEGEN failure
# ("Internal codegen error: lambda at line 0 was never registered"), and `check`
# never reaches codegen - so asserting its absence at check time proves nothing.
out="$("$WYN_ABS" run ff_lambda.wyn 2>&1)"
check "run says the same thing, not an internal codegen error" \
    "$(echo "$out" | grep -c "cannot run a closure")" "1"
check "no internal codegen error leaks" \
    "$(echo "$out" | grep -ci "internal codegen error")" "0"

# ---------------------------------------------------------------------------
# NOT REJECTED: every spawn form that works today. Pinned BY VALUE where the
# value is right - an error that fires on `spawn work()` would be far worse than
# the bug it replaces.
# ---------------------------------------------------------------------------
cat > named.wyn <<'EOF'
fn work() -> int {
    return 21 * 2
}
fn main() {
    f = spawn work()
    print(await f)
}
EOF
check "spawn of a named function still runs, correctly" \
    "$("$WYN_ABS" run named.wyn 2>/dev/null | tail -1)" "42"

cat > named_args.wyn <<'EOF'
fn double(n: int) -> int {
    return n * 2
}
fn main() {
    var n = 5
    f = spawn double(n)
    print(await f)
}
EOF
# This is the workaround the diagnostic tells the user to write, so it must work.
check "the workaround the message recommends actually works" \
    "$("$WYN_ABS" run named_args.wyn 2>/dev/null | tail -1)" "10"

cat > ff_named.wyn <<'EOF'
fn hello() {
    print("hi")
}
fn main() {
    spawn hello()
    Time.sleep(150)
    print("done")
}
EOF
check "fire-and-forget spawn of a named function still runs" \
    "$("$WYN_ABS" run ff_named.wyn 2>/dev/null | tr '\n' '|' | sed 's/|$//')" "hi|done"

# spawn of a METHOD must keep checking. Note: it returns 0 rather than the method's
# value - a separate, pre-existing defect of the same class, filed and deliberately
# not touched here (this ticket is the closure site). Pinned at `check` only, so
# fixing that later does not have to edit this arm.
cat > method.wyn <<'EOF'
struct W {
    base: int
    fn calc(self) -> int { return self.base * 2 }
}
fn main() {
    var w = W { base: 21 }
    f = spawn w.calc()
    print(await f)
}
EOF
"$WYN_ABS" check method.wyn > /dev/null 2>&1
check "spawn of a method still checks clean" \
    "$([ $? -eq 0 ] && echo yes || echo no)" "yes"

cat > awaitall.wyn <<'EOF'
fn a() -> int { return 1 }
fn b() -> int { return 2 }
fn main() {
    var rs = await_all([spawn a(), spawn b()])
    print("${rs[0]}|${rs[1]}")
}
EOF
check "await_all over spawned functions still runs, correctly" \
    "$("$WYN_ABS" run awaitall.wyn 2>/dev/null | tail -1)" "1|2"

# `parallel { }` bodies capture enclosing variables and are NOT spawn sites - the
# rule must not reach them.
cat > par.wyn <<'EOF'
fn main() {
    var n = 5
    parallel {
        print("p ${n}")
    }
    print("after")
}
EOF
check "parallel { } with a captured variable still runs" \
    "$("$WYN_ABS" run par.wyn 2>/dev/null | tr '\n' '|' | sed 's/|$//')" "p 5|after"

# And an ordinary closure, away from spawn, is untouched.
cat > closure.wyn <<'EOF'
fn main() {
    var n = 5
    g = (() => n * 2)
    print(g())
}
EOF
check "a closure NOT at a spawn site still runs, correctly" \
    "$("$WYN_ABS" run closure.wyn 2>/dev/null | tail -1)" "10"

# A lambda passed to a higher-order function must stay legal too.
cat > hof.wyn <<'EOF'
fn main() {
    var n = 3
    var xs = [1, 2, 3]
    var ys = xs.map((x) => x * n)
    print("${ys[2]}")
}
EOF
check "a lambda argument to .map() still runs, correctly" \
    "$("$WYN_ABS" run hof.wyn 2>/dev/null | tail -1)" "9"

echo ""
echo "spawn-closure: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
