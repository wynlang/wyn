#!/bin/bash
# #427: printing a set renders the SET, not its pointer.
#
# WHAT THIS REPLACES. `print(s)` printed the set pointer as a decimal (4383976288) -
# check-clean, exit 0, silently meaningless. `hashmap_format` had been written for the map
# half and its own comment says "Same for HashMap and HashSet"; the set half was never
# done, so a set had no renderer anywhere and the print path fell through to the integer
# one. Inspecting a collection is one of the most common things anyone does while
# debugging, and there was no workaround, because interpolation was broken the same way.
#
# THE SPELLING IS THE LITERAL SYNTAX, and that is asserted rather than incidental:
# `{:1, 2}` with the leading colon, `{:"a"}` for a string element, and `{:}` for an empty
# set. `{}` is the empty MAP literal, so rendering an empty set as `{}` would print two
# different values identically - the empty-set arm below is what pins that apart.
#
# TWO SPELLINGS, TWO CODE PATHS, and the second one is why this gate has both.
# `print(s)` is fixed by a WynHashSet* arm in wyn_out_append's _Generic. Interpolation
# does NOT go through that macro - it needs a string - so `"${s}"` still rendered the
# pointer after print(s) was already correct. Any future change that fixes one path and
# not the other is caught here.
#
# BOTH MODES throughout. The set formatter is declared in wyn_runtime_slim.h from the
# start, which is the lesson #453 paid for: twelve functions reached only the debug header
# and `print(map)` printed a POINTER under --release while debug rendered it correctly.
#
# ORDER IS NOT ASSERTED for multi-element sets. Set iteration is bucket order, not
# insertion order - the same caveat hashmap_keys() and hashset_elements() carry - so the
# multi-element arms check membership of the rendered text, never a fixed sequence.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# expect <label> <source> <expected-first-line>  -- both modes, own temp dir each
# (`wyn run` caches a binary as <file>.out, so one path shared between modes can silently
#  re-run the other mode's build)
expect(){
  for mode in debug release; do
    d=$(mktemp -d); printf '%b\n' "$2" > "$d/p.wyn"
    if [ "$mode" = release ]; then
      got=$(TMPDIR="$d" perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run --release "$d/p.wyn" 2>&1)
    else
      got=$(TMPDIR="$d" perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d/p.wyn" 2>&1)
    fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable' | head -1)
    rm -rf "$d"
    if [ "$got" = "$3" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 - got [$got] want [$3]"; fi
  done
}

# contains <label> <source> <substr>...  -- order-independent membership, both modes
contains(){
  label="$1"; src="$2"; shift 2
  for mode in debug release; do
    d=$(mktemp -d); printf '%b\n' "$src" > "$d/p.wyn"
    if [ "$mode" = release ]; then
      got=$(TMPDIR="$d" perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run --release "$d/p.wyn" 2>&1)
    else
      got=$(TMPDIR="$d" perl -e 'alarm(90); exec @ARGV' -- "$WYNABS" run "$d/p.wyn" 2>&1)
    fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable' | head -1)
    rm -rf "$d"
    miss=""
    for want in "$@"; do
      case "$got" in *"$want"*) ;; *) miss="$miss $want";; esac
    done
    if [ -z "$miss" ]; then ok "[$mode] $label"
    else bad "[$mode] $label - [$got] is missing:$miss"; fi
  done
}

echo "=== #427: print(set) renders the set, not its pointer ==="

# ------------------------------------------------------- print(s), every element kind
expect "print: string element" 'fn main() {\n    s = {:"x"}\n    print(s)\n}' '{:"x"}'
expect "print: int element"    'fn main() {\n    s = {:7}\n    print(s)\n}'   '{:7}'
expect "print: float element"  'fn main() {\n    s = {:1.5}\n    print(s)\n}' '{:1.5}'
expect "print: bool element"   'fn main() {\n    s = {:true}\n    print(s)\n}' '{:true}'

# An empty set must NOT render as `{}` - that is the empty MAP literal.
expect "print: empty set is {:} and not {}" \
  'fn main() {\n    s = HashSet.new()\n    print(s)\n}' '{:}'

# ------------------------------- the SECOND path: interpolation does not use the _Generic
expect "interp: string element" 'fn main() {\n    s = {:"x"}\n    print("${s}")\n}' '{:"x"}'
expect "interp: int element"    'fn main() {\n    s = {:7}\n    print("${s}")\n}'   '{:7}'
expect "interp: empty set"      'fn main() {\n    s = HashSet.new()\n    print("${s}")\n}' '{:}'

expect "interp: a set inside a longer string" \
  'fn main() {\n    s = {:9}\n    print("set=${s} done")\n}' \
  'set={:9} done'

# ------------------------------------------- multi-element: membership, never an ordering
contains "print: two int elements (order not asserted)" \
  'fn main() {\n    s = {:1}\n    s.add(2)\n    print(s)\n}' \
  '{:' '1' '2' '}' ', '

contains "interp: two string elements (order not asserted)" \
  'fn main() {\n    s = {:"a"}\n    s.add("b")\n    print("${s}")\n}' \
  '{:' '"a"' '"b"' '}'

# ------------------------------------------------------ the map control, still unchanged
expect "control: print(map) still renders the map" \
  'fn main() {\n    m = {"a": 1}\n    print(m)\n}' '{"a": 1}'

expect "control: a set and a map in one program stay distinct" \
  'fn main() {\n    m = {"a": 1}\n    s = {:"a"}\n    print("${m} ${s}")\n}' \
  '{"a": 1} {:"a"}'

echo "--- $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
