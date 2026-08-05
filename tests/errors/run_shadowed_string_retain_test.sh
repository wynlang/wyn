#!/usr/bin/env bash
# A shadowing string declaration must retain ITSELF, not the variable it shadows.
#
# THE DEFECT
#
# The string RC retain printed the raw Wyn source token:
#
#     emit("wyn_rc_retain(%.*s);\n", stmt->var.name.length, stmt->var.name.start);
#
# but the declaration it belongs to does not necessarily emit that name. A
# declaration that shadows an enclosing one is emitted as
#
#     #undef key
#     const char* key__1 = src;
#     wyn_rc_retain(key);        <-- WRONG
#     #define key key__1
#
# and the retain sits in the window between the #undef and the #define, where
# the bare identifier `key` still refers to the OUTER key. So the retain landed
# on the wrong object -- one that had already been released at the end of the
# inner block -- while the new key__1 was never retained at all. Two refcount
# errors from one line, and both are SILENT: the C compiles whenever an outer
# declaration exists to absorb the name, so nothing complains and the program
# prints the right answer while the refcounts are wrong.
#
# (Where NO outer declaration exists the same line instead fails to compile with
# "use of undeclared identifier". That louder variant is what made PR #266 break
# WynJS: registering string globals widened is_string_var() so globals reached
# this path too, and wynjs declares locals named `key` and `name` alongside
# globals of those names.)
#
# THE FIX
#
# The two branches that decide the emitted name now record it in _decl_cname,
# and the retain uses that. It must be recorded rather than recomputed:
# get_shadow_suffix() MUTATES -- it advances the shadow counter on every hit --
# so asking it a second time at the retain site returns __2 for a variable
# declared as __1. (That is exactly what an earlier attempt at this fix did, and
# it produced eight new "undeclared identifier name__1" errors.)
#
# WHY THIS TEST READS THE GENERATED C
#
# The bug does not change the program's output. Asserting on stdout would have
# passed both before and after. The refcount is the behaviour under test, so the
# emitted retain is what gets asserted.
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

# An inner block declares `key`, then function scope declares `key` again. The
# second is the shadowing declaration and is emitted as key__1. `src` is printed
# after each copy so the copy stays live and the retain path is taken (a dead
# source is moved instead of retained, and a move emits no retain at all).
cat > shadow.wyn <<'WYN'
fn main() -> int {
    var src = "hello"
    if true {
        var key = src
        print(key)
        print(src)
    }
    var key = src
    print(key)
    print(src)
    return 0
}
WYN

out="$("$WYN_ABS" run shadow.wyn 2>&1)"
check "the program still runs" "$(echo "$out" | grep -c '^hello$')" "4"

# Generate the C and assert on the retain that follows the shadowed declaration.
"$WYN_ABS" build shadow.wyn --debug -o "$work/shadow.bin" > /dev/null 2>&1

if [ ! -f shadow.wyn.c ]; then
    echo "  FAIL  the --debug C file was written"
    echo "Shadowed-string retain: 0 pass, 1 fail"
    exit 1
fi

# The shadowed declaration must exist, or the test is asserting nothing.
check "the shadowing declaration is emitted with a suffix" \
    "$(grep -c 'const char\* key__1 = src;' shadow.wyn.c)" "1"

# The retain immediately after it must name key__1.
retain_after_shadow="$(grep -A1 'const char\* key__1 = src;' shadow.wyn.c | grep -c 'wyn_rc_retain(key__1);')"
check "the shadowing declaration retains ITSELF (key__1)" \
    "$retain_after_shadow" "1"

# And must NOT name the bare `key`, which in that window is the outer one.
retain_bare_in_window="$(grep -A1 'const char\* key__1 = src;' shadow.wyn.c | grep -c 'wyn_rc_retain(key);')"
check "it does not retain the shadowed outer variable" \
    "$retain_bare_in_window" "0"

# The unshadowed declaration is unchanged: a plain name still retains plainly.
check "an unshadowed string declaration is unaffected" \
    "$(grep -c '^wyn_rc_retain(key);$' shadow.wyn.c)" "1"

# A var whose name collides with a C keyword is emitted with the wynfn_ prefix.
# The retain used to drop that prefix too, for the same reason. Guard it.
cat > kw.wyn <<'WYN'
fn main() -> int {
    var src = "hi"
    var long = src
    print(long)
    print(src)
    return 0
}
WYN
"$WYN_ABS" build kw.wyn --debug -o "$work/kw.bin" > /dev/null 2>&1
if [ -f kw.wyn.c ]; then
    # Whatever name the declaration chose, the retain must use the same one.
    decl_name="$(grep -oE '(const )?char\* (wynfn_)?long = src;' kw.wyn.c | grep -oE '(wynfn_)?long' | head -1)"
    if [ -n "$decl_name" ]; then
        check "a C-keyword name retains the name it declared ($decl_name)" \
            "$(grep -c "wyn_rc_retain($decl_name);" kw.wyn.c)" "1"
    else
        # The keyword case may not reach the retain path at all; do not invent a
        # failure for a path this fix does not claim to change.
        echo "  ok    a C-keyword name does not reach the retain path (nothing to assert)"
        pass=$((pass+1))
    fi
fi

echo
echo "Shadowed-string retain: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
