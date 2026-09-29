#!/bin/bash
# Every method the registry ADVERTISES must be callable from Wyn.
#
# `src/types.c`'s `method_signatures[]` is what tells the checker "a `map` receiver has
# an `is_empty` method returning bool". Nothing verified that the advertised method
# could actually be CALLED, and three separate things went wrong behind that gap:
#
#   m = {"a": 1}   print(m.is_empty())    # `wyn check` PASSED, build died: the
#                                         # lowering names wyn_hashmap_is_empty, a
#                                         # symbol NO runtime source defines
#   r.to_bytes() / m.entries() / s.to_array()   # advertised, then refused by the
#                                               # checker itself - dead table rows
#   var c: char = 97   c.is_uppercase()   # `char` IS `int` in Wyn (checker.c), so the
#                                         # whole `char` receiver family is
#                                         # unreachable by construction
#
# WHY THIS COMPILES A CALL RATHER THAN CHECKING THE SYMBOL TABLE. The obvious cheap
# gate - cross-check each `out->c_function` against `nm runtime/libwyn_rt.a` - gives
# WRONG ANSWERS in both directions. 19 registry names are absent from the archive, yet
# `map.clear()` is one of them and works fine, because codegen has its own lowering that
# takes precedence over the registry; while `int_to_int` resolves as a `static inline` in
# a header. Only building and running a real call answers the question the user actually
# has, so that is what this does.
#
# HOW IT STAYS HONEST. The pairs are read out of `types.c` AT RUN TIME, so a new registry
# entry enrols itself in this gate automatically and cannot be added unnoticed. The
# KNOWN_BROKEN list is checked for EXACTNESS: an entry that starts working FAILS the gate,
# so the list cannot rot into a list of things nobody has looked at since.
#
# Scope: EVERY row. It used to be the arity-0 half only, because `method_signatures`
# recorded an argument COUNT and a count cannot be turned into a call. That blind spot is
# how `.any`/`.all` and `every`/`times` shipped uncallable - all four take a function, so
# all four sat in the unreachable half. The table now carries the argument TYPES
# (`{"string", "pad_left", "string", "int, string"}`), so a real call is synthesised for
# the arity 1-2 rows too, and the arity is just the number of entries in that column.
#
# ALSO: a passing run now requires a clean compile, not merely that the program reached
# the end. A method the checker does not know about can print `Unknown method 'for_each'
# for type 'map'` and STILL produce a binary that runs (the call lowers to nothing), so
# "it printed REACHED" was not evidence the row works. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
SRC_TYPES="$(dirname "$WYNABS")/src/types.c"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# receiver.method -> why it cannot be called today. Every entry here is a REGISTRY ROW
# THAT LIES; the fix is either to implement the method or to delete the row.
known_broken(){
  case "$1" in
    # EMPTY, and that is the point: every entry this list ever held has been resolved
    # rather than tolerated.
    #   map.is_empty      the function was written (wyn_hashmap_is_empty)
    #   string.to_bytes   its dispatch had an EMPTY body and `bytes` carried the
    #                     assignment twice - a botched edit; both spellings now lower
    #   map.entries       no runtime function existed; the advertising row was removed
    #   set.to_array      ditto
    #   char.*  (8 rows)  unreachable by construction - `char` IS `int` in Wyn, so a
    #                     char-typed value resolves against the int table and never
    #                     reached a "char" row. The ten rows were removed; a single
    #                     character is a 1-length string, which the string receiver
    #                     already serves.
    # Each removal was forced by the exactness half below, which fails on an entry that
    # has started working - so this list cannot quietly become a place defects go to die.
    *) return 1;;
  esac
}

# A value of each receiver type. The string fixture is "42" on purpose: `.to_int()` and
# friends are real, working methods that PANIC on a non-numeric string, and a fixture
# that panics would be reported as a broken method.
fixture(){
  case "$1" in
    string) echo 'r = "42"';;
    int)    echo 'r = 5';;
    float)  echo 'r = 1.5';;
    bool)   echo 'r = true';;
    char)   echo 'var r: char = 97';;
    array)  echo 'r = [1, 2, 3]';;
    map)    echo 'r = {"a": 1}';;
    set)    echo 'r = {:"a"}';;
    json)   echo 'r = Json.parse("{\"a\": 1}")';;
    option) echo 'r = reg_opt()';;
    result) echo 'r = reg_res()';;
    *)      echo "";;
  esac
}

# Read the authority. Emits TAB-separated "receiver method returntype call" per row,
# de-duplicated (the table registers some rows twice - `map.contains` and `map.len` among
# them). `call` is the whole call text, arguments included, so the argument-type column is
# turned into Wyn source in exactly ONE place.
#
# The literals are chosen so a WORKING method cannot be mistaken for a broken one:
# every int is 1 and every string is "a", which is in range for the "42"/[1,2,3]/{"a": 1}
# fixtures and is a key those fixtures HAVE - a missing map key aborts, and an abort would
# be reported as an unreachable method.
python3 - "$SRC_TYPES" > "$TMP/pairs.txt" <<'PY'
import re, sys
src = open(sys.argv[1]).read()
m = re.search(r'method_signatures\[\]\s*=\s*\{(.*?)\n\};', src, re.S)
if not m:
    sys.stderr.write("could not find method_signatures[] in types.c\n"); sys.exit(2)
rows = re.findall(r'^\s*\{"([a-z_]+)",\s*"([a-zA-Z_0-9]+)",\s*"([^"]*)",\s*"([^"]*)"\}',
                  m.group(1), re.M)

LIT = {'int': '1', 'float': '1.5', 'string': '"a"', 'bool': 'true',
       'array': '[4, 5]', 'map': '{"b": 2}', 'set': '{:"b"}'}

def split_args(spec):
    """Comma-separated at depth 0, so fn(int,int)->int stays one argument."""
    out, buf, d = [], '', 0
    for c in spec:
        if c in '([': d += 1
        elif c in ')]': d -= 1
        if c == ',' and d == 0:
            out.append(buf.strip()); buf = ''
        else:
            buf += c
    if buf.strip(): out.append(buf.strip())
    return out

def literal(t):
    if t in LIT: return LIT[t]
    fn = re.match(r'^fn\(([^)]*)\)(?:->(.+))?$', t.replace(' ', ''))
    if not fn: return None
    ps = [p for p in fn.group(1).split(',') if p]
    params = ', '.join('_a%d: %s' % (i, p) for i, p in enumerate(ps))
    ret = fn.group(2)
    if not ret: return 'fn(%s) { }' % params
    body = {'int': '1', 'bool': 'true', 'float': '1.5', 'string': '"a"',
            '[int]': '[_a0]'}.get(ret)
    if body is None: return None
    return 'fn(%s) -> %s { return %s }' % (params, ret, body)

seen = set()
for recv, meth, ret, spec in rows:
    if (recv, meth) in seen: continue
    seen.add((recv, meth))
    if spec == '...':
        # Variadic: there is no fixed argument list to render. Reported as an accounted-for
        # gap rather than dropped, so the gate's own coverage is visible in its output -
        # a quiet `continue` here is the shape of the hole that left arity 1-2 unchecked.
        print('%s\t%s\t%s\tVARIADIC:%s' % (recv, meth, ret, spec))
        continue
    args = split_args(spec)
    lits = [literal(a) for a in args]
    if any(l is None for l in lits):
        # A spec this generator cannot render is a GAP IN THE GATE, not a passing row.
        print('%s\t%s\t%s\tUNRENDERABLE:%s' % (recv, meth, ret, spec))
        continue
    print('%s\t%s\t%s\t%s(%s)' % (recv, meth, ret, meth, ', '.join(lits)))
PY
if [ ! -s "$TMP/pairs.txt" ]; then
  echo "  FAIL  could not read method_signatures[] from $SRC_TYPES"
  echo ""; echo "registry-reachable: 0 pass, 1 fail"; exit 1
fi

ROWS=$(wc -l < "$TMP/pairs.txt" | tr -d ' ')
TOTAL=$(grep -cv $'\t''\(UNRENDERABLE\|VARIADIC\):' "$TMP/pairs.txt" || true)
RECEIVERS=$(cut -f1 "$TMP/pairs.txt" | sort -u)

# emit_one <file> <receiver> <call> <returntype>
# A void method must not have its result bound, or the gate reports a fixture error as a
# broken method.
emit_one(){
  { echo 'fn reg_opt() -> int? { return Some(1) }'
    echo 'fn reg_res() -> Result<int, string> { return Ok(1) }'
    echo 'fn main() {'
    echo "  $(fixture "$2")"
    if [ "$4" = "void" ]; then echo "  r.$3"; else echo "  v = r.$3"; fi
    echo '  print("REACHED")'
    echo '}'; } > "$1"
}

# A clean compile AND a run that reached the end. Reaching the end is not enough on its
# own: `m.for_each(f)` prints "Unknown method 'for_each' for type 'map'", emits nothing
# for the call, and the binary still runs to completion - so a dead row looked alive.
# Warnings (unused variable) are not failures.
compile_errors=""
builds_and_runs(){   # <file> -> 0 if it compiled cleanly and printed REACHED
  local relflag=""; [ "$MODE" = "release" ] && relflag="--release"
  local out
  out=$("$WYNABS" run $relflag "$1" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')
  compile_errors=$(echo "$out" | grep -E '^Error|Error at line|Unknown method|internal codegen|error:|Parse error' | head -1)
  [ -z "$compile_errors" ] && echo "$out" | grep -q "REACHED"
}

# BOTH MODES. `--release` emits wyn_runtime_slim.h instead of wyn_runtime.h, and that
# header is maintained BY HAND - so a method can be perfectly callable in a debug build
# and fail to compile in release, which is exactly what happened to `map.clear`,
# `int.to_int`, `char.to_int`, `"p".exists()`, `"p".is_dir()` and `"p".is_file()`. A
# debug-only sweep reported all six as fine. The existing release gate
# (run_release_slim_registry_test.sh) enumerates the NAMESPACE registry, not method
# spellings, which is why none of them were caught there either.
for MODE in debug release; do
echo "== mode: $MODE =="
echo "-- every advertised method is callable ($TOTAL of $ROWS rows, batched per receiver)"
# One program per receiver keeps the green path to ~11 compiles instead of ~220. Each
# method gets its OWN fresh receiver inside that program, so a mutating method cannot
# change the answer for a later one. A failing batch falls back to per-method compiles,
# so the report still names the exact method.
for recv in $RECEIVERS; do
  methods=$(awk -F'\t' -v r="$recv" '$1==r {print $2"\t"$3"\t"$4}' "$TMP/pairs.txt")
  batch="$TMP/batch_${MODE}_$recv.wyn"
  n=0
  { echo 'fn reg_opt() -> int? { return Some(1) }'
    echo 'fn reg_res() -> Result<int, string> { return Ok(1) }'
    echo 'fn main() {'
    while IFS=$'\t' read -r meth ret call; do
      [ -z "$meth" ] && continue
      known_broken "$recv.$meth" && continue
      case "$call" in UNRENDERABLE:*|VARIADIC:*) continue;; esac
      n=$((n+1))
      echo "  $(fixture "$recv")" | sed "s/^  r =/  r$n =/; s/^  var r:/  var r$n:/"
      if [ "$ret" = "void" ]; then echo "  r$n.$call"; else echo "  v$n = r$n.$call"; fi
    done <<< "$methods"
    echo '  print("REACHED")'
    echo '}'; } > "$batch"

  if [ "$n" -eq 0 ]; then
    ok "[$MODE] $recv: all rows are on the known-broken list (nothing to call)"
    continue
  fi
  if builds_and_runs "$batch"; then
    ok "[$MODE] $recv: $n advertised methods all callable"
  else
    # Attribute the failure to individual methods.
    while IFS=$'\t' read -r meth ret call; do
      [ -z "$meth" ] && continue
      known_broken "$recv.$meth" && continue
      case "$call" in UNRENDERABLE:*|VARIADIC:*) continue;; esac
      one="$TMP/one_${MODE}_$recv.$meth.wyn"
      emit_one "$one" "$recv" "$call" "$ret"
      if ! builds_and_runs "$one"; then
        why=$(echo "$compile_errors" | cut -c1-90)
        bad "[$MODE] $recv.$meth is advertised by types.c but not callable :: ${why:-reached-no-output}"
      fi
    done <<< "$methods"
    # A batch can fail while every method passes alone (an interaction, not a dead row).
    # Say so rather than reporting a clean sweep.
    if [ "$FAIL" -eq 0 ]; then
      bad "[$MODE] $recv: the batch program failed but every method builds alone - interaction bug"
    fi
  fi
done

echo "-- every row's argument types can be rendered into a call"
# A row whose argument-type column this gate cannot turn into Wyn source is a hole in the
# gate, and a hole is how the arity 1-2 half went unchecked for months. Fail loudly
# instead of skipping quietly.
while IFS=$'\t' read -r recv meth ret call; do
  case "$call" in
    UNRENDERABLE:*) bad "[$MODE] $recv.$meth: this gate cannot generate a call for its argument types (${call#UNRENDERABLE:}) - teach literal() in this file, or fix the row";;
    VARIADIC:*)     echo "  note  [$MODE] $recv.$meth takes variable arguments, so no call is generated here (covered by the string-format tests)";;
  esac
done < "$TMP/pairs.txt"

echo "-- the known-broken list is EXACT (a fixed entry must be removed from it)"
# Without this half the list becomes a place where defects go to be forgotten.
while IFS=$'\t' read -r recv meth ret call; do
  known_broken "$recv.$meth" || continue
  case "$call" in UNRENDERABLE:*|VARIADIC:*) continue;; esac
  one="$TMP/kb_${MODE}_$recv.$meth.wyn"
  emit_one "$one" "$recv" "$call" "$ret"
  if builds_and_runs "$one"; then
    bad "[$MODE] $recv.$meth now WORKS - delete it from known_broken() in this file"
  else
    ok "[$MODE] still broken, still listed: $recv.$meth"
  fi
done < "$TMP/pairs.txt"

echo "-- the higher-order methods give the right ANSWER, not just a clean compile"
# These are NOT a second copy of the sweep above. The sweep proves a row can be CALLED,
# with a generated predicate whose answer it deliberately ignores; these arms assert the
# VALUE, which is the half a generated call cannot check - `.any` returning false for
# `x > 2` on [1, 2, 3] compiles perfectly.
#
# `int.times` is here for a different reason: it has NO row in method_signatures at all
# (it lives only in dispatch_method), so the sweep cannot see it. It and `every` are the
# two that survived the .any/.all fix - both compiled in debug and failed under --release
# on a missing slim-header declaration.
#
# <label> <program-body> <expected-output>
a1(){
  f="$TMP/a1_${MODE}_$1.wyn"
  { echo 'fn reg_nop() -> int { return 1 }'
    echo 'fn main() {'; printf '%b\n' "$2"; echo '}'; } > "$f"
  if [ "$MODE" = "release" ]; then got=$("$WYNABS" run --release "$f" 2>&1)
  else got=$("$WYNABS" run "$f" 2>&1); fi
  got=$(echo "$got" | grep -vE 'Compiled in|^Warning|unused variable' | sed 's/\x1b\[[0-9;]*m//g')
  if [ "$got" = "$3" ]; then ok "[$MODE] $1"
  else bad "[$MODE] $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-90)] want [$3]"; fi
}
a1 "array.any"      '  a = [1, 2, 3]\n  print(a.any(fn(x: int) -> bool { return x > 2 }))' 'true'
a1 "array.any-none" '  a = [1, 2, 3]\n  print(a.any(fn(x: int) -> bool { return x > 9 }))' 'false'
a1 "array.all"      '  a = [1, 2, 3]\n  print(a.all(fn(x: int) -> bool { return x > 0 }))' 'true'
a1 "array.all-not"  '  a = [1, 2, 3]\n  print(a.all(fn(x: int) -> bool { return x > 2 }))' 'false'
a1 "array.map"      '  a = [1, 2, 3]\n  b = a.map(fn(x: int) -> int { return x * 2 })\n  print(b.len())' '3'
a1 "array.filter"   '  a = [1, 2, 3]\n  b = a.filter(fn(x: int) -> bool { return x > 1 })\n  print(b.len())' '2'
a1 "array.contains" '  a = [1, 2, 3]\n  print(a.contains(2))' 'true'
# `every` and `times` are the two that survived the .any/.all fix: both take a function,
# so the generated sweep cannot reach them, and both compiled in debug while failing under
# --release on a missing slim-header declaration. run_slim_header_parity_test.sh now
# catches that class statically; these arms pin the behaviour.
a1 "array.every"     '  a = [1, 2, 3]\n  print(a.every(fn(x: int) -> bool { return x > 0 }))' 'true'
a1 "array.every-not" '  a = [1, 2, 3]\n  print(a.every(fn(x: int) -> bool { return x > 2 }))' 'false'
a1 "int.times"       '  3.times(reg_nop)\n  print("ticked")' 'ticked'
a1 "map.contains"   '  m = {"a": 1}\n  print(m.contains("a"))' 'true'
a1 "set.contains"   '  s = {:"a"}\n  print(s.contains("a"))' 'true'

done   # MODE

echo ""; echo "registry-reachable: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
