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

# 2. The COMPILE PROBE must pass - that is the property worth gating.
#
# This deliberately does NOT assert `doctor` exits 0. It used to, described as a
# "healthy env", and that premise is false on two of the four CI platforms:
# vendor/tcc/bin/tcc is a committed macOS-ARM64 Mach-O binary, so on
# macos-15-intel (x86_64) and on Linux `doctor` correctly reports
# "✗ Bundled TCC backend - Missing: .../vendor/tcc/bin/tcc" and exits 1. It also
# reports "○ wyn in PATH" on a CI checkout, which is likewise accurate.
#
# doctor is RIGHT in both cases, so weakening doctor would be the wrong fix. The
# environment is genuinely not fully healthy there. What must hold everywhere is
# that the end-to-end compile probe succeeds - i.e. you can actually build and
# run a program - which is what this now checks. That the bundled-TCC fast path
# is macOS-ARM64-only is a real cross-platform gap (see internal-docs), not
# something a test should paper over by asserting rc=0 and hoping.
if echo "$out" | grep -A0 "Compile + run" | grep -q "✓"; then
    ok "doctor's end-to-end compile probe passes"
else
    bad "doctor compile probe failed (rc=$rc)"
    echo "$out" | sed 's/^/          /'
fi

# 3. doctor must exit 0 when it reports NO issues, and non-zero when it does -
#    the exit code has to agree with the human-readable summary either way.
if echo "$out" | grep -q "All good"; then
    if [ $rc -eq 0 ]; then ok "doctor exit code agrees with 'All good'"
    else bad "doctor said 'All good' but exited $rc"; fi
else
    if [ $rc -ne 0 ]; then ok "doctor exits non-zero when it reports issues"
    else bad "doctor reported issues but exited 0"; fi
fi

# 3. A stray ./VERSION in the cwd must NOT change `wyn version`.
echo "9.9.9" > "$TMP/VERSION"
out=$(cd "$TMP" && "$WYN" version 2>&1)
if echo "$out" | grep -q "9.9.9"; then bad "stray ./VERSION hijacked wyn version [$out]"
else ok "stray ./VERSION ignored by wyn version"; fi

echo ""; echo "doctor-version: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
