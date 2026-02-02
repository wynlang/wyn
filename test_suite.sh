#!/bin/bash
# Comprehensive test runner for Wyn v1.6.0

PASS=0
FAIL=0
SKIP=0

echo "=== Wyn v1.6.0 Test Suite ==="
echo ""

# Test Epic 1: Type System
echo "Epic 1: Type System Foundation"
for test in tests/unit/test_0{1..10}_*.wyn tests/validation/epic1*.wyn; do
  [ ! -f "$test" ] && continue
  name=$(basename "$test" .wyn)
  if ./wyn-llvm "$test" 2>&1 | grep -q "Compiled successfully"; then
    out="${test%.wyn}.out"
    if [ -f "$out" ]; then
      $out >/dev/null 2>&1
      exit_code=$?
      if [ $exit_code -eq 0 ]; then
        echo "  ✓ $name"
        ((PASS++))
      else
        echo "  ✗ $name (exit $exit_code)"
        ((FAIL++))
      fi
    else
      echo "  ⊘ $name (no binary)"
      ((SKIP++))
    fi
  else
    echo "  ✗ $name (compile error)"
    ((FAIL++))
  fi
done

# Test Epic 2: Everything is an Object
echo ""
echo "Epic 2: Everything is an Object"
for test in tests/objects/test_*.wyn; do
  [ ! -f "$test" ] && continue
  name=$(basename "$test" .wyn)
  if ./wyn-llvm "$test" 2>&1 | grep -q "Compiled successfully"; then
    out="${test%.wyn}.out"
    if [ -f "$out" ]; then
      $out >/dev/null 2>&1
      exit_code=$?
      if [ $exit_code -eq 0 ]; then
        echo "  ✓ $name"
        ((PASS++))
      else
        echo "  ✗ $name (exit $exit_code)"
        ((FAIL++))
      fi
    else
      echo "  ⊘ $name (no binary)"
      ((SKIP++))
    fi
  else
    echo "  ✗ $name (compile error)"
    ((FAIL++))
  fi
done

# Test Epic 3: Pattern Matching
echo ""
echo "Epic 3: Pattern Matching"
for test in tests/patterns/test_*.wyn; do
  [ ! -f "$test" ] && continue
  name=$(basename "$test" .wyn)
  if ./wyn-llvm "$test" 2>&1 | grep -q "Compiled successfully"; then
    out="${test%.wyn}.out"
    if [ -f "$out" ]; then
      $out >/dev/null 2>&1
      exit_code=$?
      if [ $exit_code -eq 0 ]; then
        echo "  ✓ $name"
        ((PASS++))
      else
        echo "  ✗ $name (exit $exit_code)"
        ((FAIL++))
      fi
    else
      echo "  ⊘ $name (no binary)"
      ((SKIP++))
    fi
  else
    echo "  ✗ $name (compile error)"
    ((FAIL++))
  fi
done

echo ""
echo "=== Summary ==="
echo "Pass: $PASS"
echo "Fail: $FAIL"
echo "Skip: $SKIP"
echo "Total: $((PASS + FAIL + SKIP))"

if [ $FAIL -eq 0 ]; then
  echo "✓ All tests passed!"
  exit 0
else
  echo "✗ Some tests failed"
  exit 1
fi
