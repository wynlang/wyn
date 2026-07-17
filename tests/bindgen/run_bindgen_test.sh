#!/usr/bin/env bash
# Bindgen test: `wyn bind` on a fixture header must emit the expected extern fns
# + constants. Not a run_bdd .wyn test (it invokes a subcommand, not a program).
set -u
WYN="${WYN:-./wyn}"
here="$(cd "$(dirname "$0")" && pwd)"
out="$($WYN bind "$here/sample.h" 2>/dev/null)"
fail=0
check() { if ! echo "$out" | grep -qF "$1"; then echo "  MISSING: $1"; fail=1; fi }
check "extern fn add_one(a0: int) -> int;"
check "extern fn half(a0: float) -> float;"
check "extern fn label(a0: ptr) -> string;"
check "extern fn reset();"
check "extern fn obj_new(a0: int) -> ptr;"
check "extern fn obj_value(a0: ptr) -> int;"
check "extern fn obj_free(a0: ptr);"
check "extern fn exported_fn(a0: int) -> int;"
check "const SAMPLE_MAX = 100"
check 'const SAMPLE_NAME = "sample"'
# must NOT leak system macros
if echo "$out" | grep -q "TARGET_OS\|_LP64\|__"; then echo "  LEAK: system macro emitted"; fail=1; fi
if [ "$fail" -eq 0 ]; then echo "bindgen: PASS"; else echo "bindgen: FAIL"; exit 1; fi
