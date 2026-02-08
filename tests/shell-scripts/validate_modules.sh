#!/bin/bash
# Comprehensive module system validation

echo "=== Module System Validation ==="
echo

cd /Users/aoaws/src/ao/wyn-lang/wyn

# 1. Nested modules
echo "1. Testing nested modules..."
./wyn examples/07_modules/nested_example.wyn 2>&1 | grep -q "Compiled" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 2. Visibility control - should error
echo "2. Testing visibility (should error)..."
./wyn tests/module_tests/test_visibility.wyn 2>&1 | grep -q "is private" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 3. Visibility - public access should work
echo "3. Testing public access..."
./wyn tests/module_tests/test_public_access.wyn 2>&1 | grep -q "Compiled" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 4. root:: imports (nested path)
echo "4. Testing root:: imports (nested)..."
./wyn tests/module_tests/test_relative_imports.wyn 2>&1 | grep -q "Compiled" && \
./tests/module_tests/test_relative_imports.wyn.out 2>&1 | grep -q "PASS" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 5. root:: imports
echo "5. Testing root:: imports..."
./wyn tests/module_tests/test_root_keyword.wyn 2>&1 | grep -q "Compiled" && \
./tests/module_tests/test_root_keyword.wyn.out 2>&1 | grep -q "root:: works" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 6. self:: imports - root level
echo "6. Testing self:: (root level)..."
./wyn tests/module_tests/test_self_relative.wyn 2>&1 | grep -q "Compiled" && \
./tests/module_tests/test_self_relative.wyn.out 2>&1 | grep -q "99" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 7. self:: imports - nested
echo "7. Testing self:: (nested)..."
./wyn tests/module_tests/test_self_nested.wyn 2>&1 | grep -q "Compiled" && \
./tests/module_tests/test_self_nested.wyn.out 2>&1 | grep -q "777" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 8. Collision detection - should error on short name
echo "8. Testing collision detection (short name)..."
./wyn tests/module_tests/test_collision.wyn 2>&1 | grep -q "Ambiguous" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 9. Collision bypass - full path should work
echo "9. Testing collision bypass (full path)..."
./wyn tests/module_tests/test_collision_bypass.wyn 2>&1 | grep -q "Compiled" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 10. Internal calls - private functions calling each other
echo "10. Testing internal calls..."
./wyn tests/module_tests/test_internal_calls_main.wyn 2>&1 | grep -q "Compiled" && \
./tests/module_tests/test_internal_calls_main.wyn.out 2>&1 | grep -q "42" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 11. Circular imports - should detect
echo "11. Testing circular import detection..."
timeout 5 ./wyn tests/module_tests/test_circular.wyn 2>&1 | grep -q "Circular import" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 12. Mixed root:: and self:: imports
echo "12. Testing mixed root:: and self::..."
./wyn tests/module_tests/test_mixed_imports.wyn 2>&1 | grep -q "Compiled" && \
./tests/module_tests/test_mixed_imports.wyn.out 2>&1 | grep -q "778" && echo "   ✅ PASS" || echo "   ❌ FAIL"

# 13. Duplicate imports (should be allowed)
echo "13. Testing duplicate imports..."
./wyn tests/module_tests/test_duplicate_import.wyn 2>&1 | grep -q "Compiled" && \
./tests/module_tests/test_duplicate_import.wyn.out 2>&1 | grep -q "1" && echo "   ✅ PASS" || echo "   ❌ FAIL"

echo
echo "=== Summary ==="
echo "13/13 features tested"
