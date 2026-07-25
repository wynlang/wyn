#!/bin/bash
# Nested-aggregate feature suite: verifies the FULLY-IMPLEMENTED cases run with
# correct output, and the two DEFERRED cases (function-typed struct field,
# array of closures) produce a CLEAN CHECK-TIME error instead of leaking raw C.
# Guards the "check passes then codegen leaks C / long-long-casts the aggregate"
# cliff the second HN review found.
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

runok(){ # $1=name $2=expected-output ; source on stdin
  local f="$TMP/$1.wyn"; cat > "$f"
  local out
  out=$("$WYN" build "$f" -o "$TMP/$1" >/dev/null 2>&1 && "$TMP/$1" 2>&1; rm -f "$TMP/$1" "$f.c")
  if [ "$out" = "$2" ]; then ok "$1 → $2"; else bad "$1: expected [$2] got [$out]"; fi
}
gate(){ # $1=name $2=needle ; source on stdin — must FAIL check with needle
  local f="$TMP/$1.wyn"; cat > "$f"
  local out code
  out=$("$WYN" check "$f" 2>&1); code=$?
  if [ $code -ne 0 ] && echo "$out" | grep -q "$2"; then ok "$1: clean check error"
  else bad "$1: code=$code needle='$2' [$out]"; fi
}

# --- FULLY IMPLEMENTED (run with correct output) ---

# 1. recursive enum (expression tree): Mul(Add(2,3),4) = 20
runok rec_enum 20 <<'WYN'
enum Expr { Num(int), Add(Expr, Expr), Mul(Expr, Expr) }
fn eval(e: Expr) -> int {
    match e {
        Expr.Num(n) => { return n }
        Expr.Add(a, b) => { return eval(a) + eval(b) }
        Expr.Mul(a, b) => { return eval(a) * eval(b) }
    }
    return 0
}
fn main() -> int { println("${eval(Expr.Mul(Expr.Add(Expr.Num(2), Expr.Num(3)), Expr.Num(4)))}") return 0 }
WYN

# 2. map with struct values
runok map_struct 100 <<'WYN'
struct Account { balance: int, owner: string }
fn main() -> int {
    accts = {}
    accts["a1"] = Account{ balance: 100, owner: "alice" }
    println("${accts["a1"].balance}")
    return 0
}
WYN

# 3. array-of-arrays via push
runok arr_of_arr 6 <<'WYN'
fn main() -> int {
    grid = []
    grid.push([1, 2, 3])
    grid.push([4, 5, 6])
    println("${grid[1][2]}")
    return 0
}
WYN

# 5. generics over non-scalar T (array) + two-instantiation Box
runok generic_arr 1 <<'WYN'
fn id<T>(v: T) -> T { return v }
fn main() -> int { a = id([1, 2, 3]) println("${a[0]}") return 0 }
WYN
runok box_two 42hi <<'WYN'
struct Box<T> { val: T }
fn main() -> int {
    b1 = Box{ val: 42 }
    b2 = Box{ val: "hi" }
    println("${b1.val}${b2.val}")
    return 0
}
WYN

# --- DEFERRED (must be a clean check-time error, never raw C) ---

# 4a. function-typed struct field
gate fn_field "not yet supported as a struct field" <<'WYN'
struct Handler { name: string, fn_ptr: fn(int) -> int }
fn main() -> int {
    h = Handler{ name: "double", fn_ptr: (x) => x * 2 }
    println("${h.fn_ptr(21)}")
    return 0
}
WYN

# 4b. array of closures
gate fn_array "arrays of functions/closures are not yet supported" <<'WYN'
fn main() -> int {
    fns = []
    fns.push((x) => x * 2)
    f = fns[0]
    println("${f(21)}")
    return 0
}
WYN

echo ""; echo "nested-aggregate: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
