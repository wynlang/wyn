#!/usr/bin/env bash
# A precompiled header that the C compiler refuses must not fail the build.
#
# THE DEFECT
#
# On macOS the dev loop injects `-include-pch runtime/wyn_runtime.pch` (parsing
# wyn_runtime.h is ~2/3 of the -O0 compile: 388ms with it, 6120ms without). Whether to
# inject it was decided by comparing MTIMES - pch newer than src/wyn_runtime.h - and an
# mtime cannot see the thing that actually invalidates a pch most often: the TOOLCHAIN
# changing under it. clang then hard-errors
#
#   error: PCH file '.../runtime/wyn_runtime.pch' built from a different branch
#          ((clang-2100.1.1.101)) than the compiler ((clang-2100.3.34.2))
#
# on EVERY compile. And `make runtime`'s pch rule depends on the header and the
# Makefile, neither of which moved, so it reports nothing to do - the tree stays broken
# until someone deletes the file by hand. Measured: after an Xcode update, a full
# `make test` reported all ~300 tests as BUILD FAILED, which reads like a compiler
# regression rather than a stale artifact.
#
# A pch is a CACHE. A cache miss must be recoverable, so `wyn build` now retries once
# without it and removes the dead file.
#
# WHY A SHELL TEST AND NOT AN EXPECT FILE
#
# What is under test is the state of a build ARTIFACT outside any Wyn program, plus the
# artifact's removal afterwards. A tests/regression/ EXPECT file can express neither.
#
# WHY A THROWAWAY WYN_ROOT
#
# The obvious version of this test corrupts the real runtime/wyn_runtime.pch and puts it
# back. The suite compiles with 12+ parallel jobs against that one file, so doing this
# in the shared tree would make every concurrent test fail - a self-inflicted flake of
# exactly the kind this suite has been bitten by before. Instead the test builds a
# private WYN_ROOT that SYMLINKS src/ and libwyn_rt.a and owns only its own pch, so
# nothing it does is visible to another job.
#
# HOW THE BAD PCH IS MADE
#
# Not by writing garbage - a corrupt file gives a different clang error, and the point
# is the mismatch class. It is a REAL pch built with mismatched flags (-fwrapv dropped),
# which is a documented hard clang error ("... differs in precompiled file") and is the
# same refusal path as a version mismatch. Dropping -fwrapv is not hypothetical: adding
# it to the compile line without adding it to the pch rule broke every macOS build once
# already, which is why the Makefile rule lists the Makefile as a dependency.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
REAL_ROOT="$(cd "$(dirname "$WYN_ABS")" && pwd)"

pass=0
fail=0
check() {
    if [ "$2" = "$3" ]; then
        echo "  ok    $1"
        pass=$((pass+1))
    else
        echo "  FAIL  $1"
        echo "          expected: $3"
        echo "          actual:   $2"
        fail=$((fail+1))
    fi
}

# Not macOS: there is no pch on this platform, so there is nothing to assert.
if [ "$(uname)" != "Darwin" ]; then
    echo "stale-pch: skipped (macOS-only dev-loop pch)"
    exit 0
fi
# No prebuilt runtime library means the from-source branch, which never injects a pch.
if [ ! -r "$REAL_ROOT/runtime/libwyn_rt.a" ]; then
    echo "stale-pch: skipped (no runtime/libwyn_rt.a)"
    exit 0
fi

# Recorded BEFORE anything runs, so the last check can prove the shared tree was left
# exactly as it was found (either state is fine - only a CHANGE would be a bug).
PCH_BEFORE_STATE="$([ -e "$REAL_ROOT/runtime/wyn_runtime.pch" ] && echo present || echo absent)"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

root="$work/root"
mkdir -p "$root/runtime"
ln -s "$REAL_ROOT/src" "$root/src"
ln -s "$REAL_ROOT/vendor" "$root/vendor"
ln -s "$REAL_ROOT/runtime/libwyn_rt.a" "$root/runtime/libwyn_rt.a"

PCH="$root/runtime/wyn_runtime.pch"
CC="${CC:-cc}"
# The real rule's flags, minus -fwrapv. Everything else must match or clang rejects the
# pch for a different reason and the test would pass for the wrong one.
"$CC" -x c-header -std=c11 -O0 -w -Wno-int-conversion -ffunction-sections -fdata-sections \
      -I "$root/src" "$root/src/wyn_runtime.h" -o "$PCH" 2>/dev/null
if [ ! -s "$PCH" ]; then
    echo "stale-pch: skipped (could not build a mismatched pch with $CC)"
    exit 0
fi
# Newer than the header, which is what makes the compiler consider it usable.
touch "$PCH"

cat > "$work/hello.wyn" <<'EOF'
fn main() {
    print("pch ok")
}
EOF

check "the mismatched pch is in place before the build" "$([ -s "$PCH" ] && echo yes || echo no)" "yes"

cd "$work" || exit 1
WYN_ROOT="$root" "$WYN_ABS" build hello.wyn > build.log 2>&1
build_rc=$?

check "wyn build survives a pch the C compiler refuses" "$build_rc" "0"
check "the binary runs and prints its output" "$([ -x ./hello ] && ./hello 2>/dev/null || echo '<no binary>')" "pch ok"

# The dead pch must be gone, or the next build pays the same failed compile again.
check "the stale pch was removed" "$([ -e "$PCH" ] && echo present || echo removed)" "removed"

# And the recovery must say so - a silent self-heal leaves the dev loop permanently on
# the 15x-slower path with no hint that `make runtime` would fix it.
said=$(grep -c 'stale precompiled header' build.log 2>/dev/null || true)
check "the build explains the recovery" "$([ "$said" -gt 0 ] && echo yes || echo no)" "yes"

# A second build, now with no pch at all, must still work - the retry must not have left
# a mangled command line behind.
rm -f ./hello
WYN_ROOT="$root" "$WYN_ABS" build hello.wyn > build2.log 2>&1
check "the next build (no pch present) still succeeds" "$?" "0"
check "and still produces a working binary" "$([ -x ./hello ] && ./hello 2>/dev/null || echo '<no binary>')" "pch ok"

# The real tree's pch must be untouched: this test owns a private copy precisely so it
# cannot disturb a parallel job.
check "the shared runtime pch was not touched" \
    "$([ -e "$REAL_ROOT/runtime/wyn_runtime.pch" ] && echo present || echo absent)" \
    "$PCH_BEFORE_STATE"

echo ""
echo "stale-pch: $pass pass, $fail fail"
[ "$fail" -eq 0 ]
