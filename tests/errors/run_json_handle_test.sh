#!/bin/bash
# A Json call given the JSON TEXT where a parsed HANDLE belongs must be rejected at
# check time, instead of quietly answering "empty document".
#
#   s = "{\"a\": 1}"
#   print(Json.is_valid(s))     # v1.21.0-dev: prints FALSE for valid JSON, exit 0
#   print(Json.get_int(s, "a")) # prints 0
#   print(Json.keys(s))         # prints []
#
# WHY EVERY ANSWER WAS THE "NOTHING HERE" ANSWER. Json has exactly one representation
# in Wyn: a `long long` index into the runtime's node arena (types.c says so - "Every
# entry is a capital-J handle function"). `Json_parse` is the ONLY Json runtime function
# that takes a `const char*`; all eleven others take that handle. Passing a string meant
# the POINTER was reinterpreted as an arena index, which is never in
# [0, json_node_count), so every reader returned its not-found value and every writer
# became a no-op:
#
#   is_valid -> false   has -> 0    get_int -> 0    get_bool -> false
#   keys -> []          array_len -> 0              stringify -> null
#   set_int / set_string / set_null / free -> silently nothing
#
# That is the worst failure shape available: exit 0, a plausible answer, and a program
# that simply forgot `Json.parse` looks exactly like one that parsed an empty document.
#
# The runtime is NOT at fault and is not changed. `json_member_slot` and
# `json_find_child` both bounds-check the handle, which is why this was a wrong answer
# rather than an out-of-bounds write. The defect is that the CHECKER let a string
# through to a parameter that cannot accept one.
#
# The rejected set is not a taste judgement: it is read off the same table codegen
# lowers through (wyn_json_method_c_function in types.c), so a Json method added there
# is covered here without anyone remembering to. `parse` is the one exclusion, and the
# allow arms pin it. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# reject <label> <body-lines>   (a `s` string variable is in scope)
reject(){
  { echo 'fn main() {'
    echo '  s = "{\"a\": 1, \"b\": [1, 2]}"'
    printf '%b\n' "$2"
    echo '}'; } > "$TMP/r.wyn"
  out=$("$WYNABS" check "$TMP/r.wyn" 2>&1); code=$?
  if [ $code -ne 0 ] && echo "$out" | grep -q "needs a parsed handle"; then
    ok "reject: $1"
  else bad "reject: $1 (code=$code) [$(echo "$out" | tr '\n' '|' | cut -c1-140)]"; fi
}

# allow <label> <body-lines> <expected-stdout>   (a `h` handle is in scope)
allow(){
  d="$TMP/a$PASS$FAIL"; mkdir -p "$d"
  { echo 'fn main() {'
    echo '  h = Json.parse("{\"a\": 1, \"b\": [1, 2]}")'
    printf '%b\n' "$2"
    echo '}'; } > "$d/a.wyn"
  cout=$("$WYNABS" check "$d/a.wyn" 2>&1); ccode=$?
  if [ $ccode -ne 0 ]; then
    bad "allow: $1 - check rejected it [$(echo "$cout" | tr '\n' '|' | cut -c1-140)]"; return
  fi
  got=$("$WYNABS" run "$d/a.wyn" 2>&1 | grep -v 'Compiled in' | grep -v '^Warning' | grep -v 'unused variable')
  if [ "$got" = "$3" ]; then ok "allow: $1"
  else bad "allow: $1 - got [$(echo "$got" | tr '\n' '|')] want [$(echo "$3" | tr '\n' '|')]"; fi
}

echo "-- rejected: every Json READER given the text instead of a handle"
reject "is_valid"         '  print(Json.is_valid(s))'
reject "has"              '  print(Json.has(s, "a"))'
reject "get"              '  print(Json.get(s, "a"))'
reject "get_string"       '  print(Json.get_string(s, "a"))'
reject "get_int"          '  print(Json.get_int(s, "a"))'
reject "get_float"        '  print(Json.get_float(s, "a"))'
reject "get_bool"         '  print(Json.get_bool(s, "a"))'
reject "get_array"        '  v = Json.get_array(s, "b")\n  print("x")'
reject "get_object"       '  v = Json.get_object(s, "a")\n  print("x")'
reject "keys"             '  print(Json.keys(s))'
reject "array_len"        '  print(Json.array_len(s))'
reject "array_get"        '  v = Json.array_get(s, 0)\n  print("x")'
reject "node_str"         '  print(Json.node_str(s))'
reject "stringify"        '  print(Json.stringify(s))'
reject "to_pretty_string" '  print(Json.to_pretty_string(s))'

echo "-- rejected: the WRITERS, which were silent no-ops rather than wrong answers"
reject "set"        '  Json.set(s, "k", "v")\n  print("x")'
reject "set_string" '  Json.set_string(s, "k", "v")\n  print("x")'
reject "set_int"    '  Json.set_int(s, "k", 2)\n  print("x")'
reject "set_float"  '  Json.set_float(s, "k", 1.5)\n  print("x")'
reject "set_bool"   '  Json.set_bool(s, "k", true)\n  print("x")'
reject "set_null"   '  Json.set_null(s, "k")\n  print("x")'
reject "free"       '  Json.free(s)\n  print("x")'

echo "-- rejected: however the string is spelled"
reject "string literal inline"  '  print(Json.is_valid("{\\"a\\": 1}"))'
reject "concatenated string"    '  print(Json.is_valid(s + ""))'
reject "interpolated string"    '  n = 1\n  print(Json.is_valid("{\\"a\\": ${n}}"))'
reject "string from a function" '  print(Json.is_valid(json_text()))\n}\nfn json_text() -> string {\n  return "{}"'

echo "-- allowed: Json.parse is the ONE function that takes the text"
allow "parse then is_valid"  '  print(Json.is_valid(h))' 'true'
allow "parse a second doc"   '  g = Json.parse("{\\"z\\": 9}")\n  print(Json.get_int(g, "z"))' '9'
allow "parse of BAD text is still reportable" '  b = Json.parse("nope")\n  print(Json.is_valid(b))' 'false'

echo "-- allowed: every reader on a real handle"
allow "has"        '  print(Json.has(h, "a"))' '1'
allow "get_int"    '  print(Json.get_int(h, "a"))' '1'
allow "get"        '  print(Json.get(h, "a"))' '1'
allow "keys"       '  print(Json.keys(h))' '["a", "b"]'
allow "array_len"  '  arr = Json.get_array(h, "b")\n  print(Json.array_len(arr))' '2'
allow "stringify"  '  print(Json.stringify(h))' '{"a": 1, "b": [1, 2]}'

echo "-- allowed: the writers, and the method spelling"
allow "set_int then read back" '  Json.set_int(h, "c", 3)\n  print(Json.get_int(h, "c"))' '3'
allow "set_null then stringify" '  Json.set_null(h, "a")\n  print(Json.stringify(h))' '{"a": null, "b": [1, 2]}'
allow "method h.get_int()"     '  print(h.get_int("a"))' '1'
allow "method h.is_valid()"    '  print(h.is_valid())' 'true'
# A handle IS an int, so an int-typed expression must keep working - this rule can only
# ever key on the argument being a STRING.
allow "handle passed through an int var" '  k = h\n  print(Json.is_valid(k))' 'true'

echo ""; echo "json-handle: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
