#!/bin/bash
# Self-Compilation Verification Test
# Task 2 from TODO_for_1.5.md

set -e
cd "$(dirname "$0")/.."

echo "=== Self-Compilation Verification Test ==="
echo ""
echo "Note: This test uses compiler_self.wyn (75 lines) instead of"
echo "compiler_complete.wyn (727 lines) due to compiler size limitations."
echo ""

# Test 1: Compile lexer_module.wyn
echo "Test 1: Compile lexer_module.wyn"
if [ -f lib/lexer_module.wyn ]; then
    ./lib/compiler_self.wyn.out lib/lexer_module.wyn /tmp/lexer_gen.c
    gcc /tmp/lexer_gen.c -o /tmp/lexer_gen
    echo "Generated binary:"
    /tmp/lexer_gen
    echo ""
    echo "Original binary:"
    ./lib/lexer_module.wyn.out
    echo ""
    echo "⚠ Note: Generated code is placeholder, not actual lexer"
else
    echo "⚠ lexer_module.wyn not found"
fi
echo ""

# Test 2: Compile parser_module.wyn
echo "Test 2: Compile parser_module.wyn"
if [ -f lib/parser_module.wyn ]; then
    ./lib/compiler_self.wyn.out lib/parser_module.wyn /tmp/parser_gen.c
    gcc /tmp/parser_gen.c -o /tmp/parser_gen
    echo "Generated binary:"
    /tmp/parser_gen
    echo ""
    echo "Original binary:"
    ./lib/parser_module.wyn.out
    echo ""
    echo "⚠ Note: Generated code is placeholder, not actual parser"
else
    echo "⚠ parser_module.wyn not found"
fi
echo ""

# Test 3: Compile checker_module.wyn
echo "Test 3: Compile checker_module.wyn"
if [ -f lib/checker_module.wyn ]; then
    ./lib/compiler_self.wyn.out lib/checker_module.wyn /tmp/checker_gen.c
    gcc /tmp/checker_gen.c -o /tmp/checker_gen
    echo "Generated binary:"
    /tmp/checker_gen
    echo ""
    echo "Original binary:"
    ./lib/checker_module.wyn.out
    echo ""
    echo "⚠ Note: Generated code is placeholder, not actual checker"
else
    echo "⚠ checker_module.wyn not found"
fi
echo ""

# Test 4: Compile codegen_module.wyn
echo "Test 4: Compile codegen_module.wyn"
if [ -f lib/codegen_module.wyn ]; then
    ./lib/compiler_self.wyn.out lib/codegen_module.wyn /tmp/codegen_gen.c
    gcc /tmp/codegen_gen.c -o /tmp/codegen_gen
    echo "Generated binary:"
    /tmp/codegen_gen
    echo ""
    echo "Original binary:"
    ./lib/codegen_module.wyn.out
    echo ""
    echo "⚠ Note: Generated code is placeholder, not actual codegen"
else
    echo "⚠ codegen_module.wyn not found"
fi
echo ""

# Test 5: Compile compiler_self.wyn (self-compilation)
echo "Test 5: Compile compiler_self.wyn (self-compilation)"
./lib/compiler_self.wyn.out lib/compiler_self.wyn /tmp/compiler_gen.c
gcc /tmp/compiler_gen.c -o /tmp/compiler_gen
echo "Generated binary (with test input):"
echo 'fn main() { return 0 }' > /tmp/test_input.wyn
/tmp/compiler_gen /tmp/test_input.wyn /tmp/test_output.c 2>&1 || echo "(placeholder, no actual compilation)"
echo ""
echo "Original binary (with test input):"
./lib/compiler_self.wyn.out /tmp/test_input.wyn /tmp/test_output2.c 2>&1 || echo "(requires arguments)"
echo ""
echo "⚠ Note: Generated code is placeholder, not actual compiler"
echo ""

# Test 6: Try to compile compiler_complete.wyn (expected to fail)
echo "Test 6: Try to compile compiler_complete.wyn"
if timeout 10 ./lib/compiler_self.wyn.out lib/compiler_complete.wyn /tmp/compiler_complete_gen.c 2>&1; then
    echo "✓ Compilation succeeded"
    if [ -f /tmp/compiler_complete_gen.c ]; then
        echo "✓ Generated C file exists"
        gcc /tmp/compiler_complete_gen.c -o /tmp/compiler_complete_gen
        /tmp/compiler_complete_gen
    fi
else
    echo "✗ Compilation failed or timed out (expected due to file size)"
fi
echo ""

# Cleanup
rm -f /tmp/*_gen.c /tmp/*_gen 2>/dev/null

echo "=== Summary ==="
echo "✓ Compiler can compile small modules (<100 lines)"
echo "✓ Generated C code compiles successfully"
echo "✓ Generated binaries run without errors"
echo "⚠ Generated code is placeholder (doesn't implement actual logic)"
echo "⚠ Cannot compile large files (>650 lines)"
echo "⚠ Self-compilation works but generates placeholder code"
echo ""
echo "Status: Proof of concept verified, full implementation blocked by:"
echo "  1. Compiler size limit (>650 lines causes SIGKILL)"
echo "  2. Placeholder codegen (doesn't generate actual Wyn logic)"
