#!/bin/bash
# Build TCC from source for the current platform
# Used by CI/CD to include TCC in release packages
set -e

TCC_VERSION="0.9.28rc"
TCC_REPO="https://repo.or.cz/tinycc.git"
TCC_BRANCH="mob"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WYN_ROOT="$(dirname "$SCRIPT_DIR")"
VENDOR_DIR="$WYN_ROOT/vendor/tcc"

# Detect platform
UNAME_S=$(uname -s)
UNAME_M=$(uname -m)

echo "Building TCC for $UNAME_S $UNAME_M..."

# Clone TCC source
TCC_SRC=$(mktemp -d)
git clone --depth 1 --branch "$TCC_BRANCH" "$TCC_REPO" "$TCC_SRC" 2>/dev/null

cd "$TCC_SRC"

# Configure for current platform
case "$UNAME_S" in
    Darwin)
        ./configure --prefix="$VENDOR_DIR" --cc=clang
        ;;
    Linux)
        ./configure --prefix="$VENDOR_DIR" --cc=gcc
        ;;
    MINGW*|MSYS*|CYGWIN*)
        ./configure --prefix="$VENDOR_DIR" --cc=gcc --cross-prefix=
        ;;
    *)
        echo "Unsupported platform: $UNAME_S"
        exit 1
        ;;
esac

make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Install to vendor directory
mkdir -p "$VENDOR_DIR/bin" "$VENDOR_DIR/lib" "$VENDOR_DIR/include" "$VENDOR_DIR/tcc_include"
cp tcc "$VENDOR_DIR/bin/tcc" 2>/dev/null || cp tcc.exe "$VENDOR_DIR/bin/tcc.exe" 2>/dev/null
cp libtcc.a "$VENDOR_DIR/lib/"
cp libtcc1.a "$VENDOR_DIR/lib/" 2>/dev/null || true
cp libtcc.h "$VENDOR_DIR/include/"

# Copy TCC include files
cp -r include/* "$VENDOR_DIR/tcc_include/" 2>/dev/null || true

# Build TCC runtime library for Wyn
echo "Building libwyn_rt_tcc.a..."
cd "$WYN_ROOT"
mkdir -p /tmp/tcc_rt_build
for f in src/wyn_arena.c src/runtime_exports.c src/wyn_wrapper.c src/wyn_interface.c src/io.c src/optional.c src/result.c src/arc_runtime.c src/concurrency.c src/async_runtime.c src/safe_memory.c src/error.c src/string_runtime.c src/hashmap.c src/hashset.c src/json.c src/json_runtime.c src/stdlib_runtime.c src/hashmap_runtime.c src/stdlib_string.c src/stdlib_array.c src/stdlib_time.c src/stdlib_crypto.c src/stdlib_math.c src/spawn.c src/spawn_fast.c src/future.c src/net.c src/net_runtime.c src/test_runtime.c src/net_advanced.c src/file_io_simple.c src/stdlib_enhanced.c; do
    "$VENDOR_DIR/bin/tcc" -c -I src -w "$f" -o "/tmp/tcc_rt_build/$(basename "$f" .c).o" 2>/dev/null || true
done
# Use ar rc (no ranlib) for TCC-compatible archive
ar rc "$VENDOR_DIR/lib/libwyn_rt_tcc.a" /tmp/tcc_rt_build/*.o
rm -rf /tmp/tcc_rt_build

# Cleanup
rm -rf "$TCC_SRC"

echo "✅ TCC $TCC_VERSION built for $UNAME_S $UNAME_M"
echo "   Binary: $VENDOR_DIR/bin/tcc"
echo "   Library: $VENDOR_DIR/lib/libtcc.a"
echo "   Runtime: $VENDOR_DIR/lib/libwyn_rt_tcc.a"
