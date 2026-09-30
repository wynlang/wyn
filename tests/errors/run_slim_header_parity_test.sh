#!/bin/bash
# Every runtime function a Wyn call can be lowered to must be declared in BOTH runtime
# headers - not just the one a debug build uses.
#
#   a = [1, 2, 3]
#   print(a.every(fn(x: int) -> bool { return x > 0 }))
#       # wyn run            -> true
#       # wyn run --release  -> internal codegen error: undeclared 'array_every'
#
#   3.times(nop)
#       # wyn run            -> runs
#       # wyn run --release  -> internal codegen error: undeclared 'int_times'
#
# `--release` is the one path that emits src/wyn_runtime_slim.h instead of
# wyn_runtime.h, and that header is maintained BY HAND. A function can therefore be
# defined in wyn_runtime.h, present in runtime/libwyn_rt.a, reachable from ordinary Wyn
# code, and still fail to compile in release because nothing declared it.
#
# WHY THIS IS A TEXT CHECK AND NOT MORE COMPILES. The existing registry-reachable gate
# compiles a real call per method in both modes, which is the strongest evidence
# available - but it can only generate calls for the ARITY-0 rows, because
# method_signatures records an argument COUNT and not argument TYPES. Both defects above
# take a function argument, so both sat in that blind spot: `.any`/`.all` were found only
# by hand, and `array_every`/`int_times` survived the PR that fixed those two.
#
# This check needs no call at all. It reads the three files that already know the answer:
#
#   wyn_runtime.h        defines the function (so the archive has it)
#   types.c / codegen.c  name it as a lowering target (so Wyn code can reach it)
#   wyn_runtime_slim.h   must therefore declare it
#
# It is O(1) compiles, runs in under a second, and covers every arity - which is exactly
# the coverage the call-generating gate cannot reach until method_signatures grows an
# argument-type column.
#
# A name legitimately absent from the slim header goes in the ALLOWED list below, with a
# reason. That list is checked for exactness: an entry that stops being missing fails the
# gate, so it cannot rot. (2026-09)
set -uo pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

for f in src/wyn_runtime.h src/wyn_runtime_slim.h src/types.c src/codegen.c \
         src/codegen_expr.c src/codegen_stmt.c src/codegen_program.c src/codegen_lambda.c; do
  if [ ! -f "$ROOT/$f" ]; then
    bad "cannot read $f - this gate is vacuous without it"
    echo ""; echo "slim-header-parity: $PASS pass, $FAIL fail"; exit 1
  fi
done

/usr/bin/env python3 - "$ROOT" <<'PY'
import re, sys, os
root = sys.argv[1]
def rd(p): return open(os.path.join(root, p)).read()
full, slim = rd('src/wyn_runtime.h'), rd('src/wyn_runtime_slim.h')
# EVERY codegen translation unit that names an emitted symbol, not just codegen.c.
# codegen.c #includes codegen_expr.c / codegen_stmt.c / codegen_program.c /
# codegen_lambda.c, and MOST emit() calls live in those - so scanning codegen.c alone
# made this gate blind to the majority of lowering targets. `map_to_string` is emitted by
# codegen_expr.c, was defined in wyn_runtime.h, was absent from the slim header, and this
# gate passed anyway: `print(m)` rendered a map POINTER under --release while debug
# printed `{"a": 1}`, and `"${m}"` did not compile at all. Widening the scan is the fix
# for the CLASS; adding one declaration would only have fixed the instance.
types = rd('src/types.c')
cg = ''.join(rd(f) for f in ('src/codegen.c', 'src/codegen_expr.c', 'src/codegen_stmt.c',
                             'src/codegen_program.c', 'src/codegen_lambda.c'))

# Names legitimately absent from the slim header. Each needs a reason, and the exactness
# check below fails if one of these stops being missing.
ALLOWED = {
    # (none today - both known omissions were fixed when this gate was written)
}

defs  = set(re.findall(r'^(?:[A-Za-z_][A-Za-z0-9_ *]*?)\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{]*\)\s*\{',
                       full, re.M))
# Any mention in the slim header counts: a declaration, or a `static inline` definition
# (some names MUST be duplicated rather than declared - int_to_int, wyn_malloc - because
# there is nothing in the archive to link against).
slim_names = set(re.findall(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', slim))

# Every STRING LITERAL in the dispatch tables and the codegen units, concatenated. The
# reachability test below searches this, not the raw source, and that distinction is the
# whole point of the 2026-09-30 widening.
#
# The old test asked whether the exact token `"name"` - quotes included - appeared in the
# source. That only matches a name that is a string literal ALL BY ITSELF, which is true
# for a dispatch-table row and false for most emissions: codegen writes
#     emit("({ const char* __pms = map_to_string(")
# so the name is in the MIDDLE of a longer literal and the old test could not see it.
# `map_to_string` was consequently invisible: defined in wyn_runtime.h, emitted by
# codegen_expr.c, absent from the slim header, and this gate passed - while `print(m)`
# rendered a map POINTER under --release (debug printed `{"a": 1}`) and `"${m}"` did not
# compile at all. Widening the FILE list alone did not fix it; the matching rule was the
# blind spot.
_LIT = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
_literal_text = " ".join(_LIT.findall(types) + _LIT.findall(cg))

def reachable(n):
    # A whole-word hit inside any emitted or tabled string literal means Wyn source can
    # reach this symbol. Whole-word so `array_get` does not vouch for `array_get_str`.
    return re.search(r'(?<![A-Za-z0-9_])' + re.escape(n) + r'(?![A-Za-z0-9_])',
                     _literal_text) is not None

missing = sorted(n for n in defs if n not in slim_names and reachable(n))
unexpected = [n for n in missing if n not in ALLOWED]
stale      = [n for n in ALLOWED if n not in missing]

print(f"  scanned {len(defs)} functions defined in wyn_runtime.h")
for n in unexpected:
    print(f"  FAIL  {n} is defined in wyn_runtime.h and reachable from Wyn code, "
          f"but wyn_runtime_slim.h does not declare it")
for n in stale:
    print(f"  FAIL  {n} is in this gate's ALLOWED list but is no longer missing - "
          f"remove it from the list")
if not unexpected and not stale:
    print("  ok    every reachable runtime function is declared in both headers")
sys.exit(1 if (unexpected or stale) else 0)
PY
rc=$?
if [ $rc -eq 0 ]; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi

echo ""; echo "slim-header-parity: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
