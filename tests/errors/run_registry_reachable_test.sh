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
# Scope: the arity-0 entries (121 of the 242), which are the ones that can be called
# without inventing argument types. Arity 1-2 entries are NOT covered - `Option.map` and
# `Option.filter` advertise combinators whose C symbols do not exist either, and they are
# out of reach here for exactly the argument-type reason. (2026-09)
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
    # Advertised, then refused by the checker itself - no lowering exists at all.
    string.to_bytes|map.entries|set.to_array) return 0;;
    # `char` is `int` in Wyn (checker.c: "char is int in Wyn"), so a char-annotated
    # value resolves against the INT receiver table and never reaches these rows.
    # The whole receiver family is dead, not just these names.
    char.is_alpha|char.is_numeric|char.is_alphanumeric|char.is_whitespace) return 0;;
    char.is_uppercase|char.is_lowercase|char.to_upper|char.to_lower) return 0;;
    # Check passes, build fails: the lowering names wyn_hashmap_is_empty, which nothing
    # defines. The only entry in this list that is a BUILD break rather than a dead row.
    map.is_empty) return 0;;
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

# Read the authority. Emits "receiver method returntype" per arity-0 row, de-duplicated
# (the table registers some rows twice - `map.contains` and `map.len` among them).
python3 - "$SRC_TYPES" > "$TMP/pairs.txt" <<'PY'
import re, sys
src = open(sys.argv[1]).read()
m = re.search(r'method_signatures\[\]\s*=\s*\{(.*?)\n\};', src, re.S)
if not m:
    sys.stderr.write("could not find method_signatures[] in types.c\n"); sys.exit(2)
rows = re.findall(r'\{"([a-z_]+)",\s*"([a-zA-Z_0-9]+)",\s*"([^"]+)",\s*(\d+)\}', m.group(1))
seen = set()
for recv, meth, ret, arity in rows:
    if arity == '0' and (recv, meth) not in seen:
        seen.add((recv, meth))
        print(recv, meth, ret)
PY
if [ ! -s "$TMP/pairs.txt" ]; then
  echo "  FAIL  could not read method_signatures[] from $SRC_TYPES"
  echo ""; echo "registry-reachable: 0 pass, 1 fail"; exit 1
fi

TOTAL=$(wc -l < "$TMP/pairs.txt" | tr -d ' ')
RECEIVERS=$(cut -d' ' -f1 "$TMP/pairs.txt" | sort -u)

# emit_one <file> <receiver> <method> <returntype>
# A void method must not have its result bound, or the gate reports a fixture error as a
# broken method.
emit_one(){
  { echo 'fn reg_opt() -> int? { return Some(1) }'
    echo 'fn reg_res() -> Result<int, string> { return Ok(1) }'
    echo 'fn main() {'
    echo "  $(fixture "$2")"
    if [ "$4" = "void" ]; then echo "  r.$3()"; else echo "  v = r.$3()"; fi
    echo '  print("REACHED")'
    echo '}'; } > "$1"
}

builds_and_runs(){   # <file> -> 0 if it printed REACHED
  "$WYNABS" run "$1" 2>&1 | grep -q "REACHED"
}

echo "-- every advertised arity-0 method is callable ($TOTAL rows, batched per receiver)"
# One program per receiver keeps the green path to ~11 compiles instead of ~121. Each
# method gets its OWN fresh receiver inside that program, so a mutating method cannot
# change the answer for a later one. A failing batch falls back to per-method compiles,
# so the report still names the exact method.
for recv in $RECEIVERS; do
  methods=$(awk -v r="$recv" '$1==r {print $2" "$3}' "$TMP/pairs.txt")
  batch="$TMP/batch_$recv.wyn"
  n=0
  { echo 'fn reg_opt() -> int? { return Some(1) }'
    echo 'fn reg_res() -> Result<int, string> { return Ok(1) }'
    echo 'fn main() {'
    while read -r meth ret; do
      [ -z "$meth" ] && continue
      known_broken "$recv.$meth" && continue
      n=$((n+1))
      echo "  $(fixture "$recv")" | sed "s/^  r =/  r$n =/; s/^  var r:/  var r$n:/"
      if [ "$ret" = "void" ]; then echo "  r$n.$meth()"; else echo "  v$n = r$n.$meth()"; fi
    done <<< "$methods"
    echo '  print("REACHED")'
    echo '}'; } > "$batch"

  if [ "$n" -eq 0 ]; then
    ok "$recv: all rows are on the known-broken list (nothing to call)"
    continue
  fi
  if builds_and_runs "$batch"; then
    ok "$recv: $n advertised methods all callable"
  else
    # Attribute the failure to individual methods.
    while read -r meth ret; do
      [ -z "$meth" ] && continue
      known_broken "$recv.$meth" && continue
      one="$TMP/one_$recv.$meth.wyn"
      emit_one "$one" "$recv" "$meth" "$ret"
      if ! builds_and_runs "$one"; then
        why=$("$WYNABS" run "$one" 2>&1 | grep -iE "^Error|Error at line|Unknown method|internal codegen" \
              | head -1 | sed 's/\x1b\[[0-9;]*m//g' | cut -c1-90)
        bad "$recv.$meth is advertised by types.c but not callable :: ${why:-unknown}"
      fi
    done <<< "$methods"
    # A batch can fail while every method passes alone (an interaction, not a dead row).
    # Say so rather than reporting a clean sweep.
    if [ "$FAIL" -eq 0 ]; then
      bad "$recv: the batch program failed but every method builds alone - interaction bug"
    fi
  fi
done

echo "-- the known-broken list is EXACT (a fixed entry must be removed from it)"
# Without this half the list becomes a place where defects go to be forgotten.
while read -r recv meth ret; do
  known_broken "$recv.$meth" || continue
  one="$TMP/kb_$recv.$meth.wyn"
  emit_one "$one" "$recv" "$meth" "$ret"
  if builds_and_runs "$one"; then
    bad "$recv.$meth now WORKS - delete it from known_broken() in this file"
  else
    ok "still broken, still listed: $recv.$meth"
  fi
done < "$TMP/pairs.txt"

echo ""; echo "registry-reachable: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
