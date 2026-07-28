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
  # Capture check/test output rather than discarding it. `api` and `web` failed
  # on ubuntu-latest while passing on macOS and in a local gcc:13-bookworm
  # container, and "check or test failed" alone gave no way to tell WHICH of the
  # two commands failed, let alone why - the one place you cannot attach a
  # debugger is a CI log.
  # The api/web templates depend on the sqlite package, which links the SYSTEM
  # libsqlite3. A runner without the dev library fails at link time with
  # "undefined reference to `sqlite3_exec'" - an environment gap, not a defect in
  # the template or the compiler. Gate the scaffold+check part always, and only
  # require `wyn test` (which builds and links) where sqlite3 is actually
  # available. Skipping silently would be worse, so say so out loud.
  _needs_sqlite=0
  case "$tpl" in api|web) _needs_sqlite=1 ;; esac
  _have_sqlite=1
  if [ "$_needs_sqlite" = "1" ]; then
    printf '#include <sqlite3.h>\nint main(void){return sqlite3_libversion_number()>0?0:1;}\n' > "$TMP/sqprobe.c"
    ${CC:-cc} "$TMP/sqprobe.c" -lsqlite3 -o "$TMP/sqprobe" 2>/dev/null || _have_sqlite=0
  fi

  _slog="$TMP/scaffold_$tpl.log"
  if [ "$_needs_sqlite" = "1" ] && [ "$_have_sqlite" = "0" ]; then
    # Still gate everything that does not need the library.
    ( cd "$name" && perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" check src/main.wyn 2>&1 ) > "$_slog" 2>&1
    if [ $? -eq 0 ]; then
      echo "  SKIP  $tpl: tree + check clean; 'wyn test' skipped (no system libsqlite3 to link)"
      PASS=$((PASS+1))
    else
      bad "$tpl: check failed"
      sed 's/^/          /' "$_slog" | tail -25
    fi
    continue
  fi

  ( cd "$name" &&
    echo "== wyn check ==" &&
    perl -e 'alarm 30; exec @ARGV' "$WYN_ABS" check src/main.wyn 2>&1 &&
    echo "== wyn test ==" &&
    perl -e 'alarm 90; exec @ARGV' "$WYN_ABS" test 2>&1 ) > "$_slog" 2>&1
  if [ $? -eq 0 ]; then ok "$tpl: tree + checks clean + tests pass"
  else
    bad "$tpl: check or test failed"
    sed 's/^/          /' "$_slog" | tail -25
  fi
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
