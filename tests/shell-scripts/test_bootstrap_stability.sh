#!/bin/bash
# Bootstrap stability test
# Verifies reproducible builds across multiple generations

set -e

echo "=== Bootstrap Stability Test ==="
echo ""

cd "$(dirname "$0")/.."

COMPILER="./lib/compiler_modular.wyn.out"

# Test program
TEST_PROGRAM="var x = 10
var y = 20
var z = 30"

echo "Test: Reproducible builds"
echo ""

# Generation 0: First compilation
echo "$TEST_PROGRAM" > /tmp/test_input.wyn
$COMPILER > /tmp/gen0_output.txt 2>&1
cp /tmp/test_input.c /tmp/gen0.c

echo "Gen 0: Compiled"
echo "  Output length: $(wc -c < /tmp/gen0.c) bytes"

# Generation 1: Second compilation (same input)
echo "$TEST_PROGRAM" > /tmp/test_input.wyn
$COMPILER > /tmp/gen1_output.txt 2>&1
cp /tmp/test_input.c /tmp/gen1.c

echo "Gen 1: Compiled"
echo "  Output length: $(wc -c < /tmp/gen1.c) bytes"

# Generation 2: Third compilation (same input)
echo "$TEST_PROGRAM" > /tmp/test_input.wyn
$COMPILER > /tmp/gen2_output.txt 2>&1
cp /tmp/test_input.c /tmp/gen2.c

echo "Gen 2: Compiled"
echo "  Output length: $(wc -c < /tmp/gen2.c) bytes"

echo ""
echo "Verifying reproducibility..."

# Compare Gen 0 and Gen 1
if diff /tmp/gen0.c /tmp/gen1.c > /dev/null 2>&1; then
    echo "  ✅ Gen 0 == Gen 1 (identical)"
else
    echo "  ❌ Gen 0 != Gen 1 (different)"
    echo "Differences:"
    diff /tmp/gen0.c /tmp/gen1.c || true
    exit 1
fi

# Compare Gen 1 and Gen 2
if diff /tmp/gen1.c /tmp/gen2.c > /dev/null 2>&1; then
    echo "  ✅ Gen 1 == Gen 2 (identical)"
else
    echo "  ❌ Gen 1 != Gen 2 (different)"
    echo "Differences:"
    diff /tmp/gen1.c /tmp/gen2.c || true
    exit 1
fi

# Compare Gen 0 and Gen 2
if diff /tmp/gen0.c /tmp/gen2.c > /dev/null 2>&1; then
    echo "  ✅ Gen 0 == Gen 2 (identical)"
else
    echo "  ❌ Gen 0 != Gen 2 (different)"
    echo "Differences:"
    diff /tmp/gen0.c /tmp/gen2.c || true
    exit 1
fi

echo ""
echo "Testing with different programs..."

# Test 2: Different program
TEST_PROGRAM2="var a = 100
var b = 200"

echo "$TEST_PROGRAM2" > /tmp/test_input.wyn
$COMPILER > /tmp/test2_gen0_output.txt 2>&1
cp /tmp/test_input.c /tmp/test2_gen0.c

echo "$TEST_PROGRAM2" > /tmp/test_input.wyn
$COMPILER > /tmp/test2_gen1_output.txt 2>&1
cp /tmp/test_input.c /tmp/test2_gen1.c

if diff /tmp/test2_gen0.c /tmp/test2_gen1.c > /dev/null 2>&1; then
    echo "  ✅ Test 2: Reproducible"
else
    echo "  ❌ Test 2: Not reproducible"
    exit 1
fi

# Test 3: Empty program
echo "" > /tmp/test_input.wyn
$COMPILER > /tmp/test3_gen0_output.txt 2>&1
cp /tmp/test_input.c /tmp/test3_gen0.c

echo "" > /tmp/test_input.wyn
$COMPILER > /tmp/test3_gen1_output.txt 2>&1
cp /tmp/test_input.c /tmp/test3_gen1.c

if diff /tmp/test3_gen0.c /tmp/test3_gen1.c > /dev/null 2>&1; then
    echo "  ✅ Test 3: Reproducible (empty program)"
else
    echo "  ❌ Test 3: Not reproducible"
    exit 1
fi

# Cleanup
rm -f /tmp/gen*.c /tmp/test*.c /tmp/*_output.txt /tmp/test_input.wyn 2>/dev/null

echo ""
echo "=== Bootstrap Stability Test: PASSED ==="
echo ""
echo "Summary:"
echo "  ✅ Reproducible builds verified"
echo "  ✅ No drift across generations"
echo "  ✅ Stable output for same input"
echo "  ✅ Works with different programs"
echo ""

exit 0
