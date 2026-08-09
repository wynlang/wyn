#!/usr/bin/env bash
# A typo'd stdlib namespace method must name itself, whatever C prefix it lowers to.
#
# THE DEFECT
#
# Namespace methods are deliberately NOT verified by the checker: an allowlist there would
# reject the ~100 runtime functions it has no table entry for. So `HashMap.contains(m, k)`
# passes `wyn check` cleanly, lowers to a bare C call, and dies at C-compile.
# main.c's wyn_report_undeclared_namespace_call() exists to translate that leaked C symbol
# back into Wyn spelling -- but it required the symbol to start with an UPPERCASE letter.
#
# FOUR namespaces lower to a lowercase C prefix (`HashMap.has` -> `hashmap_has`), so they
# fell through it and reported:
#
#   Error: compilation failed (internal codegen error)
#   Run with WYN_DEBUG=1 for details
#
# No method name, no namespace, no line -- for an ordinary typo. The real name here is
# `HashMap.has`, and `HashSet.contains` DOES exist, so this is easy to trip over.
#
# The four are measured, not guessed: they are the only `emit("<lowercase>_%.*s(")` blind
# prefixes in codegen_expr.c, and probing every one of the 43 builtin namespaces reported
# exactly these four as untranslated.
#
# WHY A SHELL TEST AND NOT AN EXPECT FILE
#
# What is under test is the TEXT of a diagnostic for a program that must NOT compile, plus a
# nonzero exit. An EXPECT file in tests/regression/ runs under `wyn run` and must succeed, so
# it can express neither.
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

# --- the four lowercase-prefix namespaces: the regression -------------------------------
# HashMap is the case that was actually hit while writing an app: the real name is
# `HashMap.has`, and the plausible `HashMap.contains` gave nothing to act on.
cat > hashmap.wyn <<'EOF'
fn main() {
    var m = HashMap.new()
    HashMap.set(m, "a", "1")
    if HashMap.contains(m, "a") { print("yes") } else { print("no") }
}
EOF

cat > hashset.wyn <<'EOF'
fn main() {
    var s = HashSet.new()
    HashSet.zzz_no_such_method(s)
    print("unreachable")
}
EOF

cat > regex.wyn <<'EOF'
fn main() {
    var r = Regex.zzz_no_such_method("a")
    print("${r}")
}
EOF

cat > random.wyn <<'EOF'
fn main() {
    var r = Random.zzz_no_such_method()
    print("${r}")
}
EOF

# --- an uppercase-prefix namespace: must keep working (this path already did) -----------
cat > math.wyn <<'EOF'
fn main() {
    var r = Math.zzz_no_such_method(1)
    print("${r}")
}
EOF

# --- what must NOT be claimed as a namespace typo --------------------------------------
# A bare lowercase call is rejected by the CHECKER as an undefined variable, so it never
# reaches this translator at all. Pinning it proves the four-entry table cannot swallow an
# ordinary undefined function and mislabel it as a stdlib typo.
cat > bare_call.wyn <<'EOF'
fn main() {
    print("${hashmap_nope(1)}")
}
EOF

# A real HashMap program must still compile and run. A diagnostics change is exactly where
# a false positive breaks working code, so the positive case is pinned by VALUE.
cat > good.wyn <<'EOF'
fn main() {
    var m = HashMap.new()
    HashMap.set(m, "k", "v")
    if HashMap.has(m, "k") { print("${HashMap.get(m, "k")}") } else { print("missing") }
}
EOF

for f in hashmap hashset regex random math; do
    out="$("$WYN_ABS" run "$f.wyn" 2>&1)"
    rc=$?
    check "$f: run exits nonzero" "$([ "$rc" -ne 0 ] && echo yes || echo no)" "yes"
    # The whole point: the message names the method in WYN spelling, not the C symbol.
    check "$f: names the unknown method" \
        "$(echo "$out" | grep -c "unknown method")" "1"
    check "$f: does not leak the generated-C symbol" \
        "$(echo "$out" | grep -ci "internal codegen error")" "0"
    check "$f: suggests checking the stdlib docs" \
        "$(echo "$out" | grep -c "stdlib docs")" "1"
done

# Each one must name its OWN namespace in Wyn spelling -- a table wired up wrongly would
# translate to the wrong namespace, which a mere "unknown method" grep would not catch.
check "hashmap: says HashMap, not hashmap" \
    "$("$WYN_ABS" run hashmap.wyn 2>&1 | grep -c "unknown method 'HashMap.contains'")" "1"
check "hashset: says HashSet" \
    "$("$WYN_ABS" run hashset.wyn 2>&1 | grep -c "unknown method 'HashSet.zzz_no_such_method'")" "1"
check "regex: says Regex" \
    "$("$WYN_ABS" run regex.wyn 2>&1 | grep -c "unknown method 'Regex.zzz_no_such_method'")" "1"
check "random: says Random" \
    "$("$WYN_ABS" run random.wyn 2>&1 | grep -c "unknown method 'Random.zzz_no_such_method'")" "1"
check "math: the uppercase path still says Math" \
    "$("$WYN_ABS" run math.wyn 2>&1 | grep -c "unknown method 'Math.zzz_no_such_method'")" "1"

# An undefined lowercase function is a CHECKER error about the name the user wrote, and must
# not be relabelled as a namespace method.
out="$("$WYN_ABS" check bare_call.wyn 2>&1)"
check "bare_call: rejected as an undefined variable" \
    "$(echo "$out" | grep -c "Undefined variable 'hashmap_nope'")" "1"
check "bare_call: not claimed to be a namespace method" \
    "$(echo "$out" | grep -c "unknown method")" "0"

# The positive case, by value.
"$WYN_ABS" check good.wyn >/dev/null 2>&1
check "good: a real HashMap program still checks" "$([ $? -eq 0 ] && echo yes || echo no)" "yes"
check "good: and still runs correctly" "$("$WYN_ABS" run good.wyn 2>/dev/null | tail -1)" "v"

echo ""
echo "namespace-typo-message: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
