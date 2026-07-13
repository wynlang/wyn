#!/usr/bin/env bash
# `wyn add` integration test: add libm from the curated registry, then build+run
# a program using the generated bindings — proving add → bindgen → [ffi] link →
# compile → run end to end. Uses libm (always available; linked by default).
set -u
WYN_BIN="${WYN:-./wyn}"
# resolve to an absolute path so it works from the temp dir
case "$WYN_BIN" in /*) ;; *) WYN_BIN="$(pwd)/$WYN_BIN" ;; esac
ROOT="$(pwd)"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cd "$work"
export WYN_ROOT="$ROOT"

"$WYN_BIN" add m >/dev/null 2>&1 || { echo "cpkg: FAIL (wyn add m)"; exit 1; }
grep -q 'extern fn sqrt' packages/m/m.wyn 2>/dev/null || { echo "cpkg: FAIL (no sqrt binding)"; exit 1; }
grep -q '\[ffi\]' wyn.toml 2>/dev/null || { echo "cpkg: FAIL (no [ffi] in wyn.toml)"; exit 1; }
grep -q 'libs = "m"' wyn.toml 2>/dev/null || { echo "cpkg: FAIL (libm not linked)"; exit 1; }

cat > app.wyn <<'WYN'
extern fn sqrt(a0: float) -> float;
fn main() { println("${sqrt(169.0)}") }
WYN
out="$("$WYN_BIN" build app.wyn >/dev/null 2>&1 && ./app 2>&1)"
if [ "$out" = "13.0" ]; then echo "cpkg: PASS"; else echo "cpkg: FAIL (got '$out', want 13.0)"; exit 1; fi
