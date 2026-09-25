#!/bin/bash
# There must be a string->number parse that CANNOT abort the process, and every
# predicate that claims to describe a parse must agree with that parse.
#
# THE DEFECT THIS GATES (measured on dev @ 675880a5):
#
#     "notanumber".to_int()   -> panic: to_int parse error ... (process exits 1,
#                                no line number, nothing catchable)
#     "1.5".is_numeric()      -> true   ... and then "1.5".to_int() PANICS
#
# so no Wyn CLI could read untrusted input: there was no parse that returns a
# value you can test, and the only predicate on offer answered a DIFFERENT
# question than the one the caller was about to ask. V-18.
#
# WHAT WAS ADDED
#     s.to_int_checked()   -> Result<int, string>
#     s.to_float_checked() -> Result<float, string>
#     s.is_int()           -> bool, defined as to_int_checked().is_ok()
#
# `to_int` / `to_float` keep their panic - existing programs rely on it, and the
# point here is to ADD a safe path, not to move the unsafe one.
#
# WHY THE ARMS BELOW ARE SHAPED THIS WAY
#
# The acceptance set of a parse is a pile of judgement calls, and a judgement
# call that is not written down gets re-decided differently next quarter. Each
# arm below states the answer it demands AND why, so the next reader argues with
# the reason instead of guessing at the intent:
#
#   ""        -> Err    Python's int("") raises. An empty field is the single
#                       most common thing a CLI reads; returning 0 would be the
#                       silent-wrong-answer this whole item exists to remove.
#   "   "     -> Err    same, whitespace is not a number.
#   " 12 "    -> Ok 12  Python's int(" 12 ") == 12, and `to_int` ALREADY accepts
#                       it (strtoll skips leading blanks; trailing blanks are
#                       skipped explicitly). The checked parse must not be a
#                       second, stricter rule.
#   "+7"      -> Ok 7   Python's int("+7") == 7.
#   "-7"      -> Ok -7
#   "12abc"   -> Err    trailing junk. Python raises; taking the 12 and dropping
#                       "abc" is how a mis-typed argument becomes a wrong answer.
#   "0x10"    -> Err    to_int is base 10, full stop. Python's int("0x10") also
#                       raises (it needs base=16).
#   2^63      -> Err    Wyn's int is 64-bit, so a value that does not fit is an
#                       ERROR, not a wraparound. The message says "overflow" and
#                       not "not a valid integer", because those are different
#                       mistakes for the caller to fix.
#   "1.5"     -> Err    from to_int_checked (it is not an integer) and Ok 1.5
#                       from to_float_checked. This pair IS the V-18 headline.
#
# The float sibling accepts what C's strtod accepts, which includes "0x10" (hex
# float) and "inf"/"nan". That is inherited from `to_float` verbatim and is
# ASSERTED here rather than quietly diverging: the property that matters is that
# the checked parse and the panicking parse accept exactly the same strings. If
# to_float's hex acceptance is later judged wrong, both change together.
#
# The strongest arm is PARITY: for every input, `to_int` panics if and only if
# `to_int_checked()` is Err, and `is_int()` equals `to_int_checked().is_ok()`.
# That is what stops the checked path from becoming a second, drifting copy of
# the acceptance rule - the failure shape this codebase keeps getting bitten by.
#
# NOTE FOR ANYONE RE-RUNNING THIS: the parse lives in the RUNTIME, precompiled
# into runtime/libwyn_rt.a, and is also DECLARED in src/wyn_runtime_slim.h for
# `--release`. After editing wyn_runtime.h you must
# `rm -f runtime/obj/*.o && make runtime`, or compiled programs keep the old
# behaviour and this gate appears to fail against a correct source tree. The
# last three arms run `wyn build` and `wyn run --release` for exactly that
# reason (a builtin missing from the slim header is a known live defect class).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

run_wyn() { # run_wyn <file>; sets OUT (stdout+stderr) and RC
    OUT=$(cd "$TMP" && perl -e 'alarm(120); exec @ARGV' -- "$WYN" run "$1" 2>&1); RC=$?
}
OUT=""; RC=0

# --- 1. the two acceptance lines from V-18, verbatim --------------
cat > "$TMP/accept.wyn" <<'WYN'
fn main() {
    print("abc".to_int_checked().is_err())
    print("12".to_int_checked().unwrap_or(-1))
}
WYN
run_wyn accept.wyn; out=$OUT; rc=$RC
want=$'true\n12'
got=$(printf '%s' "$out" | grep -v 'Compiled in')
if [ "$rc" -eq 0 ] && [ "$got" = "$want" ]; then
    ok "V-18 acceptance: is_err() prints true, unwrap_or(-1) prints 12, exit 0"
else
    bad "V-18 acceptance (rc=$rc, want '$want', got '$got')"
fi

# --- 2. the acceptance TABLE, one line per judgement call --------------------
# INT_<tag> <ok|err> <value-or-message-word>
cat > "$TMP/table.wyn" <<'WYN'
fn report(tag: string, s: string) {
    r = s.to_int_checked()
    if r.is_ok() {
        print("INT_${tag} ok ${r.unwrap()}")
    } else {
        print("INT_${tag} err ${r.unwrap_err()}")
    }
    print("PRED_${tag} ${s.is_int()} ${r.is_ok()}")
}
fn freport(tag: string, s: string) {
    r = s.to_float_checked()
    if r.is_ok() {
        print("FLT_${tag} ok ${r.unwrap()}")
    } else {
        print("FLT_${tag} err")
    }
}
fn main() {
    report("plain", "12")
    report("empty", "")
    report("blank", "   ")
    report("padded", " 12 ")
    report("plus", "+7")
    report("minus", "-7")
    report("junk", "12abc")
    report("hex", "0x10")
    report("over", "99999999999999999999")
    report("float", "1.5")
    report("big", "3000000000")
    report("neg64", "-9223372036854775808")
    freport("plain", "1.5")
    freport("int", "12")
    freport("junk", "12abc")
    freport("empty", "")
    freport("hex", "0x10")
    // Branched rather than interpolated on purpose: this arm is about what the
    // two predicates MEAN, and `${bool}` rendering is a separate, still-open
    // defect (string_is_alpha/_digit/_alnum/_whitespace/_numeric all return C
    // `int`, so `print(s.is_numeric())` shows 1 while `print(s.is_int())` shows
    // true). Testing the semantics through `if` keeps this gate from breaking
    // when that family is fixed.
    if "1.5".is_numeric() { print("NUMERIC_float true") } else { print("NUMERIC_float false") }
    if "1.5".is_int() { print("ISINT_float true") } else { print("ISINT_float false") }
}
WYN
run_wyn table.wyn; out=$OUT; rc=$RC
printf '%s\n' "$out" > "$TMP/table.out"
if [ "$rc" -ne 0 ]; then
    bad "acceptance table runs at exit 0 (rc=$rc)"
    sed -n '1,12p' "$TMP/table.out"
else
    ok "acceptance table runs at exit 0 (no arm aborts the process)"
fi

# substr, not $1="" - awk's field reassignment collapses runs of spaces, which
# would hide exactly the whitespace this table is asserting about.
field() { awk -v k="$1" 'index($0, k" ")==1 {print substr($0, length(k)+2); exit}' "$TMP/table.out"; }

expect() { # expect <tag> <expected-line-body> <why>
    local g; g=$(field "$1")
    if [ "$g" = "$2" ]; then ok "$3"
    else bad "$3 -- $1 want '$2' got '$g'"; fi
}

expect INT_plain  "ok 12"     '"12" parses'
# the err arms carry a message, so match on the prefix + the offending value
err_has() { # err_has <tag> <substring> <why>
    local g; g=$(field "$1")
    case "$g" in
        "err "*"$2"*) ok "$3" ;;
        *) bad "$3 -- $1 want an err mentioning '$2', got '$g'" ;;
    esac
}
err_has INT_empty  '""'        '"" is an ERROR (not 0), and the message shows the input'
err_has INT_blank  '"   "'     '"   " (whitespace only) is an ERROR'
expect  INT_padded "ok 12"     '" 12 " is Ok 12 - same as to_int already accepts, and as Python'
expect  INT_plus   "ok 7"      '"+7" is Ok 7 (Python int("+7") == 7)'
expect  INT_minus  "ok -7"     '"-7" is Ok -7'
err_has INT_junk   '"12abc"'   '"12abc" is an ERROR - trailing junk is never silently dropped'
err_has INT_hex    '"0x10"'    '"0x10" is an ERROR - to_int is base 10 (Python raises too)'
err_has INT_over   'overflow'  'a value above int64 is an ERROR, and says "overflow" not "not a valid integer"'
err_has INT_float  '"1.5"'     '"1.5" is an ERROR from to_int_checked - it is not an integer'
expect  INT_big    "ok 3000000000"            'an Ok payload above 2^31 survives - Result<int,...> is 64-bit'
expect  INT_neg64  "ok -9223372036854775808"  'int64 MIN parses (the boundary, not just a big number)'

expect  FLT_plain  "ok 1.5"    '"1.5".to_float_checked() is Ok 1.5 - the V-18 pair with INT_float'
expect  FLT_int    "ok 12.0"   '"12" is Ok as a float too'
expect  FLT_junk   "err"       '"12abc" is an ERROR for the float parse as well'
expect  FLT_empty  "err"       '"" is an ERROR for the float parse as well'
expect  FLT_hex    "ok 16.0"   '"0x10" is Ok 16.0 - INHERITED from to_float (C strtod), asserted so the two parses cannot drift apart'

expect  NUMERIC_float "true"   'is_numeric("1.5") stays true - "1.5" IS a number'
expect  ISINT_float   "false"  'is_int("1.5") is FALSE - this is the predicate that gates to_int, and it does not lie'

# --- 3. the predicate IS the parse ------------------------------------------
mismatch=$(awk '$1 ~ /^PRED_/ && $2 != $3 {print $1" "$2" != "$3}' "$TMP/table.out")
npred=$(awk '$1 ~ /^PRED_/' "$TMP/table.out" | wc -l | tr -d ' ')
if [ -z "$mismatch" ] && [ "$npred" -ge 12 ]; then
    ok "is_int() == to_int_checked().is_ok() for all $npred inputs (one acceptance rule, not two)"
else
    bad "is_int() must equal to_int_checked().is_ok() (n=$npred): $mismatch"
fi

# --- 4. PARITY: to_int panics exactly when to_int_checked() is Err -----------
# Run the panicking parse in its OWN process per input and compare its exit
# status against the Err column above. This is the arm that stops the checked
# parse from becoming a second copy of the acceptance rule.
parity_fail=""
parity_n=0
check_parity() { # check_parity <tag> <wyn-string-literal>
    local tag="$1" lit="$2" expect_err rc2
    case "$(field "INT_$tag")" in err*) expect_err=1 ;; *) expect_err=0 ;; esac
    printf 'fn main() { print("%s".to_int()) }\n' "$lit" > "$TMP/p_$tag.wyn"
    (cd "$TMP" && perl -e 'alarm(120); exec @ARGV' -- "$WYN" run "p_$tag.wyn" >/dev/null 2>&1); rc2=$?
    parity_n=$((parity_n+1))
    if [ "$expect_err" -eq 1 ] && [ "$rc2" -eq 0 ]; then
        parity_fail="$parity_fail $tag(checked=Err,to_int=ok)"
    elif [ "$expect_err" -eq 0 ] && [ "$rc2" -ne 0 ]; then
        parity_fail="$parity_fail $tag(checked=Ok,to_int=rc$rc2)"
    fi
}
check_parity plain  '12'
check_parity empty  ''
check_parity blank  '   '
check_parity padded ' 12 '
check_parity plus   '+7'
check_parity minus  '-7'
check_parity junk   '12abc'
check_parity hex    '0x10'
check_parity over   '99999999999999999999'
check_parity float  '1.5'
if [ -z "$parity_fail" ]; then
    ok "to_int panics exactly when to_int_checked() is Err ($parity_n inputs)"
else
    bad "to_int / to_int_checked disagree on:$parity_fail"
fi

# --- 5. to_int's panic is UNCHANGED (this PR adds a path, it does not move one)
cat > "$TMP/still_panics.wyn" <<'WYN'
fn main() {
    print("notanumber".to_int())
}
WYN
run_wyn still_panics.wyn; out=$OUT; rc=$RC
if [ "$rc" -ne 0 ] && printf '%s' "$out" | grep -q 'panic: to_int parse error'; then
    ok "to_int still panics on garbage with its original message (no behaviour was taken away)"
else
    bad "to_int must still panic on garbage (rc=$rc, out='$out')"
fi

# --- 6. unwrap() on an Err is still fatal, and names the reason --------------
cat > "$TMP/unwrap_err.wyn" <<'WYN'
fn main() {
    print("abc".to_int_checked().unwrap())
}
WYN
run_wyn unwrap_err.wyn; out=$OUT; rc=$RC
# grep for BOTH "unwrap()" and the input: matching only 'abc' would also be
# satisfied by a COMPILE error that happens to echo the source line, which is
# exactly how this arm passed vacuously before to_int_checked existed.
if [ "$rc" -ne 0 ] && printf '%s' "$out" | grep -q 'unwrap()' && printf '%s' "$out" | grep -q 'abc'; then
    ok "unwrap() on the Err is fatal and quotes the offending input"
else
    bad "unwrap() on the Err must be fatal and quote the input (rc=$rc, out='$out')"
fi

# --- 6b. whitespace-only is REJECTED by to_int, not silently 0 --------------
# This one IS a change to to_int, and it is deliberate: "   ".to_int() returned
# 0 at exit 0 (see wyn_parse_int_core's comment - the trailing-blank skip ran
# before the "consumed anything?" test). A parse that answers 0 for a blank
# field is the defect V-18 is about, so the shared rule rejects it and BOTH
# entry points inherit that.
cat > "$TMP/blank_to_int.wyn" <<'WYN'
fn main() {
    print("   ".to_int())
}
WYN
run_wyn blank_to_int.wyn; out=$OUT; rc=$RC
if [ "$rc" -ne 0 ] && printf '%s' "$out" | grep -q 'panic: to_int parse error'; then
    ok "\"   \".to_int() panics instead of silently returning 0"
else
    bad "\"   \".to_int() must not silently return 0 (rc=$rc, out='$out')"
fi

# --- 7. a CLI can now actually read untrusted input --------------------------
# The dogfooding shape that started V-18: sum the numeric lines, report the rest,
# and come back at exit 0.
cat > "$TMP/cli.wyn" <<'WYN'
fn main() {
    inputs = ["10", "oops", "32", "", "1.5"]
    total = 0
    bad_count = 0
    for s in inputs {
        r = s.to_int_checked()
        if r.is_ok() {
            total = total + r.unwrap()
        } else {
            bad_count = bad_count + 1
        }
    }
    print("SUM ${total} BAD ${bad_count}")
}
WYN
run_wyn cli.wyn; out=$OUT; rc=$RC
got=$(printf '%s' "$out" | awk '$1=="SUM"')
if [ "$rc" -eq 0 ] && [ "$got" = "SUM 42 BAD 3" ]; then
    ok "a loop over untrusted input sums the good values and counts the bad ones, exit 0"
else
    bad "untrusted-input loop (rc=$rc, want 'SUM 42 BAD 3', got '$got')"
fi

# --- 8. wyn build and wyn run --release must agree with wyn run -------------
# `wyn build` links runtime/libwyn_rt.a; `wyn run --release` is the ONLY command
# that compiles against src/wyn_runtime_slim.h. A new builtin missing from the
# slim header fails HERE and nowhere else.
cat > "$TMP/three.wyn" <<'WYN'
fn main() {
    print("${"abc".to_int_checked().is_err()} ${"12".to_int_checked().unwrap_or(-1)} ${"1.5".to_float_checked().unwrap_or(0.0)} ${"1.5".is_int()}")
}
WYN
WANT3='true 12 1.5 false'

run_wyn three.wyn; out=$OUT; rc=$RC
got=$(printf '%s' "$out" | grep -v 'Compiled in')
if [ "$rc" -eq 0 ] && [ "$got" = "$WANT3" ]; then ok "wyn run: $WANT3"
else bad "wyn run (rc=$rc, want '$WANT3', got '$got')"; fi

# flags go BEFORE the path for `wyn run` - `wyn run f.wyn --release` silently
# compiles NON-release and hands --release to the program.
#
# AND the --release arm gets its OWN copy of the source. `wyn run` caches the
# built binary as <file>.wyn.out and the cache key does NOT include --release, so
# running `wyn run f.wyn` and then `wyn run --release f.wyn` REUSES the
# non-release binary and never compiles against the slim header at all. Verified
# while mutation-testing this gate: with the slim-header declarations deleted, a
# fresh directory fails to compile under --release, and the same file fails to
# notice once a non-release .out exists. Reusing three.wyn here would have made
# this arm silently vacuous - which is the same class of hole the slim header
# itself keeps falling into.
cp "$TMP/three.wyn" "$TMP/three_rel.wyn"
out=$(cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" run --release three_rel.wyn 2>&1); rc=$?
got=$(printf '%s' "$out" | grep -v 'Compiled in')
if [ "$rc" -eq 0 ] && [ "$got" = "$WANT3" ]; then ok "wyn run --release (slim runtime header) agrees"
else bad "wyn run --release (rc=$rc, want '$WANT3', got '$got')"; fi

out=$(cd "$TMP" && perl -e 'alarm(180); exec @ARGV' -- "$WYN" build three.wyn 2>&1); rc=$?
if [ "$rc" -ne 0 ]; then
    bad "wyn build succeeds (rc=$rc, out='$out')"
elif [ ! -x "$TMP/three" ]; then
    bad "wyn build produced no binary at $TMP/three"
else
    got=$(perl -e 'alarm(60); exec @ARGV' -- "$TMP/three" 2>&1)
    if [ "$got" = "$WANT3" ]; then ok "wyn build (prebuilt runtime lib) agrees"
    else bad "wyn build (want '$WANT3', got '$got')"; fi
fi

echo ""; echo "parse-checked: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
