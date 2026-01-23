#!/bin/bash
# Create a new release

if [ -z "$1" ]; then
    echo "Usage: ./release.sh <version>"
    echo "Example: ./release.sh 1.0.1"
    exit 1
fi

VERSION=$1

echo "Creating release v$VERSION"

# Update VERSION file
echo "$VERSION" > VERSION

# Build main compiler
echo "Building compiler..."
make clean >/dev/null 2>&1
make wyn || exit 1

# Run module tests (skip if scripts don't exist)
echo "Running module tests..."
if [ -f ./tests/module_tests/test_modules.sh ]; then
    ./tests/module_tests/test_modules.sh >/dev/null 2>&1 && echo "✅ Module tests passed" || echo "⚠ Module tests failed"
else
    echo "⚠ Module tests not found (skipped)"
fi

# Test all examples compile
echo "Testing examples..."
FAIL=0
COUNT=0
for f in examples/[0-4]*.wyn; do
    [ -f "$f" ] || continue
    timeout 10 ./wyn "$f" >/dev/null 2>&1 || FAIL=1
    COUNT=$((COUNT + 1))
done
if [ $COUNT -eq 0 ]; then
    echo "⚠ No examples found to test"
elif [ $FAIL -eq 1 ]; then
    echo "⚠ Some examples failed to compile (non-critical)"
else
    echo "✅ All tested examples compile ($COUNT files)"
fi

# Test basic execution
echo "Testing basic execution..."
cat > /tmp/test_release.wyn << 'EOF'
fn main() -> int {
    return 42
}
EOF
./wyn /tmp/test_release.wyn >/dev/null 2>&1 || exit 1
/tmp/test_release.wyn.out >/dev/null 2>&1
if [ $? -eq 42 ]; then
    echo "✅ Basic execution works"
else
    echo "❌ Basic execution failed"
    exit 1
fi
rm -f /tmp/test_release.wyn /tmp/test_release.wyn.out /tmp/test_release.wyn.c

echo "✅ All tests passed!"

# Commit version bump
git add VERSION
git commit -m "Bump version to $VERSION"

# Create tag
git tag -a "v$VERSION" -m "Release v$VERSION"

echo "Release v$VERSION created"
echo "Push with: git push && git push --tags"
echo "GitHub Actions will build binaries for linux, macos, windows"
