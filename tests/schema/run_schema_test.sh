#!/usr/bin/env bash
# Gate for src/wyn_schema.c: derive JSON Schemas from hand-built Wyn types and
# assert on the EXACT bytes, then assert that every type the provider's strict
# subset cannot express is refused with an actionable message.
#
# A C unit test rather than a .wyn program because `ai fn` has no syntax yet, so
# nothing in the language can reach schema derivation. Shape borrowed from
# tests/tls/run_tls_seam_test.sh.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CC_BIN="${CC:-cc}"
OUT="${TMPDIR:-/tmp}/wyn_test_schema_of.$$"

# -D_GNU_SOURCE mirrors the project's own CFLAGS and is required, not cosmetic:
# under -std=c11 glibc hides some POSIX declarations behind a feature macro, so a
# file can compile on macOS and hard-error on Linux CI. A no-op on Darwin.
#
# Warnings are failures: this file and src/wyn_schema.c are both ours.
if ! "$CC_BIN" -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -O1 \
      -I "$ROOT/src" \
      -o "$OUT" "$ROOT/tests/schema/test_schema_of.c" "$ROOT/src/wyn_schema.c"; then
  echo "FAIL: schema test did not compile" >&2
  exit 1
fi

# Same alarm() watchdog the rest of the suite uses - there is no `timeout` binary
# on macOS, and a runaway walk must fail rather than stall the suite.
perl -e 'alarm 60; exec @ARGV' "$OUT" 2>&1
rc=$?
rm -f "$OUT"

if [ $rc -ne 0 ]; then
  if [ $rc -ge 128 ]; then
    echo "FAIL: schema test hit the 60s watchdog (signal $((rc - 128)))" >&2
  fi
  exit 1
fi
exit 0
