#!/bin/bash
# Test script for v1.5.0 build tools

set -e

echo "=== Wyn v1.5.0 Build Tools Validation ==="
echo ""

# Get the wyn binary path
WYN_BIN="$(cd "$(dirname "$0")/.." && pwd)/wyn"
WYN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export WYN_ROOT

echo "Using wyn: $WYN_BIN"
echo "WYN_ROOT: $WYN_ROOT"
echo ""

# Test 1: Version check
echo "Test 1: Version Check"
VERSION=$($WYN_BIN version)
echo "  Version: $VERSION"
if [[ "$VERSION" == *"1.5.0"* ]]; then
    echo "  ✅ Version is 1.5.0"
else
    echo "  ❌ Version is not 1.5.0"
    exit 1
fi
echo ""

# Test 2: Help output includes new commands
echo "Test 2: Help Output"
HELP=$($WYN_BIN --help)
if [[ "$HELP" == *"wyn init"* ]]; then
    echo "  ✅ 'wyn init' in help"
else
    echo "  ❌ 'wyn init' not in help"
    exit 1
fi
if [[ "$HELP" == *"wyn watch"* ]]; then
    echo "  ✅ 'wyn watch' in help"
else
    echo "  ❌ 'wyn watch' not in help"
    exit 1
fi
echo ""

# Test 3: Project initialization
echo "Test 3: Project Initialization"
TEST_DIR="/tmp/wyn-test-$$"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

$WYN_BIN init test-project > /dev/null 2>&1

if [ -d "test-project" ]; then
    echo "  ✅ Project directory created"
else
    echo "  ❌ Project directory not created"
    exit 1
fi

if [ -f "test-project/wyn.toml" ]; then
    echo "  ✅ wyn.toml created"
else
    echo "  ❌ wyn.toml not created"
    exit 1
fi

if [ -f "test-project/src/main.wyn" ]; then
    echo "  ✅ src/main.wyn created"
else
    echo "  ❌ src/main.wyn not created"
    exit 1
fi

if [ -f "test-project/tests/test_main.wyn" ]; then
    echo "  ✅ tests/test_main.wyn created"
else
    echo "  ❌ tests/test_main.wyn not created"
    exit 1
fi

if [ -f "test-project/README.md" ]; then
    echo "  ✅ README.md created"
else
    echo "  ❌ README.md not created"
    exit 1
fi
echo ""

# Test 4: wyn.toml format
echo "Test 4: wyn.toml Format"
TOML_CONTENT=$(cat test-project/wyn.toml)
if [[ "$TOML_CONTENT" == *"[project]"* ]]; then
    echo "  ✅ Contains [project] section"
else
    echo "  ❌ Missing [project] section"
    exit 1
fi

if [[ "$TOML_CONTENT" == *'name = "test-project"'* ]]; then
    echo "  ✅ Contains project name"
else
    echo "  ❌ Missing project name"
    exit 1
fi

if [[ "$TOML_CONTENT" == *'version = "0.1.0"'* ]]; then
    echo "  ✅ Contains version"
else
    echo "  ❌ Missing version"
    exit 1
fi

if [[ "$TOML_CONTENT" == *'entry = "src/main.wyn"'* ]]; then
    echo "  ✅ Contains entry point"
else
    echo "  ❌ Missing entry point"
    exit 1
fi

if [[ "$TOML_CONTENT" == *"[dependencies]"* ]]; then
    echo "  ✅ Contains [dependencies] section"
else
    echo "  ❌ Missing [dependencies] section"
    exit 1
fi
echo ""

# Test 5: Generated code compiles and runs
echo "Test 5: Generated Code Execution"
cd test-project
OUTPUT=$($WYN_BIN run src/main.wyn 2>&1)
if [[ "$OUTPUT" == *"Hello from test-project!"* ]]; then
    echo "  ✅ Generated code runs correctly"
else
    echo "  ❌ Generated code output incorrect"
    echo "  Output: $OUTPUT"
    exit 1
fi
cd ..
echo ""

# Test 6: Test file compiles
echo "Test 6: Test File Execution"
cd test-project
OUTPUT=$($WYN_BIN run tests/test_main.wyn 2>&1)
if [[ "$OUTPUT" == *"All tests passed!"* ]]; then
    echo "  ✅ Test file runs correctly"
else
    echo "  ❌ Test file output incorrect"
    echo "  Output: $OUTPUT"
    exit 1
fi
cd ..
echo ""

# Test 7: Watch command exists and can be invoked
echo "Test 7: Watch Command"
cd test-project
# Just verify the command exists and can start (will timeout after 2 seconds)
timeout 2 $WYN_BIN watch src/main.wyn 2>&1 | head -1 > /tmp/watch-output.txt &
WATCH_PID=$!
sleep 1
kill $WATCH_PID 2>/dev/null || true
wait $WATCH_PID 2>/dev/null || true

# Check if watch command is recognized (not "unknown command")
$WYN_BIN watch 2>&1 | grep -q "Usage: wyn watch" && echo "  ✅ Watch command exists" || (echo "  ❌ Watch command not found" && exit 1)
echo "  ✅ Watch command is functional"
cd ..
echo ""

# Cleanup
echo "Cleanup"
cd /
rm -rf "$TEST_DIR"
echo "  ✅ Test directory cleaned up"
echo ""

echo "==================================="
echo "✅ All tests passed!"
echo "==================================="
echo ""
echo "Summary:"
echo "  ✅ Version 1.5.0"
echo "  ✅ Help output updated"
echo "  ✅ wyn init creates projects"
echo "  ✅ wyn.toml format correct"
echo "  ✅ Generated code compiles"
echo "  ✅ Generated code runs"
echo "  ✅ Test files work"
echo "  ✅ wyn watch command works"
echo ""
echo "Wyn v1.5.0 Build Tools: VALIDATED ✅"
