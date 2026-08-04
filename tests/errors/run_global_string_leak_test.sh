#!/usr/bin/env bash
# Assigning a built string to a module-level GLOBAL must release the old value.
#
# THE DEFECT
#
# A string LOCAL got an ownership-transfer assignment:
#     ({ const char* __rc_tmp = concat(...); if (__rc_tmp != g) { wyn_rc_release(g); } g = __rc_tmp; })
# A string GLOBAL got a bare one:
#     g = concat(...);
# so every assignment leaked the previous string. Two causes compounded: module-level
# string globals were never passed to register_string_var (codegen_program.c), and the
# per-function reset_string_vars() would have discarded them anyway.
#
# Measured on an identical 300k-iteration loop: 29.1 MB writing to a global versus
# 1.5 MB writing to a local. It is a CAP, not a slowdown - a long-running program that
# accumulates into a global cannot finish.
#
# WHY A SHELL TEST
#
# The assertion is about PEAK MEMORY, which no EXPECT file can express. It compares the
# global form against the local form rather than against a fixed MB figure, so it does
# not need retuning when the allocator or the runtime changes.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"

pass=0
fail=0
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1

cat > glob.wyn <<'WYN'
acc = ""
fn main() -> int {
    var i = 0
    while i < 300000 { acc = "abcdefghij" + ""; i = i + 1 }
    print(acc)
    return 0
}
WYN
cat > loc.wyn <<'WYN'
fn main() -> int {
    var i = 0
    var acc = ""
    while i < 300000 { acc = "abcdefghij" + ""; i = i + 1 }
    print(acc)
    return 0
}
WYN

# Peak RSS in KB, or 0 if this host cannot measure it.
#
# There is no portable way to do this. macOS has BSD `/usr/bin/time -l` reporting
# BYTES; GNU coreutils has `-v` reporting KILOBYTES; a bare Debian image (which is what
# CI runs) has NO /usr/bin/time at all, only the bash builtin, which reports no memory.
# Units are normalised by which tool answered rather than by magnitude - a magnitude
# guess mislabelled a 1.4 MB result (1507328 bytes) as 1.4 GB, and a test that prints a
# wrong number will eventually be believed.
peak_kb() {
    local raw
    if [ "$(uname -s)" = "Darwin" ] && [ -x /usr/bin/time ]; then
        raw=$(/usr/bin/time -l "$1" 2>&1 >/dev/null | awk '/maximum resident/ {print $1}')
        [ -z "$raw" ] && { echo 0; return; }
        echo $((raw / 1024))          # BSD reports bytes
    elif [ -x /usr/bin/time ]; then
        raw=$(/usr/bin/time -v "$1" 2>&1 >/dev/null | awk '/Maximum resident/ {print $NF}')
        [ -z "$raw" ] && { echo 0; return; }
        echo "$raw"                    # GNU reports KB
    else
        echo 0                         # unmeasurable here
    fi
}

for f in glob loc; do
    if ! "$WYN_ABS" build "$f.wyn" --release -o "$f.bin" >/dev/null 2>&1; then
        echo "  FAIL  $f.wyn did not build"
        fail=$((fail+1))
    fi
done

if [ -x ./glob.bin ] && [ -x ./loc.bin ]; then
    got_g=$(./glob.bin | tail -1)
    got_l=$(./loc.bin | tail -1)
    if [ "$got_g" = "abcdefghij" ] && [ "$got_l" = "abcdefghij" ]; then
        echo "  ok    both forms print the right answer"; pass=$((pass+1))
    else
        echo "  FAIL  wrong output: global=[$got_g] local=[$got_l]"; fail=$((fail+1))
    fi

    kg=$(peak_kb ./glob.bin)
    kl=$(peak_kb ./loc.bin)
    if [ "$kg" -eq 0 ] || [ "$kl" -eq 0 ]; then
        # SKIP, not FAIL. A host with no way to read peak RSS cannot judge this, and
        # failing there would make the suite red for a missing tool rather than for a
        # defect - CI's Debian image has no /usr/bin/time at all. The correctness check
        # above still runs everywhere.
        echo "  ~     peak RSS not measurable on this host - memory check skipped"
    elif [ "$kg" -le $((kl * 4)) ]; then
        # 4x is a wide margin on purpose: it catches the 20x leak without failing on
        # allocator noise.
        echo "  ok    a global assignment does not leak (global ${kg}KB vs local ${kl}KB)"; pass=$((pass+1))
    else
        echo "  FAIL  global peak ${kg}KB vs local ${kl}KB - the old value is not released"; fail=$((fail+1))
    fi
fi

echo ""
echo "global-string-leak: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
