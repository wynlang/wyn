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
# The vendored TLS library is a LINK INPUT for every program that calls Http.* over
# https:// (runtime/libwyn_rt.a holds wyn_tls.o + wyn_https.o, which reference it), so
# it has to be part of the install layout - see release.yml's Package step. Copied
# here so arm 4 below can prove the PACKAGED layout still has HTTPS, on every PR,
# rather than discovering it at a tag.
mkdir -p "$TMP/install/vendor/mbedtls/lib"
[ -f "$ROOT/vendor/mbedtls/lib/libmbedtls_wyn.a" ] && \
  cp "$ROOT/vendor/mbedtls/lib/libmbedtls_wyn.a" "$TMP/install/vendor/mbedtls/lib/"
W="$TMP/install/bin/wyn"

# 1. hello world from a foreign cwd
mkdir -p "$TMP/work" && cd "$TMP/work"
printf 'fn main() {\n    println("install-ok")\n}\n' > hello.wyn
out=$(perl -e 'alarm(60); exec @ARGV' -- "$W" run hello.wyn 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "install-ok"; then ok "installed binary runs hello world"; else bad "hello: rc=$rc [$(echo "$out" | tail -2)]"; fi

# 1b. Same, but invoked as a bare name through PATH - how install.sh users
# actually call it. argv[0] is then just "wyn" (no directory), which used to
# make root resolution fall back to "." and probe the USER'S CWD for src/ -
# every PATH invocation died with an internal codegen error. The v1.19.0
# release canary caught this on its first gated tag.
rm -f hello.wyn.out hello
out=$(perl -e 'alarm(60); exec @ARGV' -- env PATH="$TMP/install/bin:$PATH" wyn run hello.wyn 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "install-ok"; then ok "PATH-lookup invocation works (bare argv0)"; else bad "PATH hello: rc=$rc [$(echo "$out" | tail -2)]"; fi

# 2. version is real (not the old 1.10.0 fallback)
out=$("$W" version 2>&1)
if echo "$out" | grep -q "1.10.0"; then bad "version fallback 1.10.0"; else ok "version reports real version"; fi

# 3. scaffold + test (the first-project flow)
rm -rf demo
"$W" new demo --template cli >/dev/null 2>&1
cd demo
out=$(perl -e 'alarm(120); exec @ARGV' -- "$W" test 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "passed"; then ok "wyn new + wyn test in installed layout"; else bad "new+test: rc=$rc [$(echo "$out" | tail -2)]"; fi

# 4. The installed layout can still do HTTPS. Asserted NEGATIVELY, against the
# runtime's own message, because that is the only difference a user can see: when the
# TLS library is missing from the layout, programs still BUILD and still RUN - every
# https:// call just returns "HTTPS unavailable: this binary was linked without the
# TLS backend". A packaging omission is therefore invisible to every other arm here.
# .invalid can never resolve (RFC 2606), so this needs no network.
cd "$TMP/work"
cat > tlscheck.wyn <<'WYN'
fn main() {
    var body = http_get("https://wyn-install-canary.invalid/x")
    println("err=${http_error()}")
    println("len=${body.len()}")
}
WYN
out=$(perl -e 'alarm(120); exec @ARGV' -- "$W" run tlscheck.wyn 2>&1); rc=$?
if [ $rc -ne 0 ]; then
  bad "installed layout builds an HTTPS program: rc=$rc [$(echo "$out" | tail -2)]"
elif echo "$out" | grep -q "HTTPS unavailable"; then
  bad "installed layout has NO TLS backend - vendor/mbedtls/lib is missing from the package"
elif echo "$out" | grep -q "len=0"; then
  ok "installed layout links the TLS backend (https fails cleanly, not 'unavailable')"
else
  bad "installed layout HTTPS canary: unexpected output [$(echo "$out" | tail -2)]"
fi

echo ""; echo "install-layout: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
