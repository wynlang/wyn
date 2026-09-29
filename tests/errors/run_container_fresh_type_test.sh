#!/bin/bash
# V-39 (#418): `HashMap.new()` is a NAMESPACE call, and that path adopted the
# registered return-type node directly instead of freshening it. Every
# `HashMap.new()` in a program therefore shared ONE Type*, so whichever `.set()`
# ran first fixed MapType.value_type for all of them - and a second map with a
# different value type was read through the first one's getter and answered 0.
# Silent, no error, exit 0, and it is in the published v1.21.0.
#
# The `{}` / `{:}` LITERAL paths already route through freshen_container_ret;
# these arms pin that the namespace path does too.
#
# Every arm runs in BOTH build modes: `wyn run` and `wyn run --release` compile
# against DIFFERENT runtime headers (wyn_runtime.h vs wyn_runtime_slim.h), and a
# type decision that is right in one and wrong in the other has shipped before.
# Note the flag order - `wyn run` stops interpreting arguments at the file name,
# so `--release` must come BEFORE the path or it is handed to the program.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect <name> <file> <expected-exact-stdout>
expect(){
    local name="$1" f="$2" want="$3"
    local mode flag
    for mode in debug release; do
        if [ "$mode" = release ]; then flag="--release"; else flag=""; fi
        local out rc
        # shellcheck disable=SC2086
        out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run $flag "$f" 2>&1); rc=$?
        # drop the compiler's own progress line, keep program output
        out=$(printf '%s\n' "$out" | grep -v 'Compiled in' | sed '/^[[:space:]]*$/d')
        if [ "$rc" -ne 0 ]; then
            bad "$name [$mode]: exit $rc — $(printf '%s' "$out" | tr '\n' '|' | cut -c1-160)"
        elif [ "$out" = "$want" ]; then
            ok "$name [$mode]"
        else
            bad "$name [$mode]: got '$(printf '%s' "$out" | tr '\n' '|')' want '$(printf '%s' "$want" | tr '\n' '|')'"
        fi
    done
}

# --- 1. THE REPORTED DEFECT. int map first, string map second.
cat > "$TMP/two.wyn" <<'WYN'
fn main() {
    a = HashMap.new()
    a.set("k", 1)
    b = HashMap.new()
    b.set("k", "s")
    print("${a.get("k")}")
    print("${b.get("k")}")
}
WYN
expect "two HashMap.new(): int then string" "$TMP/two.wyn" "$(printf '1\ns')"

# --- 2. THE OTHER ORDER. Not "the last one wins" - each map keeps its own type.
# Without this arm a fix that simply let the SECOND .set() overwrite the shared
# node would pass arm 1 and still be wrong.
cat > "$TMP/rev.wyn" <<'WYN'
fn main() {
    a = HashMap.new()
    a.set("k", "s")
    b = HashMap.new()
    b.set("k", 7)
    print("${a.get("k")}")
    print("${b.get("k")}")
}
WYN
expect "two HashMap.new(): string then int" "$TMP/rev.wyn" "$(printf 's\n7')"

# --- 3. THREE maps, three value types, read in an order that is not the order
# they were written in.
cat > "$TMP/three.wyn" <<'WYN'
fn main() {
    i = HashMap.new()
    i.set("k", 42)
    s = HashMap.new()
    s.set("k", "hello")
    f = HashMap.new()
    f.set("k", 2.5)
    print("${s.get("k")}")
    print("${f.get("k")}")
    print("${i.get("k")}")
}
WYN
expect "three HashMap.new(), read out of order" "$TMP/three.wyn" "$(printf 'hello\n2.5\n42')"

# --- 4. A namespace-constructed map and a LITERAL map in one program. The
# literal path was always correct; this pins that the two paths agree now rather
# than one having been dragged down to the other.
cat > "$TMP/mixed.wyn" <<'WYN'
fn main() {
    lit = {"k": "fromliteral"}
    ns = HashMap.new()
    ns.set("k", 99)
    print("${lit["k"]}")
    print("${ns.get("k")}")
}
WYN
expect "literal map alongside HashMap.new()" "$TMP/mixed.wyn" "$(printf 'fromliteral\n99')"

# --- 5. ONE map must keep working. The shared node was not merely an alias, it
# was also what carried the inferred type at all, so the single-map case is the
# regression this fix could plausibly cause.
cat > "$TMP/one.wyn" <<'WYN'
fn main() {
    m = HashMap.new()
    m.set("a", 1)
    m.set("b", 2)
    print("${m.get("a")}")
    print("${m.get("b")}")
    print("${m.len()}")
}
WYN
expect "a single HashMap.new() still infers and reads" "$TMP/one.wyn" "$(printf '1\n2\n2')"

# --- 6. Two maps of the SAME value type must still both work - the fresh node
# must be populated, not merely fresh.
cat > "$TMP/same.wyn" <<'WYN'
fn main() {
    a = HashMap.new()
    a.set("k", "x")
    b = HashMap.new()
    b.set("k", "y")
    print("${a.get("k")}${b.get("k")}")
}
WYN
expect "two HashMap.new() of the same value type" "$TMP/same.wyn" "xy"

# --- 7. The aliasing was per-PROGRAM, not per-scope: two maps built in
# different functions shared the node too.
cat > "$TMP/fns.wyn" <<'WYN'
fn build_int() -> int {
    m = HashMap.new()
    m.set("k", 5)
    return m.get("k")
}
fn build_str() -> string {
    m = HashMap.new()
    m.set("k", "z")
    return m.get("k")
}
fn main() {
    print("${build_int()}")
    print("${build_str()}")
}
WYN
expect "HashMap.new() in two different functions" "$TMP/fns.wyn" "$(printf '5\nz')"

echo ""; echo "container-fresh-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
