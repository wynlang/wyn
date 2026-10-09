#!/bin/bash
# A PAYLOAD-CARRYING ENUM VARIANT NAMED WITHOUT ARGUMENTS IS NOT A VALUE.
#
# THE BUG (issue #489).
#
#     enum Shape { Circle(float), Point }
#     print(Shape.Circle.to_string())
#
# `Circle` carries a payload, so `Shape.Circle` is that variant's CONSTRUCTOR, not a
# `Shape`. The checker typed it as the enum anyway - the branch's own comment said "it's a
# constructor" while returning the enum type - and codegen then emitted the function
# designator `Shape_Circle` wherever a value was wanted. `wyn check` reported no errors on
# a program that cannot be built, which is the one contract `check` exists to provide, and
# README.md:103 PUBLISHED that exact line as a feature.
#
# THREE SPELLINGS, AND THE THIRD IS THE WORST. The issue reports the dot form; the other
# two were found while fixing it:
#
#   Shape.Circle    check OK -> build error: passing 'Shape (double)' to parameter of
#                               incompatible type 'Shape'
#   Shape::Circle   check OK -> BUILDS AND RUNS. `Shape::Circle.to_string()` printed
#                               4346888 - the constructor's ADDRESS as a decimal - and
#                               exited 0. A silently wrong answer, which this project
#                               treats as worse than a crash.
#   Circle (bare)   check OK -> build error: 'Circle' undeclared
#
# The `::` arm is the reason this gate asserts REJECTION rather than "the build fails":
# a gate written as "it does not build" would have passed on the dot and bare spellings
# while the `::` spelling happily printed a pointer.
#
# ONE RULE, NOT ONE PER POSITION. The issue warned that a fix covering only the method
# receiver would be "one more rule with one copy per site". Every position below - method
# receiver, call argument, comparison, variable initialiser, return, interpolation -
# evaluates the variant as an expression, so all of them reach the same two checker sites
# (EXPR_FIELD_ACCESS for the dot form, EXPR_IDENT for the other two) and are rejected by
# one shared function. The positions are still asserted individually, because "they all go
# through one place" is a claim about the code, not evidence about the behaviour.
#
# WHAT MUST STAY LEGAL, and these controls are load-bearing rather than courtesy - an
# over-broad rejection rule is how nine correct corpus files were once rejected:
#   - a payload-FREE variant as a value, in all three spellings
#   - all three CONSTRUCTOR CALL spellings
#   - match arms naming variants, qualified and bare
#   - the enum's own NAME as a type annotation (it resolves to the same TYPE_ENUM symbol
#     a variant does, so a rule keyed on "the symbol is an enum" would reject every
#     annotation in the program)
#
# ONE DIAGNOSTIC PER OCCURRENCE. Some positions are check_expr'd twice and nothing in this
# checker dedupes, so the first version of the rule printed the same error twice for one
# occurrence. Asserted by COUNT, not just presence.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

ENUM='enum Shape { Circle(float), Point }'

# reject <label> <body> [expected-diagnostic-count]
# The program must FAIL `wyn check`, and the message must be the payload one - not some
# other error that happens to make check fail, which is how a rejection arm goes vacuous.
reject(){
  d="$TMP/r$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$ENUM\n$2" > "$d/a.wyn"
  out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" check "$d/a.wyn" 2>&1)
  rc=$?
  n=$(printf '%s\n' "$out" | grep -c 'carries a payload')
  want=${3:-1}
  if [ "$rc" -eq 0 ]; then
    bad "$1 - wyn check PASSED"
  elif [ "$n" -ne "$want" ]; then
    bad "$1 - wanted $want payload diagnostic(s), got $n"
  else
    ok "$1"
  fi
}

# accept <label> <body> <expected-stdout>
# Runs it: a control that only type-checks would not catch a fix that broke codegen.
accept(){
  d="$TMP/a$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$ENUM\n$2" > "$d/a.wyn"
  got=$(perl -e 'alarm(90); exec @ARGV' -- "$WYN" run "$d/a.wyn" 2>&1)
  got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
  if [ "$got" = "$3" ]; then ok "$1"
  else bad "$1 - got [$(printf '%s' "$got" | tr '\n' '|' | cut -c1-110)] want [$(printf '%s' "$3" | tr '\n' '|')]"; fi
}

echo "=== #489: a payload-carrying variant is a constructor, not a value ==="

# ----------------------------------------- REJECTED: every position, the dot spelling
reject "the reported repro: method receiver" \
  'fn main() { print(Shape.Circle.to_string()) }'
reject "call argument" \
  'fn takes(s: Shape) -> int { return 1 }\nfn main() { print("${takes(Shape.Circle)}") }'
reject "variable initialiser" \
  'fn main() {\n    x = Shape.Circle\n    print("${x}")\n}'
reject "comparison operand" \
  'fn main() { if Shape.Circle == Shape.Point { print("eq") } }'
reject "return value" \
  'fn mk() -> Shape { return Shape.Circle }\nfn main() { print(mk().to_string()) }'
reject "string interpolation" \
  'fn main() { print("${Shape.Circle}") }'

# ----------------------------------------- REJECTED: the other two spellings
# This one BUILT and printed the constructor's address before the fix.
reject "the :: spelling, which used to build and print a pointer" \
  'fn main() { print(Shape::Circle.to_string()) }'
reject "a bare variant name as a value" \
  'fn main() {\n    x = Circle\n    print("${x}")\n}'

# ----------------------------------------- ACCEPTED: payload-free variants are values
accept "control: payload-free variant, dot spelling" \
  'fn main() { print(Shape.Point.to_string()) }' \
  'Point'
accept "control: payload-free variant, :: spelling" \
  'fn main() {\n    p = Shape::Point\n    print(p.to_string())\n}' \
  'Point'
accept "control: payload-free variant, bare" \
  'fn main() {\n    p = Point\n    print(p.to_string())\n}' \
  'Point'

# ----------------------------------------- ACCEPTED: the constructor CALLS
# These are resolved inside case EXPR_CALL, which never check_expr-s its callee - the
# property the rejection rule depends on. If that changed, these go red rather than
# the rule silently covering legal code.
#
# EACH BINDS THE RESULT FIRST, and that is not stylistic. A constructor call used
# DIRECTLY as a method receiver (`Shape.Circle(1.5).to_string()`) or as a match scrutinee
# (`match Shape.Circle(1.5) { }`) is an `internal codegen error` - on the branch point as
# well as here, so it is a PRE-EXISTING defect and not this rule's doing, and it is filed
# separately. It is pinned at the bottom of this gate so these controls cannot be read as
# covering it. Writing them the natural way is how it was found.
accept "control: constructor call, dot spelling" \
  'fn main() {\n    s = Shape.Circle(1.5)\n    print(s.to_string())\n}' \
  'Circle'
accept "control: constructor call, :: spelling" \
  'fn main() {\n    s = Shape::Circle(2.5)\n    print(s.to_string())\n}' \
  'Circle'
accept "control: constructor call, bare" \
  'fn main() {\n    s = Circle(3.5)\n    print(s.to_string())\n}' \
  'Circle'
accept "control: a constructor call as a function ARGUMENT" \
  'fn takes(s: Shape) -> int { return 7 }\nfn main() { print("${takes(Shape.Circle(1.5))}") }' \
  '7'

# ----------------------------------------- ACCEPTED: patterns and annotations
accept "control: match arms, qualified patterns" \
  'fn main() {\n    s = Shape.Circle(1.5)\n    match s {\n        Shape.Circle(r) => print("circle ${r}"),\n        Shape.Point => print("point"),\n    }\n}' \
  'circle 1.5'
accept "control: match arms, bare patterns" \
  'fn main() {\n    t = Shape.Point\n    match t {\n        Circle(r) => print("c ${r}"),\n        Point => print("p"),\n    }\n}' \
  'p'
accept "control: the enum NAME as a parameter annotation and a return type" \
  'fn id(s: Shape) -> Shape { return s }\nfn main() {\n    s = id(Shape.Circle(4.5))\n    print(s.to_string())\n}' \
  'Circle'

# ----------------------------------------- A PRE-EXISTING LIMITATION, PINNED
# So this gate cannot be read as proving more than it does. An enum constructor CALL in a
# RECEIVER or SCRUTINEE position is an `internal codegen error`, while the same call bound
# to a variable first is fine:
#
#   Shape.Circle(1.5).to_string()      internal codegen error
#   match Shape.Circle(1.5) { ... }    internal codegen error
#   s = Shape.Circle(1.5); s.to_...    works   (asserted above)
#   takes(Shape.Circle(1.5))           works   (asserted above)
#
# Reproduced on the branch point, so it is not caused by the #489 rule. Asserted here as
# the CURRENT behaviour - if someone fixes it, this arm goes red and should be promoted to
# an `accept`, which is the point of pinning rather than omitting.
pin(){
  d="$TMP/p$PASS$FAIL"; mkdir -p "$d"
  printf '%b\n' "$ENUM\n$2" > "$d/a.wyn"
  out=$(perl -e 'alarm(90); exec @ARGV' -- "$WYN" run "$d/a.wyn" 2>&1)
  if printf '%s\n' "$out" | grep -q 'internal codegen error'; then
    ok "pinned (still broken, filed): $1"
  else
    bad "pinned limitation CHANGED - $1 no longer reports internal codegen error; promote this arm"
  fi
}
pin "a constructor call directly as a method receiver" \
  'fn main() { print(Shape.Circle(1.5).to_string()) }'
pin "a constructor call directly as a match scrutinee" \
  'fn main() {\n    match Shape.Circle(1.5) {\n        Shape.Circle(r) => print("c ${r}"),\n        Shape.Point => print("p"),\n    }\n}'
pin "the :: spelling directly as a method receiver, even payload-free" \
  'fn main() { print(Shape::Point.to_string()) }'

echo ""
echo "enum-variant-value: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
