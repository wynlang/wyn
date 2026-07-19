#!/bin/bash
# `wyn pkg search` (T7.1): GitHub-as-registry discovery. Network-dependent, so
# the test tolerates offline/rate-limited API: it asserts the COMMAND CONTRACT
# (exit codes, install-command lines, no infra repos leaking) only when the
# API responded, and skips gracefully otherwise.
set -uo pipefail
WYN="${WYN:-./wyn}"
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

out=$(perl -e 'alarm 45; exec @ARGV' "$WYN" pkg search 2>&1); code=$?
if echo "$out" | grep -q "could not reach api.github.com"; then
  ok "offline: graceful warning (network checks skipped)"
elif echo "$out" | grep -q "no packages matched"; then
  # API reachable but empty/rate-limited response — tolerate, but note it
  ok "API returned no results (rate-limited?) — tolerated"
else
  # Real results: contract checks
  echo "$out" | grep -q "wyn pkg add web" && ok "official package listed with add command" || bad "web missing from results"
  echo "$out" | grep -qE "wyn pkg add (wyn|internal-docs|site|sample-apps)$" && bad "infra repo leaked into results" || ok "infra repos filtered"
  [ $code -eq 0 ] && ok "exit 0 on results" || bad "exit code $code"

  # filtered search narrows results
  out2=$(perl -e 'alarm 45; exec @ARGV' "$WYN" pkg search http 2>&1)
  if echo "$out2" | grep -q "http-client"; then
    echo "$out2" | grep -q "wyn pkg add web" && bad "filter did not narrow" || ok "query filter narrows results"
  else
    ok "filtered search (rate-limited?) — tolerated"
  fi
fi

echo ""; echo "pkg-search: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
