#!/bin/bash
# `wyn new` scaffolding (T7.5): every template generates a project that
# CHECKS clean and whose tests PASS with `wyn test`. Regression guard: the
# web/api/cli templates shipped with removed `&&`/`||` syntax and didn't
# compile; `--template <name>` (incl. the http-service alias) was ignored.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

cd "$TMP"

for tpl in default cli api web; do
  name="p_$tpl"
  if [ "$tpl" = "default" ]; then
    perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" new "$name" >/dev/null 2>&1
  else
    perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" new "$name" --template "$tpl" >/dev/null 2>&1
  fi
  if [ ! -f "$name/src/main.wyn" ]; then bad "$tpl: scaffold missing src/main.wyn"; continue; fi
  ( cd "$name" &&
    perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" check src/main.wyn >/dev/null 2>&1 &&
    perl -e 'alarm 90; exec @ARGV' "$WYN_ABS" test >/dev/null 2>&1 )
  if [ $? -eq 0 ]; then ok "$tpl: checks clean + tests pass"; else bad "$tpl: check or test failed"; fi
done

# http-service alias maps to api
perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" new p_alias --template http-service 2>&1 | grep -q "api project" \
  && ok "http-service alias -> api" || bad "http-service alias"

# unknown template: helpful error, nonzero exit
out=$(perl -e 'alarm 15; exec @ARGV' "$WYN_ABS" new p_bad --template rails 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "Available:"; then
  ok "unknown template: helpful error"
else bad "unknown template: code=$code [$out]"; fi

echo ""; echo "scaffold: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
