#!/bin/bash
# Test self-hosting compiler

echo "=== Testing Self-Hosting Compiler ==="

cd ..

# Test 1: Compile simple program
echo "Test 1: Simple function"
echo 'fn add(a: int, b: int) -> int { return a + b }' > /tmp/test_add.wyn
./lib/compiler_modular.wyn.out < /tmp/test_add.wyn > /tmp/test_add_result.txt 2>&1
if [ $? -eq 0 ]; then
    echo "✅ Compiled simple function"
else
    echo "❌ Failed to compile simple function"
    cat /tmp/test_add_result.txt
    exit 1
fi

# Test 2: Compile lexer module with itself
echo ""
echo "Test 2: Self-compilation of lexer"
./lib/compiler_modular.wyn.out < lib/lexer_module.wyn > /tmp/lexer_self.txt 2>&1
if [ $? -eq 0 ]; then
    echo "✅ Self-compiled lexer module"
else
    echo "❌ Failed to self-compile lexer"
    cat /tmp/lexer_self.txt
    exit 1
fi

echo ""
echo "=== All self-hosting tests passed ==="
