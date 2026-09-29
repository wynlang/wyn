#!/bin/bash
# The Option/Result monomorphic families must be COMPLETE and their methods must
# return the PAYLOAD type.
#
# THE DEFECT THIS GATES. `Result<string, E>.unwrap_or(d)` typed as `int`, because
# method_signatures carries one concrete return type per receiver and its row is
#
#     {"result", "unwrap_or", "int", 1}   // "Type depends on Result<T,E>"
#
# so a string payload came back as int and `.upper()` on it failed with "Unknown
# method 'upper' for type 'int'". Option escaped this only by ACCIDENT: its
# receiver is usually the monomorphic TYPE_STRUCT family (OptionString), for which
# get_receiver_type_string() answers NULL, so the signature table was never
# consulted for Option at all.
#
# Underneath that, the wrong type was HIDING a missing runtime function. With the
# receiver typed int, codegen emitted ResultInt_unwrap_or - which exists - and read
# the string pointer back as a long long. ResultString_unwrap_or was the single
# hole in an otherwise complete 8-family matrix, and it only became visible as a
# link error once the type was right. A wrong type that silently selects a
# different family's function is the worst shape this code can have: it compiles
# and prints a number.
#
# So there are two kinds of arm, and both are needed:
#  - the STATIC matrix, which catches the next missing family function directly
#    rather than waiting for someone to write the one program that reveals it. It
#    checks BOTH runtime headers, because `--release` emits the slim one and that
#    file is maintained by hand.
#  - the BEHAVIOURAL arms, because a declaration proves nothing about the TYPE. The
#    original defect had every function it needed except one and still produced a
#    wrong answer. Each payload is exercised with an operation only that payload
#    supports - .upper() on a string, + 0.25 on a float - so a family resolved to
#    the wrong payload type cannot pass.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

FULL="$ROOT/src/wyn_runtime.h"
SLIM="$ROOT/src/wyn_runtime_slim.h"

# --- static: the family x method matrix ---------------------------------------
# Every Option family needs these; every Result family needs those. Add a family
# or a method here and the gate tells you which cells are missing.
OPT_METHODS="Some None is_some is_none unwrap unwrap_or to_string"
RES_METHODS="Ok Err is_ok is_err unwrap unwrap_err unwrap_or to_string"
FAMILY_SUFFIXES="Int String Float Bool"

missing_full=""; missing_slim=""
for suf in $FAMILY_SUFFIXES; do
    for m in $OPT_METHODS; do
        fn="Option${suf}_${m}"
        grep -q "\b${fn}\b" "$FULL" || missing_full="$missing_full $fn"
        grep -q "\b${fn}\b" "$SLIM" || missing_slim="$missing_slim $fn"
    done
    for m in $RES_METHODS; do
        fn="Result${suf}_${m}"
        grep -q "\b${fn}\b" "$FULL" || missing_full="$missing_full $fn"
        grep -q "\b${fn}\b" "$SLIM" || missing_slim="$missing_slim $fn"
    done
done
if [ -z "$missing_full" ]; then ok "every Option/Result family function is in wyn_runtime.h"
else bad "wyn_runtime.h is missing:$missing_full"; fi
if [ -z "$missing_slim" ]; then ok "every Option/Result family function is declared in wyn_runtime_slim.h"
else bad "wyn_runtime_slim.h is missing:$missing_slim (--release emits this header)"; fi

# --- behavioural: unwrap_or returns the PAYLOAD type, in BOTH build modes -----
# $1 label  $2 declared return type  $3 Ok/Some expr  $4 empty expr  $5 default
# $6 expression using the result in a payload-specific way  $7 expected output
behave() {
    local label="$1" rty="$2" okx="$3" emptyx="$4" def="$5" use="$6" want="$7"
    local f="$TMP/f.wyn"
    cat > "$f" <<WYN
fn g(b: bool) -> $rty { if b { return $okx } return $emptyx }
fn main() -> int {
    v = g(false).unwrap_or($def)
    print("\${$use}")
    return 0
}
WYN
    local mode rc out
    for mode in "" "--release"; do
        rm -f "$TMP/f" "$f.c"
        if ! (cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" build $mode "$f") > "$TMP/b.log" 2>&1; then
            bad "$label unwrap_or (${mode:-dev}) builds"
            grep -m1 -oE "error: .*" "$TMP/b.log" | sed 's/^/          /'
            continue
        fi
        out=$(perl -e 'alarm(60); exec @ARGV' -- "$TMP/f" 2>&1)
        if [ "$out" = "$want" ]; then ok "$label unwrap_or returns its payload type (${mode:-dev})"
        else bad "$label unwrap_or (${mode:-dev}): want '$want', got '$out'"; fi
    done
}

behave "Option<string>"        "Option<string>"        'Some("y")'  'None'      '"fb"'  'v.upper()' "FB"
behave "Result<string, string>" "Result<string, string>" 'Ok("y")'   'Err("e")'  '"fb"'  'v.upper()' "FB"
behave "Option<float>"         "Option<float>"         'Some(1.5)'  'None'      '2.5'   'v + 0.25'  "2.75"
behave "Result<float, string>"  "Result<float, string>"  'Ok(1.5)'   'Err("e")'  '2.5'   'v + 0.25'  "2.75"
behave "Option<int>"           "Option<int>"           'Some(7)'    'None'      '3'     'v + 1'     "4"
behave "Result<int, string>"    "Result<int, string>"    'Ok(7)'     'Err("e")'  '3'     'v + 1'     "4"
behave "Option<bool>"          "Option<bool>"          'Some(true)' 'None'      'false' 'v'         "false"
behave "Result<bool, string>"   "Result<bool, string>"   'Ok(true)'  'Err("e")'  'false' 'v'         "false"

# --- the slim header, actually compiled --------------------------------------
# `wyn build --release` deliberately keeps the FULL runtime header, so none of the
# arms above compile wyn_runtime_slim.h at all - verified by mutation: deleting the
# slim DECLARATION of ResultString_unwrap_or left every behavioural arm green and
# only the static arm red. `wyn run --release` is the one command that emits the
# slim header, so it is the only way to turn a missing slim declaration into a
# compile error instead of a text-check finding.
cat > "$TMP/slim.wyn" <<'WYN'
fn g(b: bool) -> Result<string, string> { if b { return Ok("y") } return Err("e") }
fn main() -> int { v = g(false).unwrap_or("fb"); print("${v.upper()}"); return 0 }
WYN
out=$(cd "$TMP" && perl -e 'alarm(240); exec @ARGV' -- "$WYN" run --release "$TMP/slim.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "FB"; then
    ok "Result<string> unwrap_or compiles against the SLIM header (wyn run --release)"
else
    bad "Result<string> unwrap_or against the slim header (rc=$rc) [$(echo "$out" | grep -m1 -oE 'error: .*' | cut -c1-70)]"
fi

# --- the mismatch diagnostic must survive ------------------------------------
# Returning the wrapped type from the unwrap_or path must not swallow the check
# that the DEFAULT matches it - that check shares the same code.
cat > "$TMP/bad.wyn" <<'WYN'
fn g(b: bool) -> int? { if b { return 7 } return none }
fn main() -> int { v = g(false).unwrap_or("oops"); print("${v}"); return 0 }
WYN
out=$(cd "$TMP" && perl -e 'alarm(120); exec @ARGV' -- "$WYN" check "$TMP/bad.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "unwrap_or default is string but the value is int"; then
    ok "a default that does not match the payload is still rejected"
else
    bad "a default that does not match the payload is still rejected (rc=$rc) [$(echo "$out" | head -1)]"
fi

# --- #386: to_string / unwrap_err must be TYPED, not just present -------------
# The static matrix above already proves these eight functions EXIST. That is not the
# same as the checker knowing what they return, and it did not: `to_string()` came back
# typed `int`, so
#
#     s = g().to_string(); print(s.len())     # Unknown method 'len' for type 'int'
#
# even though printing it inline worked. `unwrap_err` typed correctly already, via the
# `<Family>_<method>` symbol route; it is asserted here so the two cannot diverge.
#
# WHY THESE ARMS AND NOT JUST REGISTRY ROWS. An earlier attempt at #386 added the
# `{"option","to_string","string",0}` rows to method_signatures and was reverted for
# having no effect. Re-measured: with the rows in place and nothing else changed, all
# three arms below still failed. Every realistic receiver is the monomorphic TYPE_STRUCT
# family, for which get_receiver_type_string() answers NULL, so that table is never
# consulted. Each arm therefore USES the result as a string - `.len()`, `.upper()`, a
# `-> string` return position - because only that can tell the type apart from int.
# typed <label> <program> <expected>
typed() {
    local label="$1" mode got
    printf '%s\n' "$2" > "$TMP/ts.wyn"
    for mode in "" "--release"; do
        rm -f "$TMP/ts" "$TMP/ts.wyn.c"
        if ! (cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" build $mode "$TMP/ts.wyn") \
             > "$TMP/b.log" 2>&1; then
            bad "$label (${mode:-dev}) builds"
            grep -m1 -oE "(error|Error)[:@] .*" "$TMP/b.log" | cut -c1-100 | sed 's/^/          /'
            continue
        fi
        got=$(perl -e 'alarm(60); exec @ARGV' -- "$TMP/ts" 2>&1)
        if [ "$got" = "$3" ]; then ok "$label (${mode:-dev})"
        else bad "$label (${mode:-dev}): want [$3] got [$got]"; fi
    done
}
typed "Option.to_string() is a string when stored" \
'fn g() -> int? { return Some(1) }
fn main() { s = g().to_string(); print(s.len()) }' '7'
typed "Result.to_string() is a string when stored" \
'fn g() -> Result<string, string> { return Ok("y") }
fn main() { s = g().to_string(); print(s.len()) }' '7'
typed "Option.to_string() satisfies a '-> string' return" \
'fn f(o: string?) -> string { return o.to_string() }
fn main() { print(f(Some("hi")).upper()) }' 'SOME("HI")'
typed "Result.unwrap_err() is a string" \
'fn e() -> Result<int, string> { return Err("bad") }
fn main() { u = e().unwrap_err(); print(u.upper()) }' 'BAD'
# A Result whose Err is NOT a string keeps its own err type - the four builtin families
# store a `const char*` err by construction, but a monomorphic Result<T,E> family does
# not, and answering "string" for it would be a guess. This arm is what fails if the
# resolution above ever stops asking.
typed "a non-string Err keeps its own type" \
'fn e() -> Result<int, int> { return Err(7) }
fn main() { print(e().unwrap_err() + 1) }' '8'

echo ""; echo "option-result-family: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
