#!/bin/bash
# Test integrated compiler with file I/O

set -e

echo "=== Testing Integrated Compiler with File I/O ==="

# Create test input file
cat > /tmp/test_input.wyn << 'EOF'
var x = 42
EOF

# Compile and run the modular compiler
echo "Running compiler on test input..."
cd "$(dirname "$0")/../.."
./wyn/wyn run wyn/lib/compiler_modular.wyn

# Check if output file was created
if [ ! -f /tmp/test_input.c ]; then
    echo "FAIL: Output file not created"
    exit 1
fi

echo "Output file created successfully"

# Verify output contains C code
if ! grep -q "#include" /tmp/test_input.c; then
    echo "FAIL: Output doesn't contain C includes"
    exit 1
fi

if ! grep -q "int x = 42" /tmp/test_input.c; then
    echo "FAIL: Output doesn't contain expected C code"
    exit 1
fi

echo "Output contains valid C code"

# Try to compile the generated C code
echo "Compiling generated C code..."
gcc /tmp/test_input.c -o /tmp/test_input.out

echo "Generated C code compiles successfully"

# Run the generated executable
/tmp/test_input.out
echo "Generated executable runs successfully (exit code: $?)"

# Cleanup
rm -f /tmp/test_input.wyn /tmp/test_input.c /tmp/test_input.out

echo "=== All tests passed ==="
