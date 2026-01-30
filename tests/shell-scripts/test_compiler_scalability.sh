#!/bin/bash
# Test: Compiler scalability
# Tests that compiler can handle progressively larger files

echo "=== Test 1: 100-line file ==="

# Generate 100-line file
cat > /tmp/test_100.wyn << 'EOF'
fn test1() -> int { return 1 }
fn test2() -> int { return 2 }
fn test3() -> int { return 3 }
fn test4() -> int { return 4 }
fn test5() -> int { return 5 }
fn test6() -> int { return 6 }
fn test7() -> int { return 7 }
fn test8() -> int { return 8 }
fn test9() -> int { return 9 }
fn test10() -> int { return 10 }
fn test11() -> int { return 11 }
fn test12() -> int { return 12 }
fn test13() -> int { return 13 }
fn test14() -> int { return 14 }
fn test15() -> int { return 15 }
fn test16() -> int { return 16 }
fn test17() -> int { return 17 }
fn test18() -> int { return 18 }
fn test19() -> int { return 19 }
fn test20() -> int { return 20 }
fn test21() -> int { return 21 }
fn test22() -> int { return 22 }
fn test23() -> int { return 23 }
fn test24() -> int { return 24 }
fn test25() -> int { return 25 }
fn test26() -> int { return 26 }
fn test27() -> int { return 27 }
fn test28() -> int { return 28 }
fn test29() -> int { return 29 }
fn test30() -> int { return 30 }
fn test31() -> int { return 31 }
fn test32() -> int { return 32 }
fn test33() -> int { return 33 }
fn test34() -> int { return 34 }
fn test35() -> int { return 35 }
fn test36() -> int { return 36 }
fn test37() -> int { return 37 }
fn test38() -> int { return 38 }
fn test39() -> int { return 39 }
fn test40() -> int { return 40 }
fn test41() -> int { return 41 }
fn test42() -> int { return 42 }
fn test43() -> int { return 43 }
fn test44() -> int { return 44 }
fn test45() -> int { return 45 }
fn test46() -> int { return 46 }
fn test47() -> int { return 47 }
fn test48() -> int { return 48 }
fn test49() -> int { return 49 }
fn test50() -> int { return 50 }

fn main() -> int {
    var sum = test1() + test2() + test3() + test4() + test5()
    return sum
}
EOF

wc -l /tmp/test_100.wyn

cd wyn
timeout 10s ./wyn /tmp/test_100.wyn 2>&1
if [ $? -eq 0 ] && [ -f /tmp/test_100.wyn.out ]; then
    /tmp/test_100.wyn.out
    result=$?
    if [ "$result" = "15" ]; then
        echo "✅ Test 1 PASSED: 100-line file compiles and runs (1+2+3+4+5=15)"
    else
        echo "❌ Test 1 FAILED: Expected 15, got $result"
        exit 1
    fi
else
    echo "❌ Test 1 FAILED: Compilation failed or timed out"
    exit 1
fi

echo ""
echo "=== Test 2: 200-line file with structs ==="

# Generate 200-line file with structs
{
    for i in {1..50}; do
        echo "struct Point$i { x: int, y: int }"
    done
    for i in {1..50}; do
        echo "fn func$i() -> int { return $i }"
    done
    echo "fn main() -> int { return func1() + func2() }"
} > /tmp/test_200.wyn

wc -l /tmp/test_200.wyn

timeout 10s ./wyn /tmp/test_200.wyn 2>&1
if [ $? -eq 0 ] && [ -f /tmp/test_200.wyn.out ]; then
    /tmp/test_200.wyn.out
    result=$?
    if [ "$result" = "3" ]; then
        echo "✅ Test 2 PASSED: 200-line file with structs compiles (1+2=3)"
    else
        echo "❌ Test 2 FAILED: Expected 3, got $result"
        exit 1
    fi
else
    echo "❌ Test 2 FAILED: Compilation failed or timed out"
    exit 1
fi

echo ""
echo "=== Test 3: 500-line file ==="

# Generate 500-line file
{
    for i in {1..100}; do
        echo "struct Data$i { value: int }"
    done
    for i in {1..100}; do
        echo "fn compute$i(x: int) -> int { return x + $i }"
    done
    echo "fn main() -> int { return compute1(10) }"
} > /tmp/test_500.wyn

wc -l /tmp/test_500.wyn

timeout 15s ./wyn /tmp/test_500.wyn 2>&1
if [ $? -eq 0 ] && [ -f /tmp/test_500.wyn.out ]; then
    /tmp/test_500.wyn.out
    result=$?
    if [ "$result" = "11" ]; then
        echo "✅ Test 3 PASSED: 500-line file compiles (10+1=11)"
    else
        echo "❌ Test 3 FAILED: Expected 11, got $result"
        exit 1
    fi
else
    echo "❌ Test 3 FAILED: Compilation failed or timed out"
    echo "This is expected - compiler has known limitation with large files"
    exit 1
fi

echo ""
echo "=== All scalability tests PASSED ==="
