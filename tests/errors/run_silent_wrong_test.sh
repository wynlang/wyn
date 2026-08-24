#!/bin/bash
# Silent-wrong-answer batch (FLOWY_DESIGN P0): the error paths.
# The value paths (correct results) live in tests/regression/
# test_map_get_default.wyn, test_index_compound_assign.wyn,
# test_float_array_reductions.wyn. This script covers what must now be
# ERRORS instead of silently wrong output:
#   - unknown format specs in interpolation (${pi:.2}) - compile error
#   - map.get default type mismatch - compile error
#   - m[k] += v on a missing key - runtime panic (Python KeyError semantics)
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

expect_check_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 1 ] && echo "$out" | grep -q "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -1)]"; fi
}

# 1. Format spec in interpolation: silently swallowed before, error now.
printf 'pi = 3.14159\nprint("${pi:.2}")\n' > "$TMP/spec.wyn"
expect_check_error "format spec rejected" "$TMP/spec.wyn" "Unsupported syntax in string interpolation"

# 2. Width/align spec too.
printf 'n = 5\nprint("${n:>8}")\n' > "$TMP/spec2.wyn"
expect_check_error "align spec rejected" "$TMP/spec2.wyn" "Unsupported syntax in string interpolation"

# 3. map.get default type mismatch: the default IS the fallback value, so a
#    string default on an int map would reintroduce the garbage class.
printf 'm = {"a": 5}\nprint(m.get("a", "oops"))\n' > "$TMP/mismatch.wyn"
expect_check_error "get default type mismatch" "$TMP/mismatch.wyn" "map.get default is string but the map's values are int"

# 4. Compound assign on a missing map key: runtime panic, not invented 0.
printf 'm = {"a": 5}\nm["missing"] += 1\nprint(m["missing"])\n' > "$TMP/missing.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/missing.wyn" 2>&1); rc=$?
if echo "$out" | grep -q 'map key "missing" not found for compound assignment'; then
    ok "compound assign missing key panics"
else
    bad "compound assign missing key panics (rc=$rc) [$(echo "$out" | tail -1)]"
fi

# 5. Positive: compound assign on an EXISTING key works (guard against the
#    panic firing for present keys).
printf 'm = {"a": 5}\nm["a"] += 1\nprint(m["a"])\n' > "$TMP/present.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/present.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q '^6$'; then ok "compound assign present key works"
else bad "compound assign present key works (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 6. Positive: a colon INSIDE a string/expression is not a format spec.
printf 'url = "http://x.com:8080/y"\nprint("addr ${url}")\n' > "$TMP/colon.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/colon.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'addr http://x.com:8080/y'; then ok "colon in interpolated value still fine"
else bad "colon in interpolated value still fine (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# --- .format() error paths -------------------------------------------------
# `.format()` used to be a silent no-op: the format string came back unchanged
# and every argument was discarded ("Hello %s".format("World") printed
# "Hello %s" at exit 0). Semantics are brace-style `{}`; printf-style specs and
# count mismatches are compile errors now. The value path lives in
# tests/regression/test_string_format_method.wyn.

# 7. printf-style spec is rejected, not silently copied through.
printf 'print("Hello %%s".format("World"))\n' > "$TMP/fmt_pct.wyn"
expect_check_error "format %s spec rejected" "$TMP/fmt_pct.wyn" "uses {} placeholders"

# 8. Precision spec too.
printf 'print("%%.2f".format(3.14159))\n' > "$TMP/fmt_prec.wyn"
expect_check_error "format %.2f spec rejected" "$TMP/fmt_prec.wyn" "uses {} placeholders"

# 9. Too few arguments for the placeholders.
printf 'print("{} {}".format(1))\n' > "$TMP/fmt_few.wyn"
expect_check_error "format too few args" "$TMP/fmt_few.wyn" "2 {} placeholders but 1 argument"

# 10. Too many arguments (silently dropped before).
printf 'print("{}".format(1, 2))\n' > "$TMP/fmt_many.wyn"
expect_check_error "format too many args" "$TMP/fmt_many.wyn" "1 {} placeholder but 2 arguments"

# 11. Positive: a bare percent sign is literal prose, not a spec.
printf 'print("{}%% done".format(50))\n' > "$TMP/fmt_pctok.wyn"
out=$(perl -e 'alarm(15); exec @ARGV' -- "$WYN" run "$TMP/fmt_pctok.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q '^50% done$'; then ok "literal percent still fine"
else bad "literal percent still fine (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# --- input_line() ----------------------------------------------------------
# input_line() was registered as returning `int`, so `s = input_line()` inferred
# int and codegen stringified the returned char* with int_to_string - printing
# the line's ADDRESS (e.g. 4339582744) instead of its text, at exit 0. It also
# returned a `static` buffer, so two calls aliased, and truncated at 1023 bytes.

# 12. The line comes back as its text, usable as a real string.
printf 'a = input_line()\nprint(a)\nprint(a.len())\nprint("got ${a}")\nprint("f " + a)\n' \
    > "$TMP/inp.wyn"
out=$(printf 'hello world\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/inp.wyn" 2>&1); rc=$?
expected=$(printf 'hello world\n11\ngot hello world\nf hello world')
got=$(echo "$out" | grep -v 'Compiled in\|Building\|Built')
if [ $rc -eq 0 ] && [ "$got" = "$expected" ]; then ok "input_line returns the line text"
else bad "input_line returns the line text (rc=$rc) [$(echo "$got" | tr '\n' '|')]"; fi

# 13. Two calls must NOT alias (the old static buffer made a == b).
printf 'a = input_line()\nb = input_line()\nprint(a)\nprint(b)\n' > "$TMP/inp2.wyn"
out=$(printf 'first\nsecond\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/inp2.wyn" 2>&1)
got=$(echo "$out" | grep -v 'Compiled in\|Building\|Built')
if [ "$got" = "$(printf 'first\nsecond')" ]; then ok "consecutive input_line calls do not alias"
else bad "consecutive input_line calls do not alias [$(echo "$got" | tr '\n' '|')]"; fi

# 14. A line longer than the old 1024-byte buffer is not truncated.
printf 'a = input_line()\nprint(a.len())\n' > "$TMP/inp3.wyn"
long=$(perl -e 'print "x" x 5000')
out=$(printf '%s\n' "$long" | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/inp3.wyn" 2>&1)
if echo "$out" | grep -q '^5000$'; then ok "long input_line is not truncated"
else bad "long input_line is not truncated [$(echo "$out" | tail -1)]"; fi

# 15. input_line() in a value position (checker must type it string, not int).
printf 'print(input_line().len())\nprint(input_line().upper())\n' > "$TMP/inp4.wyn"
out=$(printf 'abcd\nefg\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/inp4.wyn" 2>&1)
got=$(echo "$out" | grep -v 'Compiled in\|Building\|Built')
if [ "$got" = "$(printf '4\nEFG')" ]; then ok "input_line() usable inline as a string"
else bad "input_line() usable inline as a string [$(echo "$got" | tr '\n' '|')]"; fi

# 16. EOF (no input at all) yields an empty string, not garbage or a crash.
printf 'a = input_line()\nprint(a.len())\nprint("[${a}]")\n' > "$TMP/inp5.wyn"
out=$(printf '' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/inp5.wyn" 2>&1); rc=$?
got=$(echo "$out" | grep -v 'Compiled in\|Building\|Built')
if [ $rc -eq 0 ] && [ "$got" = "$(printf '0\n[]')" ]; then ok "input_line at EOF is the empty string"
else bad "input_line at EOF is the empty string (rc=$rc) [$(echo "$got" | tr '\n' '|')]"; fi

# --- input() / input_float() ----------------------------------------------
# input() reads an INTEGER via scanf("%d"). Two defects shared one error path:
#   (a) a non-numeric line returned 0 at exit 0 - the silent-wrong class this
#       whole file exists to close, on the first line of any tutorial program;
#   (b) the buffer drain was `while (getchar() != '\n')`, which at EOF spins on
#       getchar() returning EOF forever - an UNBOUNDED HANG on empty stdin.
# Both now panic like to_int does (str_parse_int), naming input_line() for text,
# with WYN_LENIENT=1 restoring the old return-0 behavior. Every case below is
# wrapped in alarm() because (b) was a hang: a regression here must FAIL, not
# wedge the suite.

# 17. A non-numeric line panics instead of silently yielding 0.
printf 'n = input()\nprint("got: ${n}")\n' > "$TMP/in_int.wyn"
out=$(printf 'hello\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_int.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q 'input(): "hello" is not a valid integer' \
   && echo "$out" | grep -q 'input_line()' && ! echo "$out" | grep -q 'got: 0'; then
    ok "input() on non-numeric panics and names input_line()"
else bad "input() on non-numeric panics and names input_line() (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 18. EOF terminates instead of hanging. rc=142 would be the alarm firing.
printf 'n = input()\nprint("got: ${n}")\n' > "$TMP/in_eof.wyn"
out=$(printf '' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_eof.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && [ $rc -ne 142 ] && echo "$out" | grep -q 'input(): end of input'; then
    ok "input() at EOF panics instead of hanging"
else bad "input() at EOF panics instead of hanging (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 19. Positive: a valid integer still reads correctly (guard the happy path).
out=$(printf '42\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_int.wyn" 2>&1); rc=$?
got=$(echo "$out" | grep -v 'Compiled in\|Building\|Built')
if [ $rc -eq 0 ] && [ "$got" = "got: 42" ]; then ok "input() still reads a valid integer"
else bad "input() still reads a valid integer (rc=$rc) [$(echo "$got" | tr '\n' '|')]"; fi

# 20. WYN_LENIENT=1 restores the old behavior, exactly as it does for to_int.
out=$(printf 'hello\n' | WYN_LENIENT=1 perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_int.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'got: 0'; then ok "WYN_LENIENT=1 restores input()'s old 0"
else bad "WYN_LENIENT=1 restores input()'s old 0 (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 21. input_float() shares the error path and must be fixed with it.
printf 'x = input_float()\nprint("got: ${x}")\n' > "$TMP/in_flt.wyn"
out=$(printf 'abc\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_flt.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q 'input_float(): "abc" is not a valid float'; then
    ok "input_float() on non-numeric panics"
else bad "input_float() on non-numeric panics (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 22. input_float() at EOF terminates rather than hanging (same drain loop).
out=$(printf '' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_flt.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && [ $rc -ne 142 ] && echo "$out" | grep -q 'input_float(): end of input'; then
    ok "input_float() at EOF panics instead of hanging"
else bad "input_float() at EOF panics instead of hanging (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 23. Positive: a valid float still reads correctly.
out=$(printf '2.5\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_flt.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'got: 2.5'; then ok "input_float() still reads a valid float"
else bad "input_float() still reads a valid float (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 24. A trailing non-numeric TAIL is a parse error too, not a silently accepted
#     prefix: scanf("%d") on "12abc" consumes 12 and returns 1, so the tail was
#     swallowed. This is the same class as to_int's `end != '\0'` check.
out=$(printf '12abc\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_int.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q 'input():'; then ok "input() rejects a trailing non-numeric tail"
else bad "input() rejects a trailing non-numeric tail (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 26. input() must not leave its newline in the buffer: scanf("%d") did, so a
#     FOLLOWING input_line() returned "" instead of the next line, at exit 0.
printf 'n = input()\nrest = input_line()\nprint("n=${n} rest=[${rest}]")\n' > "$TMP/in_mix.wyn"
out=$(printf '1\ntwo\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_mix.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'n=1 rest=\[two\]'; then
    ok "input() then input_line() reads the NEXT line"
else bad "input() then input_line() reads the NEXT line (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 27. A Wyn int is 64-bit, but the runtime declared `int input()` (C 32-bit), so
#     4294967297 came back as 1 - a wrong answer at exit 0, not an overflow error.
out=$(printf '4294967297\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_int.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'got: 4294967297'; then ok "input() reads a full 64-bit int"
else bad "input() reads a full 64-bit int (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 28. A Wyn float is a C double, but the runtime declared `float input_float()`,
#     so 0.1234567890123 came back as 0.12345679104328156 (float32 rounding).
#     Same class as the v1.20.0 float-printing round-trip fix, in the stdin path.
out=$(printf '0.1234567890123\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_flt.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'got: 0.1234567890123$'; then ok "input_float() keeps double precision"
else bad "input_float() keeps double precision (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 29. An int that genuinely overflows 64 bits is an ERROR, not a clamp or a wrap
#     (to_int's ERANGE arm, same posture).
out=$(printf '99999999999999999999\n' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_int.wyn" 2>&1); rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q 'input(): .* does not fit'; then ok "input() rejects a 64-bit overflow"
else bad "input() rejects a 64-bit overflow (rc=$rc) [$(echo "$out" | tail -1)]"; fi

# 25. Guard the shadowing contract: a USER function named `input` must still win
#     over the builtin (tests/expect/test_user_function_names.wyn relies on it,
#     and the panic must not fire for it).
printf 'fn input(prompt: string) -> string {\n  return "u:" + prompt\n}\nprint(input("hi"))\n' \
    > "$TMP/in_shadow.wyn"
out=$(printf '' | perl -e 'alarm(20); exec @ARGV' -- "$WYN" run "$TMP/in_shadow.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q '^u:hi$'; then ok "a user fn named input still shadows the builtin"
else bad "a user fn named input still shadows the builtin (rc=$rc) [$(echo "$out" | tail -1)]"; fi

echo ""; echo "silent-wrong: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
