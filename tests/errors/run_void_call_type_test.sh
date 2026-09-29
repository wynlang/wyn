#!/bin/bash
# A void function's call is typed `void` at CHECK time, not `int`.
#
# `wyn check` is advertised as the fast type-check oracle, so a file it passes must not
# fail to BUILD for a type reason. It did, for every un-annotated function:
#
#   fn side() { print("hi") }
#   var a = 0
#   a = side()        # wyn check: "no errors" (just "unused variable 'a'")
#                     # wyn build: error: assigning to 'long long' from incompatible
#                     #                   type 'void'
#
# Two passes disagreed. The signature pass registers every function BEFORE any body is
# checked and used a flat `int` DEFAULT when there was no `-> T` annotation; codegen reads
# fn->return_type (still NULL for a void body) and emits a C `void` signature. So the
# checker compared int against int and the C compiler got the void.
#
# The `-> void` spelling had the same split for a different reason: the checker's
# annotation chain knew int/string/float/bool/array and fell through to "look up a struct
# named void", finding nothing and keeping the int default.
#
# Note the ORIGINAL report (#408) blamed liveness - it said the error only appeared once
# the variable was read. That is not what gates it: the error fires for a dead variable
# too, as the `unused` arm below pins. What gated it was which KIND of callee: a builtin
# like `Time.sleep` is registered with a real void type and was always caught.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# rejects <label> <program> - `wyn check` must FAIL and name the void
rejects(){
  printf '%b\n' "$2" > "$TMP/$1.wyn"
  # Exit status taken from wyn itself, not from a pipeline: `wyn check | sed` would report
  # sed's status, and this whole test would then pass no matter what the checker did.
  "$WYNABS" check "$TMP/$1.wyn" > "$TMP/$1.out" 2>&1; rc=$?
  out=$(sed 's/\x1b\[[0-9;]*m//g' "$TMP/$1.out")
  if [ "$rc" -eq 0 ]; then
    bad "$1: wyn check PASSED a void-to-int assignment (wyn build rejects it)"
  elif ! echo "$out" | grep -q "void"; then
    bad "$1: rejected, but the message never says void :: $(echo "$out" | head -1 | cut -c1-70)"
  else
    ok "$1"
  fi
}

# accepts <label> <program> - must still check AND build clean
accepts(){
  printf '%b\n' "$2" > "$TMP/$1.wyn"
  if ! "$WYNABS" check "$TMP/$1.wyn" >/dev/null 2>&1; then
    bad "$1: wyn check rejected a correct program"
  elif ! "$WYNABS" run "$TMP/$1.wyn" 2>&1 | grep -q "REACHED"; then
    bad "$1: checked, then failed to build or run"
  else
    ok "$1"
  fi
}

echo "-- a void call assigned to an int variable is a CHECK error"
# The variable is never read here: the rule must not be gated on liveness.
rejects "unused" 'fn side() {\n  print("hi")\n}\nfn main() -> int {\n  var a = 0\n  a = side()\n  return 0\n}'
rejects "used"   'fn side() {\n  print("hi")\n}\nfn main() -> int {\n  var a = 0\n  a = side()\n  print("${a}")\n  return 0\n}'
rejects "explicit-void" 'fn side() -> void {\n  print("hi")\n}\nfn main() -> int {\n  var a = 0\n  a = side()\n  return 0\n}'
rejects "string-target" 'fn side() {\n  print("hi")\n}\nfn main() -> int {\n  var s = ""\n  s = side()\n  return 0\n}'

echo "-- a value-returning function is untouched, annotated or not"
accepts "inferred-int"   'fn f() {\n  return 5\n}\nfn main() -> int {\n  var a = 0\n  a = f()\n  print("REACHED")\n  return 0\n}'
accepts "annotated-int"  'fn f() -> int {\n  return 5\n}\nfn main() -> int {\n  var a = 0\n  a = f()\n  print("REACHED")\n  return 0\n}'
accepts "void-statement" 'fn side() {\n  print("hi")\n}\nfn main() -> int {\n  side()\n  print("REACHED")\n  return 0\n}'
# main is un-annotated and still emitted as `long long wyn_main()`, so a `return` inside it
# must not be judged against void.
accepts "bare-main"      'fn side() {\n  print("hi")\n}\nfn main() {\n  side()\n  print("REACHED")\n}'
# A value return inside a loop or a match still makes the function non-void: the
# signature-pass walk has to see those, or a working program would be rejected.
accepts "return-in-while" 'fn f() {\n  while true {\n    return 7\n  }\n  return 0\n}\nfn main() -> int {\n  var a = 0\n  a = f()\n  print("REACHED")\n  return 0\n}'

echo ""; echo "void-call-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
