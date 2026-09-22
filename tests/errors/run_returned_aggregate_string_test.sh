#!/bin/bash
# A COMPUTED STRING STORED INTO A RETURNED AGGREGATE MUST SURVIVE THE RETURN.
#
# `b = "n=${n}"; return R { body: b }` printed the EMPTY STRING at length 0 and
# exit 0 - "build a value, return it", the first thing anyone writes, silently
# lost its strings. Same for `Ok(b)` / `Err(b)` / `Some(b)` / `Ok(R{body:b})` /
# `[b]` / `cond ? b : "x"`. Only COMPUTED strings died (interpolation, concat,
# `.upper()`); literals and parameters survived, which is what made it so hard
# to see.
#
# Root cause: the pre-return release authority (emit_string_releases_for_return,
# src/codegen.c) skips any local the return expression still reads, and asks
# expr_references_var() which locals those are. That liveness walk handled
# BINARY/CALL/INTERP and fell through `default: return 0` for every AGGREGATE
# kind - struct init, Ok/Err/Some, array, tuple, ternary, match, block, map. A
# "no" from the walk means "release it", so the local was freed one statement
# BEFORE the constructor read it: a use-after-free that reads as empty output.
#
# So EVERY arm below asserts OUTPUT, never just exit status: this whole defect
# class exits 0, and an exit-code-only test would pass against the bug.
#
# The three commands are all exercised because they are three different paths:
# `wyn run`, `wyn build`, and `wyn run --release` (the only command that
# compiles against src/wyn_runtime_slim.h - note flags go BEFORE the path).
set -uo pipefail
WYN="${WYN:-./wyn}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# $1=name $2=expected-output (newlines as '|') ; wyn source on stdin.
# Asserts the SAME output from all three compile/run paths.
runok3(){
  local name="$1" want="$2" f="$TMP/$1.wyn" got
  cat > "$f"
  # 1. wyn run (interpreted-compile path, cached .out next to source)
  got=$("$WYN" run "$f" 2>/dev/null | tr '\n' '|')
  [ "$got" = "$want" ] && ok "$name/run → $want" || bad "$name/run: want [$want] got [$got]"
  # 2. wyn build -> native binary
  got=$("$WYN" build "$f" -o "$TMP/$name.bin" >/dev/null 2>&1 && "$TMP/$name.bin" 2>/dev/null | tr '\n' '|')
  [ "$got" = "$want" ] && ok "$name/build → $want" || bad "$name/build: want [$want] got [$got]"
  # 3. wyn run --release (slim runtime header). Flags BEFORE the path.
  got=$("$WYN" run --release "$f" 2>/dev/null | tr '\n' '|')
  [ "$got" = "$want" ] && ok "$name/release → $want" || bad "$name/release: want [$want] got [$got]"
  rm -f "$TMP/$name.bin" "$f.c" "$f.out"
}

echo "--- returned aggregate keeps its computed string ---"

# ============================================================
# BROKEN BEFORE THE FIX: computed string into a returned aggregate
# ============================================================

# A1. struct literal + interpolation - the headline repro.
runok3 struct_interp 'n=7|3|' <<'WYN'
struct R { body: string }
fn f(n: int) -> R {
    b = "n=${n}"
    return R { body: b }
}
fn main() { r = f(7) print(r.body) print(r.body.len()) }
WYN

# A2. struct literal + CONCAT (a different string-producing expression:
# string_concat, not snprintf-interp - they are separate codegen paths).
runok3 struct_concat 'n=7|3|' <<'WYN'
struct R { body: string }
fn f(s: string) -> R {
    b = "n=" + s
    return R { body: b }
}
fn main() { r = f("7") print(r.body) print(r.body.len()) }
WYN

# A3. struct literal + METHOD RESULT (`.upper()` -> string_upper).
runok3 struct_method 'ABC|3|' <<'WYN'
struct R { body: string }
fn f() -> R {
    b = "abc".upper()
    return R { body: b }
}
fn main() { r = f() print(r.body) print(r.body.len()) }
WYN

# A4. Ok(b) - EXPR_OK. The payload stores the pointer RAW, so a release before
# the return left the Result pointing at freed memory.
runok3 ok_interp 'n=7|3|' <<'WYN'
fn f(n: int) -> Result<string, string> {
    b = "n=${n}"
    return Ok(b)
}
fn main() {
    match f(7) { Ok(v) => { print(v) print(v.len()) } Err(e) => print("ERR") }
}
WYN

# A5. Err(b) - EXPR_ERR, the error arm of the same family.
runok3 err_interp 'e=7|3|' <<'WYN'
fn f(n: int) -> Result<int, string> {
    b = "e=${n}"
    return Err(b)
}
fn main() {
    match f(7) { Ok(v) => print("OK") Err(e) => { print(e) print(e.len()) } }
}
WYN

# A6. Some(b) - EXPR_SOME.
runok3 some_interp 'n=7|3|' <<'WYN'
fn f(n: int) -> Option<string> {
    b = "n=${n}"
    return Some(b)
}
fn main() {
    match f(7) { Some(v) => { print(v) print(v.len()) } None => print("NONE") }
}
WYN

# A7. NESTED: Ok(R{body:b}) - an aggregate inside an aggregate. The walk has to
# recurse through BOTH kinds, so this reddens if either case is missing.
runok3 ok_struct 'n=7|3|' <<'WYN'
struct R { body: string }
fn f(n: int) -> Result<R, string> {
    b = "n=${n}"
    return Ok(R { body: b })
}
fn main() {
    match f(7) { Ok(v) => { print(v.body) print(v.body.len()) } Err(e) => print("ERR") }
}
WYN

# A8. local-then-INLINE: `s = b; return Ok(s)`. Binding to a second local was
# NOT a workaround for the Ok shape - the alias was released too.
runok3 ok_alias 'n=7|3|' <<'WYN'
fn f(n: int) -> Result<string, string> {
    b = "n=${n}"
    s = b
    return Ok(s)
}
fn main() {
    match f(7) { Ok(v) => { print(v) print(v.len()) } Err(e) => print("ERR") }
}
WYN

# A9. ARRAY literal - EXPR_ARRAY, another aggregate that fell through default.
runok3 array_elem 'n=7|3|' <<'WYN'
fn f(n: int) -> [string] {
    b = "n=${n}"
    return [b]
}
fn main() { a = f(7) print(a[0]) print(a[0].len()) }
WYN

# A10. TERNARY - EXPR_TERNARY. Not an aggregate at all: proof the defect was in
# the generic liveness walk and not in any one constructor.
runok3 ternary 'n=7|3|' <<'WYN'
fn f(n: int) -> string {
    b = "n=${n}"
    return n > 0 ? b : "neg"
}
fn main() { v = f(7) print(v) print(v.len()) }
WYN

# ============================================================
# WORKED BEFORE THE FIX - must still work (no regression)
# ============================================================

# B1. string LITERAL into a returned struct (never went through a local).
runok3 keep_literal 'lit|3|' <<'WYN'
struct R { body: string }
fn f() -> R { return R { body: "lit" } }
fn main() { r = f() print(r.body) print(r.body.len()) }
WYN

# B2. PARAMETER into a returned struct (a borrow the callee does not own).
runok3 keep_param 'par|3|' <<'WYN'
struct R { body: string }
fn f(p: string) -> R { return R { body: p } }
fn main() { r = f("par") print(r.body) print(r.body.len()) }
WYN

# B3. local-then-return (`r = R{body:b}; return r`) - the documented workaround.
runok3 keep_local_struct 'n=7|3|' <<'WYN'
struct R { body: string }
fn f(n: int) -> R {
    b = "n=${n}"
    r = R { body: b }
    return r
}
fn main() { r = f(7) print(r.body) print(r.body.len()) }
WYN

# B4. A computed string RETURNED DIRECTLY, and one CONSUMED (not returned) - the
# release must still happen for locals the return does NOT read, or this arm
# would pass even with the releases deleted wholesale.
runok3 keep_direct 'dead|n=7|' <<'WYN'
fn f(n: int) -> string {
    dead = "dead=${n}"
    print(dead.substring(0, 4))
    b = "n=${n}"
    return b
}
fn main() { print(f(7)) }
WYN

# B5. A returned string local used in a LOOP - many allocations, one survivor.
# Catches a fix that suppresses releases so broadly that a loop body stops
# freeing (which would show up here as a leak, and under ASan as a report).
runok3 keep_loop 'v=3|3|' <<'WYN'
fn f() -> string {
    out = ""
    for i in 0..4 {
        out = "v=${i}"
    }
    return out
}
fn main() { v = f() print(v) print(v.len()) }
WYN

echo ""; echo "returned-aggregate-string: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
