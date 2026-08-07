#!/usr/bin/env bash
# `import {x} from m as u` must be rejected with a message about the SYNTAX.
#
# THE DEFECT
#
# The combination has never been supported: an alias renames a whole MODULE, while a
# selective import binds the names directly, so there is nothing for the alias to
# attach to. But the selective-import parse simply ENDED at the module name, leaving
# `as u` to be parsed as two expression statements:
#
#   $ wyn check main.wyn         # main.wyn: import {helper} from util as u
#   Error at line 1: Undefined variable 'as'
#     Did you mean: Os?
#   Error at line 1: Undefined variable 'u'
#     Did you mean: Db?
#
# Two errors about variables that do not exist, plus two nonsense suggestions, for
# what is a syntax question. Nothing tells the user which form to write instead.
#
# BOTH parse paths need the check. The selective-import grammar is implemented twice
# (statement_impl and the program-level loop) as near-duplicates, so a one-site fix
# leaves half of all programs still reporting "Undefined variable 'as'". That is why
# the fix is a shared helper and why this test exercises the form at top level AND
# inside a nested position.
#
# WHY A SHELL TEST AND NOT AN EXPECT FILE
#
# What is under test is the exit status and the TEXT of a diagnostic for a program
# that must NOT compile, plus a sibling module file. An EXPECT file in
# tests/regression/ runs under `wyn run` and must succeed, so it can express neither.
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

cat > util.wyn <<'EOF'
export fn helper() -> int {
    return 42
}
EOF

# The unsupported combination, at TOP LEVEL (the program-level parse path).
cat > bad.wyn <<'EOF'
import {helper} from util as u

fn main() {
    print("${helper()}")
}
EOF

# The same thing in STATEMENT position, inside a function body. This goes through the
# OTHER parse path (statement_impl), and it is genuinely a separate arm: with the
# statement-position check removed, the top-level cases above all still pass while this
# one reports "Undefined variable 'as'" again. Verified by mutation, not assumed -- my
# first version of this test covered only the top-level form and would have let a
# one-site fix through.
cat > bad_nested.wyn <<'EOF'
fn main() {
    import {helper} from util as u
    print("${helper()}")
}
EOF

# The two forms that ARE supported must keep working - a fix that rejected the alias
# too eagerly would break these, and they are the forms the message recommends.
cat > ok_selective.wyn <<'EOF'
import {helper} from util

fn main() {
    print("${helper()}")
}
EOF

cat > ok_alias.wyn <<'EOF'
import util as u

fn main() {
    print("${u.helper()}")
}
EOF

# --- the bad form is rejected, by every entry point ---------------------------

# Both parse paths, checked identically.
for prog in bad bad_nested; do
    for cmd in check build run; do
        "$WYN_ABS" $cmd "$prog.wyn" >/dev/null 2>&1
        check "$prog: wyn $cmd exits nonzero" "$([ $? -ne 0 ] && echo yes || echo no)" "yes"
    done

    msg=$("$WYN_ABS" check "$prog.wyn" 2>&1 || true)

    # The message names the actual problem.
    check "$prog: names 'as' and the selective import" \
        "$(echo "$msg" | grep -cE "'as' cannot be combined with a selective import")" "1"

    # And it names BOTH working forms, so the user does not have to guess which to use.
    check "$prog: suggests the selective form" \
        "$(echo "$msg" | grep -cF 'import {a, b} from m')" "1"
    check "$prog: suggests the module-alias form" \
        "$(echo "$msg" | grep -cF 'import m as u')" "1"

    # The old misleading output must be gone. This is the regression that matters: the
    # error was not absent before, it was WRONG.
    check "$prog: no 'Undefined variable' error" \
        "$(echo "$msg" | grep -c "Undefined variable")" "0"
    check "$prog: no bogus 'Did you mean' suggestion" \
        "$(echo "$msg" | grep -cE "Did you mean: (Os|Db)")" "0"

    # No half-generated artifact left behind.
    check "$prog: no generated .c is left behind" \
        "$(ls "$prog.wyn.c" 2>/dev/null | wc -l | tr -d ' ')" "0"
done

# --- the supported forms still work ------------------------------------------

for f in ok_selective ok_alias; do
    "$WYN_ABS" check "$f.wyn" >/dev/null 2>&1
    check "$f: wyn check exits zero" "$([ $? -eq 0 ] && echo yes || echo no)" "yes"

    # Not just "checks clean" - it must RUN and produce the right answer, or the
    # import did not actually bind anything.
    check "$f: runs and prints 42" "$("$WYN_ABS" run "$f.wyn" 2>/dev/null | tail -1)" "42"
done

echo ""
echo "selective-import-alias: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
