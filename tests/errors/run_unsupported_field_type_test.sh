#!/bin/bash
# Honesty gates: features that used to pass check/parse and then either silently
# mistype the field (HashMap/HashSet struct fields lowered to `long long`, so
# `s.m["k"]` later reported "struct has no field 'm'") or leak a cryptic parser
# message (array-typed enum payload -> "Expected type name in variant"). These
# must now yield a clean "not yet supported" diagnostic. Supported cases
# (array-typed struct field, Option field) must still compile.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

expect_error() {
    local name="$1"; local file="$2"; local pat="$3"
    local out rc
    out=$(perl -e 'alarm(10); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -ge 1 ] && [ $rc -le 127 ] && echo "$out" | grep -qi "$pat"; then ok "$name"
    else bad "$name (rc=$rc) [$(echo "$out" | head -2)]"; fi
}
expect_ok() {
    local name="$1"; local file="$2"
    local out rc
    out=$(perl -e 'alarm(20); exec @ARGV' -- "$WYN" check "$file" 2>&1); rc=$?
    if [ $rc -eq 0 ]; then ok "$name"; else bad "$name (rc=$rc) [$(echo "$out" | head -2)]"; fi
}

printf 'struct S { name: string, m: HashMap<string,int> }\nfn main() { println("x") }\n' > "$TMP/map_field.wyn"
expect_error "HashMap struct field -> not-yet-supported error" "$TMP/map_field.wyn" "not yet supported"

printf 'struct S { name: string, m: HashSet<int> }\nfn main() { println("x") }\n' > "$TMP/set_field.wyn"
expect_error "HashSet struct field -> not-yet-supported error" "$TMP/set_field.wyn" "not yet supported"

printf 'enum Json { Arr([Json]), Num(int) }\nfn main() { println("x") }\n' > "$TMP/arr_payload.wyn"
expect_error "array-typed enum payload -> not-yet-supported error" "$TMP/arr_payload.wyn" "not yet supported"

# Mutually-recursive enum cycle -> clean check error (DEFERRED feature). A
# DIRECT self-reference (Tree(Tree, Tree)) IS supported and must still compile.
printf 'enum A { AtoB(B), ANil }\nenum B { BtoA(A), BNil }\nfn main() { println("x") }\n' > "$TMP/mutual_enum.wyn"
expect_error "mutually-recursive enum cycle -> clean error" "$TMP/mutual_enum.wyn" "mutually-recursive enum cycle"

printf 'enum Tree { Leaf(int), Node(Tree, Tree), Empty }\nfn main() {\n var t = Tree::Node(Tree::Leaf(1), Tree::Leaf(2))\n match t { Tree::Node(a, b) => println("node"), Tree::Leaf(n) => println("leaf"), Tree::Empty => println("empty") }\n}\n' > "$TMP/direct_rec.wyn"
expect_ok "direct self-recursive enum still compiles" "$TMP/direct_rec.wyn"

# Supported: an array-typed struct field must still compile cleanly.
printf 'struct S { name: string, vals: [int] }\nfn main() {\n var s = S{name:"a", vals:[1,2]}\n println(s.vals[0])\n}\n' > "$TMP/arr_field_ok.wyn"
expect_ok "array-typed struct field still checks clean" "$TMP/arr_field_ok.wyn"

# Supported: an optional-typed struct field must still compile cleanly.
printf 'struct S { name: string, v: int? }\nfn main() {\n var s = S{name:"a", v:5}\n println(s.name)\n}\n' > "$TMP/opt_field_ok.wyn"
expect_ok "optional-typed struct field still checks clean" "$TMP/opt_field_ok.wyn"

echo ""; echo "unsupported-field-type: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
