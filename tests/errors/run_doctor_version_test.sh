#!/bin/bash
# #9 HONESTY: `wyn doctor` must actually compile+run a trivial program (so it
# can't falsely claim "All good" in an environment where `wyn run` fails), and
# `wyn version` must NOT be hijacked by a stray ./VERSION file in the cwd.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. doctor runs an end-to-end compile probe and reports it.
out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" doctor 2>&1); rc=$?
if echo "$out" | grep -q "Compile + run a trivial program"; then ok "doctor performs a real compile probe"
else bad "doctor missing compile probe [$out]"; fi

# 2. In this (healthy) environment the probe must pass and doctor exit 0.
if [ $rc -eq 0 ] && echo "$out" | grep -q "Compile + run a trivial program"; then
    if echo "$out" | grep -A0 "Compile + run" | grep -q "✓"; then ok "doctor compile probe passes in a healthy env"
    else bad "doctor probe did not pass in healthy env [$out]"; fi
else
    # Print doctor's OWN output on failure. Reporting only "rc=1" told us nothing
    # when this failed on the macOS CI runners while passing locally - doctor
    # checks the bundled TCC, the system cc, the precompiled runtime, git and an
    # end-to-end compile, and which of those is unhappy is the entire diagnosis.
    bad "doctor rc=$rc in healthy env"
    echo "$out" | sed 's/^/          /'
fi

# 3. A stray ./VERSION in the cwd must NOT change `wyn version`.
echo "9.9.9" > "$TMP/VERSION"
out=$(cd "$TMP" && "$WYN" version 2>&1)
if echo "$out" | grep -q "9.9.9"; then bad "stray ./VERSION hijacked wyn version [$out]"
else ok "stray ./VERSION ignored by wyn version"; fi

echo ""; echo "doctor-version: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
