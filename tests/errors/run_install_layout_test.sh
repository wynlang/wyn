#!/bin/bash
# Install-layout canary: simulate the EXACT release-tarball layout
# (bin/wyn + src/ + runtime/ + vendor/ + VERSION) in a temp dir and run the
# first-five-minutes flow from a foreign cwd. Every release before 2026-07-20
# shipped broken because nothing exercised this path: the binary could not
# find src/wyn_runtime.h from bin/ (missing parent probe) and reported
# v1.10.0 (cwd-dependent version). Mirrors release.yml's install-canary job.
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
ROOT="$(dirname "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# Build the installed layout.
mkdir -p "$TMP/install/bin" "$TMP/install/runtime" "$TMP/install/vendor"
cp "$WYN" "$TMP/install/bin/wyn"
cp -r "$ROOT/src" "$TMP/install/"
cp "$ROOT/VERSION" "$TMP/install/" 2>/dev/null || echo "0.0.0-test" > "$TMP/install/VERSION"
[ -d "$ROOT/vendor/minicoro" ] && cp -r "$ROOT/vendor/minicoro" "$TMP/install/vendor/"
[ -f "$ROOT/runtime/libwyn_rt.a" ] && cp "$ROOT/runtime/libwyn_rt.a" "$TMP/install/runtime/"
W="$TMP/install/bin/wyn"

# 1. hello world from a foreign cwd
mkdir -p "$TMP/work" && cd "$TMP/work"
printf 'fn main() {\n    println("install-ok")\n}\n' > hello.wyn
out=$(perl -e 'alarm(60); exec @ARGV' -- "$W" run hello.wyn 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "install-ok"; then ok "installed binary runs hello world"; else bad "hello: rc=$rc [$(echo "$out" | tail -2)]"; fi

# 2. version is real (not the old 1.10.0 fallback)
out=$("$W" version 2>&1)
if echo "$out" | grep -q "1.10.0"; then bad "version fallback 1.10.0"; else ok "version reports real version"; fi

# 3. scaffold + test (the first-project flow)
rm -rf demo
"$W" new demo --template cli >/dev/null 2>&1
cd demo
out=$(perl -e 'alarm(120); exec @ARGV' -- "$W" test 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "passed"; then ok "wyn new + wyn test in installed layout"; else bad "new+test: rc=$rc [$(echo "$out" | tail -2)]"; fi

echo ""; echo "install-layout: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
