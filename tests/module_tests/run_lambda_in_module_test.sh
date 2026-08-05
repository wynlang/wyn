#!/usr/bin/env bash
# A lambda inside an imported module must work, and must not steal another
# lambda's body.
#
# THE DEFECT
#
# Two counters decided which body a `(n) => ...` reference meant:
# lambda_id_counter, advanced while SCANNING for bodies, and lambda_ref_counter,
# advanced while EMITTING references. They agree only if scan order equals
# emission order, and with an imported module it does not:
#
#   1. The pre-scan walked prog->stmts only (codegen_program.c), and a
#      whole-module `import m` deliberately does NOT merge module fns into
#      prog->stmts (module_loader.c) -- module bodies are emitted separately via
#      emit_function_with_prefix. So a lambda in a module fn was never scanned,
#      no body was generated, and the reference named a __lambda_N that did not
#      exist. The user saw a C compiler error about a symbol absent from their
#      own source, which reads as an internal compiler error.
#
#   2. Worse, where a module lambda WAS reachable through a merged statement it
#      got a HIGH scan id (merged module statements are appended to the end of
#      prog->stmts) but a LOW emission id (module bodies are emitted before the
#      main file's functions). The orders are inverted, so a module lambda and a
#      main-file lambda swapped bodies: a WRONG ANSWER at exit 0, which is worse
#      than the compile error.
#
# THE FIX
#
# References are bound by AST pointer through lambda_functions[i].ast -- which
# the definition side already stored and the body emitter already walked -- so
# there is no positional counter left to disagree with. The pre-scan also walks
# module ASTs in the same order the emitter does, and scan_stmt_for_lambdas
# unwraps STMT_EXPORT like its sibling veto_scan_stmt always has.
#
# WHY IT MATTERS BEYOND THE ERROR MESSAGE
#
# This withdrew the entire higher-order toolkit from all multi-file Wyn code:
# `.map`/`.filter` appear ZERO times in wyncanvas's layer.wyn, history.wyn and
# gui's widgets.wyn. Fixing it is what lets multi-file Wyn read like Wyn.
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
mkdir -p src

cat > src/m.wyn <<'WYN'
pub fn evens(nums: [int]) -> [int] {
    return nums.filter((n) => n % 2 == 0)
}
pub fn tripled(nums: [int]) -> [int] {
    return nums.map((n) => n * 3)
}
WYN

# 1. The base case: one lambda, inside one module fn. This alone used to emit a
#    reference to a body that was never generated.
cat > one.wyn <<'WYN'
import m
fn main() -> int {
    var a = m.evens([1, 2, 3, 4])
    print(a[0])
    return 0
}
WYN
check "a lambda works inside an imported module" \
    "$("$WYN_ABS" run one.wyn 2>/dev/null | tail -1)" "2"

# 2. TWO module lambdas, both called. An off-by-one in the id mapping cannot
#    survive two distinct bodies with distinguishable results.
cat > two.wyn <<'WYN'
import m
fn main() -> int {
    var a = m.evens([1, 2, 3, 4])
    var b = m.tripled([1, 2, 3, 4])
    print(a[0])
    print(b[0])
    return 0
}
WYN
out="$("$WYN_ABS" run two.wyn 2>/dev/null)"
check "two module lambdas keep their own bodies (first)" \
    "$(echo "$out" | sed -n '1p')" "2"
check "two module lambdas keep their own bodies (second)" \
    "$(echo "$out" | sed -n '2p')" "3"

# 3. The crossed-ids case: module lambdas AND a main-file lambda in one program.
#    This is where the two counters ran in opposite directions. `+ 100` is chosen
#    so that receiving the wrong body gives a visibly wrong number rather than a
#    plausible one.
cat > mixed.wyn <<'WYN'
import m
fn main() -> int {
    var xs = [1, 2, 3, 4]
    var a = m.evens(xs)
    var b = m.tripled(xs)
    var c = xs.map((n) => n + 100)
    print(a[0])
    print(b[0])
    print(c[0])
    return 0
}
WYN
out="$("$WYN_ABS" run mixed.wyn 2>/dev/null)"
check "module and main-file lambdas do not swap bodies (module filter)" \
    "$(echo "$out" | sed -n '1p')" "2"
check "module and main-file lambdas do not swap bodies (module map)" \
    "$(echo "$out" | sed -n '2p')" "3"
check "module and main-file lambdas do not swap bodies (main map)" \
    "$(echo "$out" | sed -n '3p')" "101"

# 4. A selective import brings the module fn in as a STMT_EXPORT wrapper, which
#    the lambda scanner did not unwrap (its sibling veto_scan_stmt always did).
#    The scanner now does, but this case cannot be asserted end-to-end yet: a
#    SELECTIVELY imported fn declared `-> [int]` is still typed as `int` by the
#    checker, independently of lambdas --
#
#        export fn plain(nums: [int]) -> [int] { return [7, 8] }
#        import { plain } from e2 ... plain([5,6])[0]
#            -> Type mismatch: Expected int, Got array
#
#    with no lambda anywhere in the program. That is the same
#    module-fn-return-type family as run_struct_array_return_test.sh, not this
#    defect, so asserting the value here would record an unrelated bug as this
#    one's contract. Assert the part that IS this defect: that the lambda body
#    gets emitted, i.e. the failure is the type error and NOT a missing
#    __lambda_N symbol.
cat > src/e.wyn <<'WYN'
export fn doubled(nums: [int]) -> [int] {
    return nums.map((n) => n * 2)
}
WYN
cat > sel.wyn <<'WYN'
import { doubled } from e
fn main() -> int {
    var d = doubled([5, 6])
    print(d[0])
    return 0
}
WYN
sel_err="$("$WYN_ABS" run sel.wyn 2>&1)"
if echo "$sel_err" | grep -q "unregistered lambda\|__lambda_"; then
    check "a lambda in a selectively-imported fn is registered" "unregistered" "registered"
else
    check "a lambda in a selectively-imported fn is registered" "registered" "registered"
fi

echo
echo "Lambda-in-module: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
