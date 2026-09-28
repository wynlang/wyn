#!/usr/bin/env bash
# Gate for the native HTTPS transport (src/wyn_https.c).
#
# Two halves:
#   1. compile + run tests/https/test_https_request.c against an in-process TLS
#      server on loopback (no network egress, no fixed port, no committed key);
#   2. a TRIPWIRE: neither the runtime source nor the SHIPPED runtime archive may
#      contain `openssl s_client` or a popen() in the https_* paths. That is the
#      regression that must never come back - it was a remote code execution bug
#      (the URL and POST body were spliced into a shell command) live in a
#      released compiler, and it would be invisible to every behavioural test
#      here, because shelling out to openssl also "works".
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIB="$ROOT/vendor/mbedtls/lib/libmbedtls_wyn.a"
CC_BIN="${CC:-cc}"
OUT="${TMPDIR:-/tmp}/wyn_test_https.$$"
rc_total=0

if [ ! -f "$LIB" ]; then
  echo "FAIL: $LIB is missing - run 'make mbedtls' first" >&2
  exit 1
fi

# On Windows the trust store is CryptoAPI's "ROOT" (src/wyn_tls.c calls
# CertOpenSystemStoreA), so the link needs -lcrypt32 and -lws2_32 there. ci.yml's
# Windows job does not currently run this suite - it runs a named portable subset -
# but a gate that only links on the platform it happens to be run on is a trap for
# whoever adds it to that list.
EXTRA_LIBS="-lpthread"
case "$(uname -s 2>/dev/null || echo unknown)" in
  MINGW*|MSYS*|CYGWIN*) EXTRA_LIBS="-lws2_32 -lcrypt32 -lpthread" ;;
esac

# -D_GNU_SOURCE mirrors the project's own CFLAGS and is required, not cosmetic:
# under -std=c11 glibc hides setenv/unsetenv behind a feature macro, so this
# compiles on macOS and hard-errors on Linux CI. A no-op on Darwin.
#
# Warnings are failures: this file and src/wyn_https.c are ours, unlike vendor/.
if ! "$CC_BIN" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -O1 \
      -I "$ROOT/src" -I "$ROOT/vendor/mbedtls/include" \
      -o "$OUT" "$ROOT/tests/https/test_https_request.c" \
      "$ROOT/src/wyn_https.c" "$ROOT/src/wyn_tls.c" \
      "$LIB" $EXTRA_LIBS; then
  echo "FAIL: native HTTPS test did not compile" >&2
  exit 1
fi

# A hung handshake or an un-terminated read loop must FAIL, not stall the suite.
# Same alarm() watchdog the rest of the suite uses - macOS has no `timeout`.
perl -e 'alarm 90; exec @ARGV' "$OUT" 2>&1
rc=$?
rm -f "$OUT"
if [ $rc -ne 0 ]; then
  if [ $rc -ge 128 ]; then
    echo "FAIL: native HTTPS test hit the 90s watchdog (signal $((rc - 128)))" >&2
  fi
  rc_total=1
fi

echo "--- tripwire: no shell-spliced TLS anywhere in the HTTPS path ---"

# 1. The source. COMMENT LINES ARE STRIPPED FIRST: both wyn_runtime.h and
#    wyn_https.c explain, in prose, the command they replaced, and a gate that
#    forbids naming a fixed bug only teaches people to delete the explanation.
#    What must never come back is the command in CODE.
src_hits=$(grep -hn "openssl s_client" "$ROOT/src/wyn_runtime.h" \
             "$ROOT/src/wyn_runtime_slim.h" "$ROOT/src/wyn_https.c" 2>/dev/null \
           | grep -v ':[[:space:]]*\(//\|\*\|/\*\)' || true)
if [ -n "$src_hits" ]; then
  echo "$src_hits"
  echo "FAIL: 'openssl s_client' is back in the runtime CODE - that is the RCE" >&2
  rc_total=1
else
  echo "  ok    no 'openssl s_client' in runtime code (outside comments)"
fi

# 2. No popen()/system() inside the four https_* implementations - i.e. from
#    wyn_https_body (the single call site) down to http_set_header, which spans
#    https_get / https_post / http_put / http_delete. Scoped to that span on purpose:
#    unrelated popen() sites elsewhere in the header are not in this gate's remit, and
#    widening it would fail the gate for the wrong reason the first time someone
#    touches one. Comment lines are dropped so the prose above the span - which names
#    the popen() being removed - cannot trip it.
shell_hits=$(awk '/^static char\* wyn_https_body\(/,/^void http_set_header\(/' \
               "$ROOT/src/wyn_runtime.h" \
             | grep -v '^[[:space:]]*\(//\|\*\|/\*\)' \
             | grep -n "popen(\|system(" || true)
if [ -n "$shell_hits" ]; then
  echo "$shell_hits"
  echo "FAIL: a popen()/system() is back in the https_* / http_put / http_delete paths" >&2
  rc_total=1
else
  echo "  ok    no popen()/system() in the https_* paths"
fi

# 3. The ARTIFACT, not just the source - that is the thing users run. A stale
#    archive with the old string in it is exactly the failure mode the project's
#    "verify the artifact, not the exit code" lesson is about.
RT="$ROOT/runtime/libwyn_rt.a"
if [ -f "$RT" ]; then
  # `| grep -q` would SIGPIPE `strings`/`nm`, and `set -o pipefail` turns that into
  # rc=141 - i.e. a check that PASSED reported as a failure. Capture, then test.
  art_hits=$(strings "$RT" 2>/dev/null | grep -c "openssl s_client" || true)
  if [ "${art_hits:-0}" -ne 0 ]; then
    echo "FAIL: runtime/libwyn_rt.a still contains 'openssl s_client'" >&2
    rc_total=1
  else
    echo "  ok    runtime/libwyn_rt.a carries no 'openssl s_client' string"
  fi
  # The native transport must actually BE in the shipped archive. Without this,
  # a Makefile that forgot src/wyn_https.c would leave every program linking the
  # old behaviour with no test able to tell.
  sym_hits=$(nm "$RT" 2>/dev/null | grep -c "wyn_https_request" || true)
  if [ "${sym_hits:-0}" -ne 0 ]; then
    echo "  ok    runtime/libwyn_rt.a exports wyn_https_request"
  else
    echo "FAIL: runtime/libwyn_rt.a does not contain wyn_https_request (RT_SRCS wiring)" >&2
    rc_total=1
  fi
else
  echo "FAIL: runtime/libwyn_rt.a is missing - run 'make runtime'" >&2
  rc_total=1
fi

exit $rc_total
