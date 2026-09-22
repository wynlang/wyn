#!/usr/bin/env bash
# Gate for src/wyn_tls.c: compile the seam plus an in-process TLS server and run
# four arms (one happy path, three that must REFUSE). Loopback only - no network
# egress, no openssl CLI, no fixed port.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIB="$ROOT/vendor/mbedtls/lib/libmbedtls_wyn.a"
CC_BIN="${CC:-cc}"
OUT="${TMPDIR:-/tmp}/wyn_test_tls_seam.$$"

if [ ! -f "$LIB" ]; then
  echo "FAIL: $LIB is missing - run 'make mbedtls' first" >&2
  exit 1
fi

# Warnings are failures here: this file is ours, unlike the vendored tree.
if ! "$CC_BIN" -std=c11 -Wall -Wextra -Werror -O1 \
      -I "$ROOT/src" -I "$ROOT/vendor/mbedtls/include" -I "$ROOT/tests/tls" \
      -o "$OUT" "$ROOT/tests/tls/test_tls_seam.c" "$ROOT/src/wyn_tls.c" \
      "$LIB" -lpthread; then
  echo "FAIL: TLS seam test did not compile" >&2
  exit 1
fi

# A handshake that hangs must fail, not stall the suite. Same alarm() watchdog the
# rest of the suite uses - there is no `timeout` binary on macOS.
perl -e 'alarm 60; exec @ARGV' "$OUT" 2>&1
rc=$?
rm -f "$OUT"

if [ $rc -ne 0 ]; then
  if [ $rc -ge 128 ]; then
    echo "FAIL: TLS seam test hit the 60s watchdog (signal $((rc - 128)))" >&2
  fi
  exit 1
fi
exit 0
