#!/usr/bin/env bash
# Codegen must be able to FAIL THE BUILD, on every path that invokes it.
#
# The defect this guards: codegen printed "Error: Unknown method '%s' for type '%s'",
# emitted nothing for the call, and fell through — so `wyn build` printed the error AND
# "✓ Built", exit 0, and the program ran with the call deleted. `parser_had_error()` and
# `checker_had_error()` existed; there was no `codegen_had_error()`. Three enforcement
# sites consume it now: build, cross-compile, and run.
#
# WHY THIS SCRIPT EXISTS AT ALL, when the same rule has .wyn regression tests:
# `tests/run_bdd.sh` drives every test through `$WYN build` (lines 174 and 318). It never
# calls `wyn run`. So the `run` enforcement site — the most-used command in the product —
# had no coverage, and reverting that one hunk restored the complete original defect with
# the whole suite still green. The .wyn tests cover `build`; this covers `run`.
#
# Two things it asserts that a message check cannot:
#   - the EXIT CODE, which is the thing that was wrong (the diagnostic was already
#     printed before the fix, so asserting the text alone passed on the broken compiler);
#   - that no `<file>.out` cache entry is left behind for a failed program, because
#     `wyn run` caches next to the source and a cached success would mask a later failure.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
PASS=0; FAIL=0
TMP=$(mktemp -d) || exit 2
trap 'rm -rf "$TMP"' EXIT

ok()  { echo "  ok    $1"; PASS=$((PASS+1)); }
bad() { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <program>  — `wyn run` must exit non-zero and say why.
reject() {
    local label="$1" src="$2" d
    d=$(mktemp -d "$TMP/arm.XXXXXX") || exit 2
    printf '%b\n' "$src" > "$d/a.wyn"
    local out rc
    out=$(cd "$d" && perl -e 'alarm 60; exec @ARGV' -- "$WYN_ABS" run a.wyn 2>&1); rc=$?
    if [ "$rc" -eq 0 ]; then
        bad "$label: wyn run exited 0; it must fail. output: $(printf '%s' "$out" | tr '\n' ' ' | cut -c1-160)"
        return
    fi
    # Assert the diagnostic is PRESENT, never that an error string is absent: an
    # absence check passes when the message merely moves to another stream.
    if ! printf '%s' "$out" | grep -q "Unknown method"; then
        bad "$label: failed with rc=$rc but did not name the unknown method: $(printf '%s' "$out" | tr '\n' ' ' | cut -c1-160)"
        return
    fi
    if [ -e "$d/a.wyn.out" ]; then
        bad "$label: a failed run left a <file>.out cache entry, which a later run would reuse"
        return
    fi
    ok "$label: wyn run exits $rc, names the method, leaves no cache entry"
}

# accept <label> <program> <expected stdout>  — stops the fix becoming "reject everything".
accept() {
    local label="$1" src="$2" want="$3" d
    d=$(mktemp -d "$TMP/arm.XXXXXX") || exit 2
    printf '%b\n' "$src" > "$d/a.wyn"
    local out rc
    out=$(cd "$d" && perl -e 'alarm 60; exec @ARGV' -- "$WYN_ABS" run a.wyn 2>&1); rc=$?
    if [ "$rc" -ne 0 ]; then
        bad "$label: a valid program failed to run (rc=$rc): $(printf '%s' "$out" | tr '\n' ' ' | cut -c1-160)"
    elif ! printf '%s' "$out" | grep -qF "$want"; then
        bad "$label: ran but printed the wrong thing (want '$want'): $(printf '%s' "$out" | tr '\n' ' ' | cut -c1-120)"
    else
        ok "$label: runs and prints '$want'"
    fi
}

echo "=== codegen must fail the build on the \`wyn run\` path ==="

# One arm per scalar receiver family. They are separate arms on purpose: the fix touches
# one give-up point per receiver kind, and a partial fix must not hide behind a sibling.
reject "int receiver"   'x = 42\nx.nope_zzz_method()'
reject "float receiver" 'x = 3.5\nx.nope_zzz_method()'
reject "bool receiver"  'x = true\nx.nope_zzz_method()'

# The ACCEPT arms matter as much: without them, `codegen_had_error()` returning true
# unconditionally would satisfy every reject arm above.
accept "valid int method"    'x = 41\nprint((x + 1).to_string())'          '42'
accept "valid float method"  'x = 4.0\nprint(x.sqrt().to_int().to_string())' '2'
accept "valid bool method"   'x = true\nprint(x.to_int().to_string())'      '1'

echo ""
echo "codegen-fails-the-build: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] && [ "$PASS" -ge 6 ] || exit 1
