#!/bin/bash
# `wyn new` scaffolding (T7.5): every template generates a project that
# CHECKS clean and whose tests PASS with `wyn test`. Regression guard: the
# web/api/cli templates shipped with removed `&&`/`||` syntax and didn't
# compile; `--template <name>` (incl. the http-service alias) was ignored.
#
# Also guards the Windows release blocker: scaffolding used to shell out with
# `system("mkdir -p ...")`, which cmd.exe cannot run, so on Windows the src/,
# tests/ and .github/workflows/ dirs were never created. The fix uses a
# portable wyn_mkdir_p() (no shell). We assert the full directory tree exists
# for every template AND for the `wyn init` alias — the exact dirs that were
# created by the removed `mkdir -p` calls.
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
  # Directory tree must exist (broken on Windows by `mkdir -p` shell-out).
  miss=""
  [ -d "$name/src" ]          || miss="$miss src/"
  [ -d "$name/tests" ]        || miss="$miss tests/"
  [ -f "$name/wyn.toml" ]     || miss="$miss wyn.toml"
  [ -f "$name/src/main.wyn" ] || miss="$miss src/main.wyn"
  # Non-default templates also scaffold a nested CI workflow dir.
  if [ "$tpl" != "default" ]; then
    [ -f "$name/.github/workflows/test.yml" ] || miss="$miss .github/workflows/test.yml"
  fi
  if [ -n "$miss" ]; then bad "$tpl: scaffold missing:$miss"; continue; fi
  ( cd "$name" &&
    perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" check src/main.wyn >/dev/null 2>&1 &&
    perl -e 'alarm 90; exec @ARGV' "$WYN_ABS" test >/dev/null 2>&1 )
  if [ $? -eq 0 ]; then ok "$tpl: tree + checks clean + tests pass"; else bad "$tpl: check or test failed"; fi
done

# `wyn init` alias must also build the full tree (same scaffolding path).
perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" init p_init >/dev/null 2>&1
if [ -d p_init/src ] && [ -d p_init/tests ] && [ -f p_init/wyn.toml ] && [ -f p_init/src/main.wyn ]; then
  ok "init alias: full tree created"
else bad "init alias: incomplete tree"; fi

# http-service alias maps to api
perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" new p_alias --template http-service 2>&1 | grep -q "api project" \
  && ok "http-service alias -> api" || bad "http-service alias"

# unknown template: helpful error, nonzero exit
out=$(perl -e 'alarm 15; exec @ARGV' "$WYN_ABS" new p_bad --template rails 2>&1); code=$?
if [ $code -ne 0 ] && echo "$out" | grep -q "Available:"; then
  ok "unknown template: helpful error"
else bad "unknown template: code=$code [$out]"; fi

echo ""; echo "scaffold: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
