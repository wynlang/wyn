#!/bin/bash
# Error POSITIONS inside string interpolation - V-14 (2026-09).
#
#     print("v=${s.bogus()}")          # on line 5
#     -> Error at line 1: struct 'S' has no method 'bogus'
#        with the caret drawn under `struct S { n: int }`
#
# The same expression written outside the `${}` reported line 5 correctly, so the
# position was lost by the interpolation path specifically: primary() extracts the
# text between `${` and `}` into a FRESH buffer and calls init_lexer() on it, which
# restarts line counting at 1. Every token of every interpolated sub-expression
# therefore carried line 1. `${}` is *the* idiomatic formatter in Wyn, so in practice
# most real diagnostics pointed at line 1 - three separate agents hit this while
# working on something else, which is the measure of how much noise it made.
#
# Two neighbouring sightings turned out to be a DIFFERENT root cause in the same
# family - AST nodes that never record a token at all, so token.line stays 0 from
# the calloc in alloc_expr():
#   * EXPR_STRUCT_INIT -> "Type mismatch at line 0:0"
#   * EXPR_LAMBDA      -> "Type mismatch at line 0:0" (and the reason an older
#                         `spawn` diagnostic said "lambda at line 0")
# Both are asserted here too, because "the line number is wrong" is one concern
# whichever half produces it.
#
# Every arm asserts an EXACT line number on a line >= 3, and additionally asserts
# that neither `line 1` nor `line 0` appears - so the arm cannot pass on the old
# behaviour, and an off-by-one cannot pass either (the two-interpolation arm pins
# lines 5 AND 6 in one file, so a uniform +-1 shift reddens it).
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Run `wyn check` on $1 and require that:
#   - it FAILED (a line-number assertion on a program that checks clean is vacuous)
#   - the ERROR diagnostics cite "line <N>" for every N in $2 (space-separated)
#   - they cite NO other line number at all (so line 1 / line 0 / N+-1 all fail)
# Warnings are excluded on purpose (the unused-variable warning carries its own,
# unrelated line); each fixture is written so the errors do not cascade.
# $3 is the arm description.
want_lines() {
    local f="$1" wants="$2" desc="$3"
    local out rc got n
    out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" check "$f" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then
        bad "$desc (checked CLEAN - nothing to locate)"; return
    fi
    # Every "line <N>" the ERROR diagnostics mention, de-duplicated, ascending.
    got=$(printf '%s\n' "$out" | grep 'Error' | grep -oE 'line [0-9]+' | sed 's/line //' | sort -un | tr '\n' ' ')
    got="${got% }"
    local want_sorted
    want_sorted=$(printf '%s\n' $wants | sort -un | tr '\n' ' '); want_sorted="${want_sorted% }"
    if [ "$got" = "$want_sorted" ]; then
        ok "$desc (line $got)"
    else
        bad "$desc: wanted line(s) [$want_sorted], diagnostics said [$got]"
        printf '%s\n' "$out" | grep -iE 'error' | head -3 | sed 's/^/        /'
    fi
}

# ---------------------------------------------------------------- inside ${}

# 1. Unknown method on a struct, inside ${}, on line 5.
cat > "$TMP/method.wyn" <<'EOF'
struct S { n: int }

fn main() {
  s = S { n: 1 }
  print("v=${s.bogus()}")
}
EOF
want_lines "$TMP/method.wyn" 5 "unknown method inside \${} -> line 5"

# 2. A TYPE error inside ${}: wrong argument type to a known function, line 5.
cat > "$TMP/type.wyn" <<'EOF'
fn add(a: int, b: int) -> int { return a + b }

fn main() {

  print("v=${add("x", 3)}")
}
EOF
want_lines "$TMP/type.wyn" 5 "type error inside \${} -> line 5"

# 3. An undefined variable inside ${}, on line 4.
cat > "$TMP/undef.wyn" <<'EOF'
fn main() {


  print("v=${nope}")
}
EOF
want_lines "$TMP/undef.wyn" 4 "undefined variable inside \${} -> line 4"

# 4. Nested interpolation: the error is in the INNER ${} of an interpolated string
#    literal, which re-enters the same code path one level down.
cat > "$TMP/nested.wyn" <<'EOF'
fn main() {


  print("v=${"inner=${nope}"}")
}
EOF
want_lines "$TMP/nested.wyn" 4 "nested interpolation -> line 4"

# 4b. The namespace rule (#357 for `Ns.m()`, #369 for `Ns::m()`) is a separate
#     diagnostic path from the struct/value receivers above, and BOTH spellings of it
#     resolve through their own code - so both are pinned inside a `${}`.
cat > "$TMP/ns_dot.wyn" <<'EOF'
fn main() {


  print("t=${Time.nosuchmethod()}")
}
EOF
want_lines "$TMP/ns_dot.wyn" 4 "unknown Ns.method inside \${} -> line 4"

cat > "$TMP/ns_colon.wyn" <<'EOF'
fn main() {


  print("t=${Time::nosuchmethod()}")
}
EOF
want_lines "$TMP/ns_colon.wyn" 4 "unknown Ns::method inside \${} -> line 4"

# 5. TWO interpolations on DIFFERENT lines in ONE file. A constant offset (the old
#    "always 1") and an off-by-one both fail this arm.
cat > "$TMP/two.wyn" <<'EOF'
struct S { n: int }

fn main() {
  s = S { n: 1 }
  print("a=${s.bogus()}")
  print("b=${s.alsobogus()}")
}
EOF
want_lines "$TMP/two.wyn" "5 6" "two interpolations -> lines 5 and 6"

# 6. A multi-line """ string: the ${} is on line 4 of the file, three lines after
#    the token starts. Counting from the token's own start line is what makes this
#    work; counting from its END line would say 5.
cat > "$TMP/triple.wyn" <<'EOF'
fn main() {
  s = """
line2
v=${nope}
"""
  print(s)
}
EOF
want_lines "$TMP/triple.wyn" 4 "\${} inside a \"\"\" string -> line 4"

# 7. The format-spec rejection (raised by the parser, not the checker) must point at
#    the interpolation too - inside a """ string it used to report the string's END
#    line (6), not the ${}'s line (4).
cat > "$TMP/spec.wyn" <<'EOF'
fn main() {
  x = 1.5
  s = """
v=${x:.2}

"""
  print(s)
}
EOF
want_lines "$TMP/spec.wyn" 4 "unsupported format spec in a \"\"\" string -> line 4"

# ------------------------------------------- AST nodes that carried no token

# 8. A struct-init used where an int is wanted: the node reported line 0.
cat > "$TMP/structinit.wyn" <<'EOF'
struct S { n: int }
fn f(a: int) -> int { return a }
fn main() {
  y = f(S { n: 1 })
  print("${y}")
}
EOF
want_lines "$TMP/structinit.wyn" 4 "struct-init in the wrong slot -> line 4, not 0"

# 9. A lambda used where an int is wanted: the node reported line 0. All THREE
#    lambda spellings are separate constructions in primary(), i.e. three copies of
#    the same rule, so all three are pinned.
cat > "$TMP/lambda_arrow.wyn" <<'EOF'
fn f(a: int) -> int { return a }
fn main() {
  y = f((x: int) -> int => x)
  print("${y}")
}
EOF
want_lines "$TMP/lambda_arrow.wyn" 3 "(x) => lambda in the wrong slot -> line 3, not 0"

cat > "$TMP/lambda_pipe.wyn" <<'EOF'
fn f(a: int) -> int { return a }
fn main() {

  y = f(|x| x)
  print("${y}")
}
EOF
want_lines "$TMP/lambda_pipe.wyn" 4 "|x| lambda in the wrong slot -> line 4, not 0"

cat > "$TMP/lambda_fn.wyn" <<'EOF'
fn f(a: int) -> int { return a }
fn main() {


  y = f(fn(x: int) -> int { return x })
  print("${y}")
}
EOF
want_lines "$TMP/lambda_fn.wyn" 5 "fn(x) lambda in the wrong slot -> line 5, not 0"

# ------------------------------------------------- no-regression: outside ${}

# 10. The same three errors written OUTSIDE an interpolation must keep the lines they
#     already reported correctly - the fix must not shift them.
cat > "$TMP/out_method.wyn" <<'EOF'
struct S { n: int }

fn main() {
  s = S { n: 1 }
  x = s.bogus()
  print("v=${x}")
}
EOF
want_lines "$TMP/out_method.wyn" 5 "unknown method OUTSIDE \${} still line 5"

cat > "$TMP/out_type.wyn" <<'EOF'
fn add(a: int, b: int) -> int { return a + b }

fn main() {

  x = add("x", 3)
  print("v=${x}")
}
EOF
want_lines "$TMP/out_type.wyn" 5 "type error OUTSIDE \${} still line 5"

cat > "$TMP/out_undef.wyn" <<'EOF'
fn main() {


  print(nope)
}
EOF
want_lines "$TMP/out_undef.wyn" 4 "undefined variable OUTSIDE \${} still line 4"

# 11. THE CARET, not just the number. A right line number with the wrong excerpt is
#     still a wrong diagnostic, and the caret is what the reader actually looks at.
#
#     COLUMNS: Token (src/common.h) has no column field at all - only type/start/
#     length/line - so there is no column for this fix to get wrong. Every
#     type_error_mismatch() call passes a literal 0, and show_source_line() underlines
#     the WHOLE source line. Sub-column precision is therefore out of scope here; what
#     IS assertable is that the quoted line and the caret run under it belong to the
#     offending line, which is what this arm pins.
cat > "$TMP/caret.wyn" <<'EOF'
struct S { n: int }

fn main() {
  s = S { n: 1 }
  print("v=${s.bogus()}")
}
EOF
out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" check "$TMP/caret.wyn" 2>&1); rc=$?
# Strip ANSI so the excerpt and the caret run can be matched literally.
plain=$(printf '%s\n' "$out" | sed $'s/\033\\[[0-9;]*m//g')
excerpt=$(printf '%s\n' "$plain" | grep -E '^ +5 \| ')
carets=$(printf '%s\n' "$plain" | grep -E '^ +\^+$' | head -1 | tr -d ' ')
src_line='  print("v=${s.bogus()}")'
if [ $rc -ne 0 ] &&
   [ "$excerpt" = "     5 | $src_line" ] &&
   [ ${#carets} -eq ${#src_line} ] &&
   ! printf '%s\n' "$plain" | grep -qE '^ +1 \| '; then
    ok "the caret quotes line 5's own text (${#carets} carets under ${#src_line} chars)"
else
    bad "caret/excerpt wrong: excerpt=[$excerpt] carets=${#carets} want=${#src_line}"
    printf '%s\n' "$plain" | head -4 | sed 's/^/        /'
fi

# 12. A program whose ONLY interpolations are correct must still check clean and run -
#     the position bookkeeping must not change what parses.
cat > "$TMP/good.wyn" <<'EOF'
struct S { n: int }
fn main() {
  s = S { n: 7 }
  a = "x"
  print("v=${s.n} a=${a} nested=${"in=${a}"}")
  print("""
multi=${s.n}
""")
  print("escaped=\${a}")
}
EOF
out=$(perl -e 'alarm(30); exec @ARGV' -- "$WYN" run "$TMP/good.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ] &&
   printf '%s' "$out" | grep -q 'v=7 a=x nested=in=x' &&
   printf '%s' "$out" | grep -q 'multi=7' &&
   printf '%s' "$out" | grep -q 'escaped=\${a}'; then
    ok "valid interpolations (nested, triple-quoted, escaped) still compile and print"
else
    bad "valid interpolations broke (rc=$rc)"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "interp-error-line: $PASS pass, 0 fail"
    exit 0
fi
echo "interp-error-line: $PASS pass, $FAIL fail"
exit 1
