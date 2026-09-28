#!/bin/bash
# Synthesized-Expr initialisation gate for the parallel{} lowering.
#
# THE BUG THIS GATES: codegen_stmt.c's STMT_PARALLEL lowering fabricates EXPR_SPAWN
# Exprs on the stack - for the implicit-spawn form (`var x = user_fn()`) and for a
# bare `spawn f()` inside the block - assigning only `type`, `spawn.call` and
# `_codegen_temp_id`. codegen_expr() calls cg_expr_is_bool_typed() on EVERY
# expression, which dereferences `expr->expr_type`. On a partially-initialised
# Expr that field is stack residue, so the compiler dereferenced a garbage
# pointer and died of SIGSEGV while compiling the user's program.
#
# WHY THE OBVIOUS TEST IS NOT ENOUGH, and why this file has two parts:
# whether stack residue happens to be benign depends on the optimisation level of
# the COMPILER ITSELF. The repro below segfaulted 5/5 with the shipped `-O2`
# compiler and 0/5 with a plain `make` (`-g`) build - and `make test` runs the
# `-g` build. So part 1 alone is VACUOUS in the normal test run: it passes whether
# or not the bug is present. It is kept because it is the real behavioural check
# and it does bite when the suite is run against a release-configured compiler.
#
# Part 2 is what makes this gate real at any optimisation level: a static
# assertion that no synthesized Expr in these two lowering paths is left
# uninitialised. Verified to fail when either zero-initialiser is removed.
#
# (A deterministic behavioural gate is also possible - building the compiler with
# `-ftrivial-auto-var-init=pattern` makes the unfixed compiler crash 3/3 even at
# -O0 - but that needs a compiler rebuild, which belongs in CI, not here.)
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "parallel{} synthesized-Expr initialisation gate"

# --- Part 1: behavioural. Both block forms, in the order that crashed. ---------
# An implicit-spawn parallel block followed by a bare-spawn parallel block. The
# ORDER matters: the reverse order never crashed, because the bare-spawn block's
# wrapper collection runs before the implicit-spawn lowering has dirtied the frame.
cat > "$TMP/both_forms.wyn" <<'EOF'
fn work(s: int) -> int { return s * 2 }
fn main() -> int {
    parallel { var b1 = work(1) }
    parallel { spawn work(1) }
    print("ok")
    return 0
}
EOF
for mode in "" "--release"; do
    label="wyn build ${mode:-(dev)} of implicit-spawn block then bare-spawn block"
    out=$(perl -e 'alarm(120); exec @ARGV' -- "$WYN" build $mode "$TMP/both_forms.wyn" 2>&1); rc=$?
    # rc 139 / 11 is the SIGSEGV this gate exists for; any nonzero rc is a failure.
    if [ $rc -eq 0 ]; then ok "$label"
    else bad "$label (rc=$rc)$([ $rc -ge 128 ] && echo ' <-- compiler CRASHED')"; fi
    rm -f "$TMP/both_forms" "$TMP/both_forms.wyn.c"
done

# The reverse order, which must also keep working.
cat > "$TMP/reversed.wyn" <<'EOF'
fn work(s: int) -> int { return s * 2 }
fn main() -> int {
    parallel { spawn work(1) }
    parallel { var b1 = work(1) }
    print("ok")
    return 0
}
EOF
out=$(perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$TMP/reversed.wyn" 2>&1); rc=$?
if [ $rc -eq 0 ]; then ok "reverse order (bare-spawn block first)"
else bad "reverse order (rc=$rc)"; fi

# --- Part 2: static. Every synthesized Expr must be zero-initialised. ---------
# This is the arm that can actually fail in a normal `-g` test run.
for f in src/codegen_stmt.c src/codegen_lambda.c; do
    # Match a bare `Expr <name>;` or `Expr <name>[N];` local declaration - i.e. one
    # with no initialiser. Pointers (`Expr* e;`) are not the hazard: nothing reads
    # through them until they are assigned a real Expr.
    hits=$(grep -nE '^[[:space:]]*Expr[[:space:]]+[A-Za-z_][A-Za-z0-9_]*(\[[0-9]+\])?[[:space:]]*;' \
             "$ROOT/$f" || true)
    if [ -z "$hits" ]; then ok "$f: no uninitialised synthesized Expr"
    else
        bad "$f: uninitialised Expr declaration(s) - add = {0}"
        echo "$hits" | sed 's/^/          /'
    fi
done

echo "  $PASS pass, $FAIL fail"
[ $FAIL -eq 0 ]
