#!/bin/bash
# One regex contract, asserted on every platform - because Wyn ships TWO regex engines.
#
#   src/wyn_runtime.h:  #ifdef _WIN32  -> the bundled NFA in src/wyn_regex.h (wre_*)
#                       #else          -> POSIX regcomp/regexec
#
# Only the `\d \w \s` expansion is shared (wyn_regex_expand_escapes, deliberately upstream
# of both engines). Everything else - quantifiers, anchors, alternation, groups, replace,
# find, split - is two separate implementations, and NO gate has ever run the Windows one.
# So every regex-using Wyn program could behave differently there and nothing would notice,
# even though ci.yml already builds and tests on windows-latest.
#
# This file is therefore a MEASUREMENT before it is a guard. It asserts only what any
# reasonable ERE engine must do, so a failure on one platform is a real divergence and not
# a matter of taste. If the Windows job reds on this, that IS the finding the issue asked
# for, and the message names the case.
#
# Run in both modes, because Regex_match is one of the eight functions that were declared
# `int` in the slim header while the archive defines them `bool` - which made
# `"abc".ends_with("z")` return true on x86-64 under --release. A regex predicate reading
# its result the wrong width would be the same bug wearing a different hat. (2026-09)
set -uo pipefail
WYN="${WYN:-./wyn}"
WYNABS=$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# rx <label> <body> <expected-stdout>
rx(){
  d="$TMP/r$PASS$FAIL"; mkdir -p "$d"
  { echo 'fn main() {'; printf '%b\n' "$2"; echo '}'; } > "$d/a.wyn"
  for mode in debug release; do
    if [ "$mode" = release ]; then got=$("$WYNABS" run --release "$d/a.wyn" 2>&1); else got=$("$WYNABS" run "$d/a.wyn" 2>&1); fi
    got=$(printf '%s' "$got" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE 'Compiled in|^Warning|unused variable')
    if [ "$got" = "$3" ]; then ok "[$mode] $1"
    else bad "[$mode] $1 - got [$(echo "$got" | tr '\n' '|' | cut -c1-80)] want [$(echo "$3" | tr '\n' '|')]"; fi
  done
}

echo "-- literals and the class escapes (the one shared pass)"
rx "literal present/absent" '  print(Regex.match("hello", "ell"))\n  print(Regex.match("hello", "xyz"))' 'true
false'
rx "\\d matches a digit, not the letter d" '  print(Regex.match("a1b", "\\\\d"))\n  print(Regex.match("adb", "\\\\d"))' 'true
false'
rx "\\w and \\s" '  print(Regex.match("a_1", "\\\\w"))\n  print(Regex.match("a b", "\\\\s"))\n  print(Regex.match("ab", "\\\\s"))' 'true
true
false'
rx "negated classes" '  print(Regex.match("abc", "\\\\D"))\n  print(Regex.match("123", "\\\\D"))' 'true
false'

echo "-- quantifiers"
rx "star / plus" '  print(Regex.match("aaa", "a*"))\n  print(Regex.match("aaa", "a+"))\n  print(Regex.match("bbb", "a+"))' 'true
true
false'
rx "optional" '  print(Regex.match("color", "colou?r"))\n  print(Regex.match("colour", "colou?r"))' 'true
true'

echo "-- anchors"
rx "start / end" '  print(Regex.match("hello", "^hel"))\n  print(Regex.match("hello", "^ello"))\n  print(Regex.match("hello", "llo$"))' 'true
false
true'

echo "-- alternation, groups, character ranges"
rx "alternation" '  print(Regex.match("cat", "cat|dog"))\n  print(Regex.match("dog", "cat|dog"))\n  print(Regex.match("cow", "cat|dog"))' 'true
true
false'
rx "group + quantifier" '  print(Regex.match("abab", "(ab)+"))\n  print(Regex.match("xy", "(ab)+"))' 'true
false'
rx "ranges and sets" '  print(Regex.match("x7", "[0-9]"))\n  print(Regex.match("xy", "[0-9]"))\n  print(Regex.match("b", "[abc]"))' 'true
false
true'

echo "-- replace, find"
rx "replace one and many" '  print(Regex.replace("a1b2", "\\\\d", "#"))\n  print(Regex.replace("abc", "\\\\d", "#"))' 'a#b#
abc'
rx "find returns an index" '  print(Regex.find("hello", "llo"))\n  print(Regex.find("hello", "zzz"))' '2
-1'

echo "-- a bool-returning predicate read at the right width (the #373 shape)"
rx "match in a variable, then branched on" '  m = Regex.match("a1", "\\\\d")\n  if m { print("matched") } else { print("no") }\n  print(m)' 'matched
true'

echo ""; echo "regex-contract: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
