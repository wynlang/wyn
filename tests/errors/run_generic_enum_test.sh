#!/bin/bash
# Generic enums are NOT yet monomorphized (generic STRUCTS are). Before this
# guard, `enum Opt<T> { None, Some(T) }` passed `wyn check` and then emitted
# broken C - the literal type parameter leaked into the output (`T Some_value;`,
# `Opt_Some(T value)`) and the build died with a raw "unknown type name 'T'".
# A single C enum type also cannot hold two different payload types for two
# instantiations. Until generic-enum monomorphization lands, reject cleanly at
# check time. (2026-07, fix/deeper-generics g3.)
#
# Companion coverage: generic STRUCTS returning/instantiating their own generic
# type, nested generic literals, and multi-param generic-fn returns all WORK -
# see tests/regression/generic_fn_returns_generic.wyn and generic_nested_literal.wyn.
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# single type-param generic enum -> clean check error
printf 'enum Opt<T> { None, Some(T) }\nfn main() { var o = Opt.Some(5) }\n' > "$TMP/one.wyn"
out=$("$WYN" check "$TMP/one.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "generic enums are not yet supported"; then
  ok "single-param generic enum: clean check error"
else bad "single-param generic enum: code=$code [$out]"; fi

# multi type-param generic enum -> same clean error (plural wording)
printf 'enum Either<A, B> { Left(A), Right(B) }\nfn main() { }\n' > "$TMP/two.wyn"
out=$("$WYN" check "$TMP/two.wyn" 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "generic enums are not yet supported"; then
  ok "multi-param generic enum: clean check error"
else bad "multi-param generic enum: code=$code [$out]"; fi

# a NON-generic enum with the same shape still compiles + runs
printf 'enum Opt { None, Some(int) }\nfn main() {\n  var o = Opt.Some(5)\n  match o {\n    Opt.Some(v) => { println("${v}") }\n    Opt.None => { println("none") }\n  }\n}\n' > "$TMP/concrete.wyn"
out=$("$WYN" build "$TMP/concrete.wyn" >/dev/null 2>&1 && "$TMP/concrete" 2>&1; rm -f "$TMP/concrete" "$TMP/concrete.wyn.c")
[ "$out" = "5" ] && ok "concrete enum still runs" || bad "concrete enum: [$out]"

# a generic STRUCT (the supported analogue) still compiles + runs
printf 'struct Box<T> { val: T }\nfn wrap<T>(x: T) -> Box<T> { return Box { val: x } }\nfn main() {\n  var b = wrap(42)\n  println("${b.val}")\n}\n' > "$TMP/gs.wyn"
out=$("$WYN" build "$TMP/gs.wyn" >/dev/null 2>&1 && "$TMP/gs" 2>&1; rm -f "$TMP/gs" "$TMP/gs.wyn.c")
[ "$out" = "42" ] && ok "generic struct returning its own generic type still runs" || bad "generic struct: [$out]"

echo ""; echo "generic-enum: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
