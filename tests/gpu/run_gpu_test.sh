#!/bin/bash
# GPU transparent-dispatch tests (macOS/Metal float32 [float].map).
#
# Covers the honesty/correctness contract of the [gpu] feature:
#   1. flag-OFF equivalence  - the SAME .wyn with no [gpu] opt-in emits C with
#      ZERO GPU dispatch sites and produces the same output as any other build.
#   2. float32 opt-in gate   - the lossy GPU path turns on ONLY with BOTH
#      [gpu] enabled = true AND [gpu] float32 = true. `enabled` alone stays CPU.
#   3. GPU correctness       - with the opt-in on and the GPU actually used
#      (WYN_GPU_FORCE=1), an eligible [float].map matches the CPU result within
#      float32 tolerance.
#   4. eligibility gate      - captures / method calls / multi-statement bodies
#      are rejected (0 GPU sites) and fall back to CPU with correct results.
#   5. kill-switch           - no wyn.toml, enabled=false, and enabled=true
#      without float32 all run CPU-only (0 GPU sites).
#
# NOTE on the .out cache: `wyn run` caches <file>.wyn.out by mtime, so we
# always `rm -f` the .out and .c before rebuilding after a toml toggle, and
# use `wyn build` (not run) so each case compiles fresh.
#
# On non-macOS (no Metal) the GPU path is compiled out; the flag-off,
# gate, and kill-switch site-count checks still hold (they assert on emitted
# C, which is platform-independent), and the "GPU actually runs" checks are
# skipped with a clear message.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

IS_MAC=0
[ "$(uname -s)" = "Darwin" ] && IS_MAC=1

# Write a wyn.toml into $TMP with the given [gpu] body ($1 = lines, may be empty
# for "no [gpu] section"; pass "NONE" to write no wyn.toml at all).
set_toml() {
    rm -f "$TMP/wyn.toml"
    [ "$1" = "NONE" ] && return
    { printf '[project]\nname="gpu_t"\nversion="0.1.0"\n'; printf '%b' "$1"; } > "$TMP/wyn.toml"
}

# Build $TMP/p.wyn fresh (busting the .out/.c cache) and echo the number of
# emitted GPU dispatch sites. Sets global BUILD_OK / BUILD_ERR.
build_sites() {
    rm -f "$TMP/p.wyn.out" "$TMP/p.wyn.c" "$TMP/p"
    if ( cd "$TMP" && "$WYN_ABS" build p.wyn >/dev/null 2>build.err ); then
        BUILD_OK=1
        # grep -c exits 1 on zero matches, so capture into a var (avoid `|| echo`
        # which would double-print). Missing .c => 0.
        local n
        n=$(grep -c "wyn_gpu_try_map_float" "$TMP/p.wyn.c" 2>/dev/null)
        echo "${n:-0}"
    else
        BUILD_OK=0; BUILD_ERR=$(head -3 "$TMP/build.err")
        echo 0
    fi
}

# The eligible program used across most cases: a pure-arithmetic float map.
ELIGIBLE_PROG='fn main() {
    var a = [1.0, 2.0, 3.0, 4.0, 5.0]
    var b = a.map((x) => x * 2.5 + 1.0)
    println(b)
}'
EXPECTED_OUT="[3.5, 6.0, 8.5, 11.0, 13.5]"

# --- 1. flag-OFF equivalence: no [gpu] section => 0 sites, and output equals
#        a build with the opt-in on (which also stays CPU under the cost model).
printf '%s\n' "$ELIGIBLE_PROG" > "$TMP/p.wyn"

set_toml "NONE"
sites=$(build_sites)
out_no_toml=$( cd "$TMP" && ./p 2>/dev/null )
[ "$sites" = "0" ] && ok "no wyn.toml => 0 GPU sites (pure CPU codegen)" \
    || bad "no wyn.toml: expected 0 sites, got $sites"

set_toml '[gpu]\nenabled = true\nfloat32 = true\n'
sites_on=$(build_sites)
out_opt_in=$( cd "$TMP" && ./p 2>/dev/null )
[ "$out_no_toml" = "$out_opt_in" ] && ok "flag-off output == opt-in output (behavior equivalence)" \
    || bad "flag-off/opt-in output differ: [$out_no_toml] vs [$out_opt_in]"
[ "$out_no_toml" = "$EXPECTED_OUT" ] && ok "eligible map result correct ($EXPECTED_OUT)" \
    || bad "eligible map wrong: [$out_no_toml]"

# --- 2. float32 opt-in gate ---
set_toml '[gpu]\nenabled = true\n'          # enabled ALONE
sites=$(build_sites)
[ "$sites" = "0" ] && ok "[gpu] enabled=true WITHOUT float32 => 0 GPU sites (CPU doubles)" \
    || bad "enabled-only: expected 0 sites, got $sites"

set_toml '[gpu]\nfloat32 = true\n'          # float32 without enabled
sites=$(build_sites)
[ "$sites" = "0" ] && ok "[gpu] float32=true WITHOUT enabled => 0 GPU sites" \
    || bad "float32-without-enabled: expected 0 sites, got $sites"

set_toml '[gpu]\nenabled = true\nfloat32 = true\n'
sites=$(build_sites)
[ "$sites" = "1" ] && ok "[gpu] enabled + float32 => GPU dispatch site emitted" \
    || bad "both flags: expected 1 site, got $sites"

# --- 2b. cross-platform dual-kernel emission (platform-independent: inspects
#         emitted C). Codegen emits BOTH an MSL kernel (Metal) and an OpenCL-C
#         kernel (OpenCL) at every eligible site; the linked backend picks one
#         via WYN_GPU_KERNEL. This is what makes the SAME .wyn accelerate on
#         macOS/Metal and Linux/Windows/OpenCL.
build_sites >/dev/null   # ensure p.wyn.c is fresh for the both-flags toml
grep -q 'metal_stdlib' "$TMP/p.wyn.c" 2>/dev/null \
    && ok "dual-kernel: MSL kernel emitted (Metal backend source)" \
    || bad "dual-kernel: no MSL kernel in emitted C"
grep -q '__kernel void wyn_map_kernel' "$TMP/p.wyn.c" 2>/dev/null \
    && ok "dual-kernel: OpenCL-C kernel emitted (OpenCL backend source)" \
    || bad "dual-kernel: no OpenCL-C kernel in emitted C"

# --- 3. GPU correctness (float32 tolerance) - only where Metal runs ---
if [ "$IS_MAC" = "1" ]; then
    # WYN_GPU_FORCE=1 makes the cost model dispatch even this tiny array.
    out_gpu=$( cd "$TMP" && WYN_GPU_FORCE=1 ./p 2>/dev/null )
    forced=$( cd "$TMP" && WYN_GPU_FORCE=1 WYN_GPU_DEBUG=1 ./p 2>&1 >/dev/null | grep -c 'FORCE -> GPU' )
    if [ "$forced" -ge 1 ]; then
        ok "WYN_GPU_FORCE=1 actually dispatches the eligible map to the GPU"
    else
        bad "WYN_GPU_FORCE did not dispatch to GPU (no Metal device?)"
    fi
    # WYN_GPU=0 runtime kill-switch: even with FORCE, no dispatch happens, and
    # the result is the (correct) CPU one. Proves the env kill-switch overrides.
    killed=$( cd "$TMP" && WYN_GPU=0 WYN_GPU_FORCE=1 WYN_GPU_DEBUG=1 ./p 2>&1 >/dev/null | grep -c '\-> GPU' )
    out_killed=$( cd "$TMP" && WYN_GPU=0 WYN_GPU_FORCE=1 ./p 2>/dev/null )
    { [ "$killed" = "0" ] && [ "$out_killed" = "$EXPECTED_OUT" ]; } \
        && ok "WYN_GPU=0 forces CPU even under FORCE (kill-switch, correct result)" \
        || bad "WYN_GPU=0 kill-switch failed: dispatches=$killed out=[$out_killed]"
    # float32(2.5x+1) of small exact values is exact => identical string here,
    # which is the tightest possible tolerance check.
    [ "$out_gpu" = "$EXPECTED_OUT" ] && ok "GPU result matches CPU within f32 tolerance ($out_gpu)" \
        || bad "GPU result diverged beyond tolerance: [$out_gpu] vs [$EXPECTED_OUT]"

    # Precision contract, made visible: a large value not representable in
    # float32 loses its low bit on the GPU but is EXACT on the CPU.
    printf 'fn main() {\n  var a = [16777217.0]\n  var b = a.map((x) => x * 1.0)\n  println(b[0] - 16777217.0)\n}\n' > "$TMP/p.wyn"
    set_toml '[gpu]\nenabled = true\nfloat32 = true\n'
    build_sites >/dev/null
    res_gpu=$( cd "$TMP" && WYN_GPU_FORCE=1 ./p 2>/dev/null )
    set_toml '[gpu]\nenabled = true\n'   # CPU (no float32)
    build_sites >/dev/null
    res_cpu=$( cd "$TMP" && ./p 2>/dev/null )
    [ "$res_cpu" = "0.0" ] && ok "CPU path is EXACT for 16777217 (residual 0.0)" \
        || bad "CPU residual not exact: [$res_cpu]"
    [ "$res_gpu" = "-1.0" ] && ok "GPU float32 path is LOSSY for 16777217 (residual -1.0) - opt-in justified" \
        || bad "GPU residual unexpected (env-dependent): [$res_gpu]"
    # restore eligible prog for later reuse
    printf '%s\n' "$ELIGIBLE_PROG" > "$TMP/p.wyn"
else
    echo "  skip  GPU-run checks (no Metal on $(uname -s))"
fi

# --- 4. eligibility gate: ineligible lambdas => 0 sites, still correct on CPU ---
set_toml '[gpu]\nenabled = true\nfloat32 = true\n'

# 4a. captured free variable
printf 'fn main() {\n  var k = 10.0\n  var a = [1.0, 2.0, 3.0]\n  println(a.map((x) => x + k))\n}\n' > "$TMP/p.wyn"
sites=$(build_sites); out=$( cd "$TMP" && ./p 2>/dev/null )
{ [ "$sites" = "0" ] && [ "$out" = "[11.0, 12.0, 13.0]" ]; } \
    && ok "captured variable => CPU fallback, correct ($out)" \
    || bad "capture gate: sites=$sites out=[$out]"

# 4b. method call in the body
printf 'fn main() {\n  var a = [-1.0, 2.0, -3.0]\n  println(a.map((x) => x.abs()))\n}\n' > "$TMP/p.wyn"
sites=$(build_sites); out=$( cd "$TMP" && ./p 2>/dev/null )
{ [ "$sites" = "0" ] && [ "$out" = "[1.0, 2.0, 3.0]" ]; } \
    && ok "method call => CPU fallback, correct ($out)" \
    || bad "method-call gate: sites=$sites out=[$out]"

# 4c. multi-statement lambda body
printf 'fn main() {\n  var a = [1.0, 2.0, 3.0]\n  var r = a.map((x) => {\n    var y = x * 2.0\n    return y + 1.0\n  })\n  println(r)\n}\n' > "$TMP/p.wyn"
sites=$(build_sites); out=$( cd "$TMP" && ./p 2>/dev/null )
{ [ "$sites" = "0" ] && [ "$out" = "[3.0, 5.0, 7.0]" ]; } \
    && ok "multi-statement body => CPU fallback, correct ($out)" \
    || bad "multi-stmt gate: sites=$sites out=[$out]"

# --- 5. kill-switch: these three configs must ALL be CPU-only (0 sites) ---
printf '%s\n' "$ELIGIBLE_PROG" > "$TMP/p.wyn"

set_toml "NONE";                              [ "$(build_sites)" = "0" ] \
    && ok "kill-switch: no wyn.toml => CPU-only" || bad "kill-switch: no toml emitted GPU site"
set_toml '[gpu]\nenabled = false\nfloat32 = true\n'; [ "$(build_sites)" = "0" ] \
    && ok "kill-switch: enabled=false => CPU-only (even with float32=true)" || bad "kill-switch: enabled=false emitted GPU site"
set_toml '[gpu]\nenabled = true\n';           [ "$(build_sites)" = "0" ] \
    && ok "kill-switch: enabled=true without float32 => CPU-only" || bad "kill-switch: enabled-only emitted GPU site"

echo ""
echo "gpu: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ]
