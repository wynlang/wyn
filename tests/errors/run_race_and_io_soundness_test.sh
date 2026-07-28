#!/bin/bash
# Soundness gates for three "the compiler said it was fine and it wasn't" defects.
#
# 1. Shared mutable globals written from concurrent code (data race).
#    A plain global lowers to an unsynchronized C global; concurrent
#    read-modify-writes lose updates, so the program printed a WRONG NUMBER and
#    exited 0. Now a check-time error that points at Shared/channels. The
#    false-positive cases (global touched only by main, read-only global, local/
#    param shadowing, no spawn at all) must still compile.
#
# 2. `wyn check` must be a sound gate for annotated `[int]` arrays: the packed
#    WynIntArray representation only supports push/sort/len/index/iterate, so
#    any other use previously produced C that mixed it with generic WynArray
#    helpers - check passed, codegen failed. (Positive behavioral coverage is in
#    tests/regression/test_annotated_int_array_ops.wyn; here we assert the
#    check-then-build contract holds end to end.)
#
# 3. File.read_line() called free() on an RC-offset pointer on the EOF path, so
#    every read-to-EOF loop corrupted the heap and aborted (SIGABRT/rc=134).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect_error <name> <file> <grep-pattern> - check must REJECT
expect_error() {
    local name="$1" file="$2" pat="$3" out rc
    out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 1 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$out]"; fi
}

# expect_ok <name> <file> - check must ACCEPT (false-positive guard)
expect_ok() {
    local name="$1" file="$2" out rc
    out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then ok "$name"; else bad "$name (rc=$rc) [$out]"; fi
}

# expect_runs <name> <file> <expected-stdout> - check passes AND builds AND runs
# with the given output. This is the actual `wyn check` soundness contract: a
# program check accepts must compile and produce the right answer.
expect_runs() {
    local name="$1" file="$2" want="$3" out rc bin
    out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -ne 0 ]; then bad "$name (check rejected, rc=$rc) [$out]"; return; fi
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" build "$file" 2>&1); rc=$?
    if [ $rc -ne 0 ] || echo "$out" | grep -q "Build failed"; then
        bad "$name (check passed but CODEGEN FAILED) [$out]"; return; fi
    bin="${file%.wyn}"
    out=$(perl -e 'alarm(30); exec @ARGV' -- "$bin" 2>&1); rc=$?
    if [ $rc -ne 0 ]; then bad "$name (ran rc=$rc) [$out]"; return; fi
    if [ "$out" = "$want" ]; then ok "$name"
    else bad "$name (wrong answer: want [$want] got [$out])"; fi
}

echo "-- 1. shared mutable global data race --"

# The reproducer: 4 tasks each incrementing a plain global. Used to print
# 7947/7953/7957/7963 instead of 8000, at exit 0.
cat > "$TMP/race.wyn" <<'EOF'
var total = 0
fn bump(n: int) -> int {
    for i in 0..2000 { total = total + 1; Time::sleep(0) }
    return total
}
fn main() {
    a = spawn bump(1); b = spawn bump(2)
    r = await a + await b
    print(total)
}
EOF
expect_error "spawned fn writing a mutable global is rejected" "$TMP/race.wyn" "data race"

# The same defect without a yield point - it "looked fine" on every run, which
# is exactly why a runtime-only posture is not enough.
cat > "$TMP/race_noyield.wyn" <<'EOF'
var total = 0
fn bump(n: int) -> int {
    for i in 0..100000 { total = total + 1 }
    return total
}
fn main() {
    a = spawn bump(1); b = spawn bump(2)
    r = await a + await b
    print(total)
}
EOF
expect_error "race with no yield point is still rejected" "$TMP/race_noyield.wyn" "data race"

# parallel { } bodies run concurrently even without the `spawn` keyword.
cat > "$TMP/race_parallel.wyn" <<'EOF'
var total = 0
fn bump(n: int) -> int { total = total + n; return total }
fn main() {
    parallel {
        a = bump(1)
        b = bump(2)
    }
    print(total)
}
EOF
expect_error "parallel{} implicit spawn is rejected" "$TMP/race_parallel.wyn" "data race"

# Reachability is transitive: spawn -> outer -> inner writes the global.
cat > "$TMP/race_transitive.wyn" <<'EOF'
var total = 0
fn inner(n: int) -> int { total = total + n; return total }
fn outer(n: int) -> int { return inner(n) }
fn main() {
    a = spawn outer(1); b = spawn outer(2)
    print(await a + await b)
}
EOF
expect_error "transitively-reachable write is rejected" "$TMP/race_transitive.wyn" "data race"

# The error must name the fix, not just the problem.
out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" check "$TMP/race.wyn" 2>&1)
if echo "$out" | grep -q "Shared.add"; then ok "diagnostic suggests Shared"
else bad "diagnostic suggests Shared [$out]"; fi

echo "-- 1b. false-positive guards (these MUST still compile) --"

# Only main touches it - main is the single root thread.
cat > "$TMP/ok_main.wyn" <<'EOF'
var total = 0
fn main() {
    total = total + 1
    total = total + 41
    print(total)
}
EOF
expect_ok "global written only from main" "$TMP/ok_main.wyn"

# Written by main BEFORE the fan-out, then only read by the tasks.
cat > "$TMP/ok_before.wyn" <<'EOF'
var config = 0
fn work(n: int) -> int { return n + config }
fn main() {
    config = 10
    a = spawn work(1); b = spawn work(2)
    print(await a + await b)
}
EOF
expect_ok "global written before spawn, read after" "$TMP/ok_before.wyn"

# Read-only sharing is safe.
cat > "$TMP/ok_read.wyn" <<'EOF'
var config = 7
fn work(n: int) -> int { return n * config }
fn main() {
    a = spawn work(1); b = spawn work(2)
    print(await a + await b)
}
EOF
expect_ok "global only read by spawned fn" "$TMP/ok_read.wyn"

# A local of the same name is a different variable.
cat > "$TMP/ok_shadow.wyn" <<'EOF'
var total = 0
fn work(n: int) -> int {
    var total = 0
    for i in 0..10 { total = total + n }
    return total
}
fn main() {
    a = spawn work(1); b = spawn work(2)
    print(await a + await b)
}
EOF
expect_ok "local shadowing the global" "$TMP/ok_shadow.wyn"

# So is a parameter of the same name.
cat > "$TMP/ok_param.wyn" <<'EOF'
var total = 0
fn work(total: int) -> int { total = total + 1; return total }
fn main() {
    a = spawn work(1); b = spawn work(2)
    print(await a + await b)
}
EOF
expect_ok "param shadowing the global" "$TMP/ok_param.wyn"

# No concurrency anywhere: a global counter is perfectly fine.
cat > "$TMP/ok_seq.wyn" <<'EOF'
var total = 0
fn bump(n: int) -> int { total = total + n; return total }
fn main() {
    print(bump(1))
    print(bump(2))
}
EOF
expect_ok "sequential program with a global counter" "$TMP/ok_seq.wyn"

# And the recommended migration must actually be race-free and correct.
cat > "$TMP/ok_shared.wyn" <<'EOF'
total = Shared.new(0)
fn bump(n: int) -> int {
    for i in 0..2000 { Shared.add(total, 1); Time::sleep(0) }
    return 0
}
fn main() {
    a = spawn bump(1); b = spawn bump(2)
    r = await a + await b
    print(Shared.get(total))
}
EOF
expect_runs "Shared migration is accepted and correct" "$TMP/ok_shared.wyn" "4000"

echo "-- 2. annotated [int] check-then-codegen soundness --"

# The original reproducer: the annotation (not .sort()) was the trigger.
cat > "$TMP/ann_sort.wyn" <<'EOF'
fn main() {
    var arr: [int] = [3, 1, 2]
    var s = arr.sort()
    print(s[0])
}
EOF
expect_runs "annotated [int] .sort() value" "$TMP/ann_sort.wyn" "1"

# Indexed assignment used to COMPILE and write the wrong slot (silent).
cat > "$TMP/ann_idx.wyn" <<'EOF'
fn main() {
    var arr: [int] = [3, 1, 2]
    arr[0] = 9
    print(arr[0])
    print(arr[1])
}
EOF
expect_runs "annotated [int] indexed assign" "$TMP/ann_idx.wyn" "9
1"

# Passing an annotated [int] to a fn taking [int].
cat > "$TMP/ann_pass.wyn" <<'EOF'
fn take(a: [int]) -> int { return a.len() }
fn main() {
    var arr: [int] = [3, 1, 2]
    print(take(arr))
}
EOF
expect_runs "annotated [int] passed to a function" "$TMP/ann_pass.wyn" "3"

# A representative spread of the other formerly-broken operations.
cat > "$TMP/ann_ops.wyn" <<'EOF'
fn main() {
    var a: [int] = [3, 1, 2]
    print(a.sum())
    var b: [int] = [3, 1, 2]
    print(b.join(","))
    var c: [int] = [3, 1, 2]
    print(c.filter((x) => x > 1).len())
    var d: [int] = [3, 1, 2]
    var e = d
    print(e[0])
}
EOF
expect_runs "annotated [int] sum/join/filter/copy" "$TMP/ann_ops.wyn" "6
3,1,2
2
3"

# The packed fast path must stay correct too.
cat > "$TMP/ann_fast.wyn" <<'EOF'
fn main() {
    var a: [int] = []
    for i in 0..5 { a.push(i * 2) }
    a.sort()
    print(a.len())
    print(a[0])
    for x in a { print(x) }
}
EOF
expect_runs "annotated [int] packed fast path" "$TMP/ann_fast.wyn" "5
0
0
2
4
6
8"

echo "-- 3. File.read_line to EOF (RC free() heap corruption) --"

printf 'alpha\nbeta\ngamma\n' > "$TMP/data.txt"
cat > "$TMP/readloop.wyn" <<EOF
fn main() {
    f = File.open("$TMP/data.txt", "r")
    var n = 0
    while File.eof(f) == 0 {
        line = File.read_line(f)
        if line.len() > 0 { n = n + 1 }
    }
    File.close(f)
    print(n)
}
EOF
expect_runs "read_line loop to EOF does not abort" "$TMP/readloop.wyn" "3"

# .len() on a returned line must be right (the RC header's cached length is now
# set, matching wyn_strdup; string_length() prefers it over strlen()).
cat > "$TMP/readlen.wyn" <<EOF
fn main() {
    f = File.open("$TMP/data.txt", "r")
    line = File.read_line(f)
    print(line.len())
    File.close(f)
}
EOF
expect_runs "read_line result has correct .len()" "$TMP/readlen.wyn" "6"

# An empty file goes straight to the EOF path - the crashing case, minimally.
: > "$TMP/empty.txt"
cat > "$TMP/readempty.wyn" <<EOF
fn main() {
    f = File.open("$TMP/empty.txt", "r")
    var n = 0
    while File.eof(f) == 0 {
        line = File.read_line(f)
        if line.len() > 0 { n = n + 1 }
    }
    File.close(f)
    print(n)
}
EOF
expect_runs "read_line on an empty file does not abort" "$TMP/readempty.wyn" "0"

echo ""; echo "race-and-io-soundness: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
