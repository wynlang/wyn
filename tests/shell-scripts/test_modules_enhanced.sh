#!/bin/bash
# Enhanced modules system validation with proper test isolation

WYN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export WYN_ROOT
WYN_BIN="$WYN_ROOT/wyn"

echo "=== Enhanced Modules System Validation ==="
echo ""

PASS=0
FAIL=0

run_test() {
    local test_name="$1"
    local test_dir="$2"
    
    cd /tmp
    rm -rf "$test_dir"
    mkdir -p "$test_dir"
    cd "$test_dir"
}

# Test 1: Basic import
run_test "Basic Import" "mod_test1"
cat > utils.wyn << 'EOF'
pub fn helper() -> int { return 42 }
EOF
cat > main.wyn << 'EOF'
import root::utils
fn main() -> int {
    if utils::helper() == 42 {
        print("PASS")
        return 0
    }
    return 1
}
EOF
echo "Test 1: Basic Import"
if $WYN_BIN run main.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Pass"
    PASS=$((PASS + 1))
else
    echo "  ❌ Fail"
    FAIL=$((FAIL + 1))
fi

# Test 2: Multiple imports
run_test "Multiple Imports" "mod_test2"
cat > math.wyn << 'EOF'
pub fn add(a: int, b: int) -> int { return a + b }
EOF
cat > string.wyn << 'EOF'
pub fn greet() -> string { return "Hello" }
EOF
cat > main.wyn << 'EOF'
import root::math
import root::string
fn main() -> int {
    if math::add(5, 3) == 8 {
        print("PASS")
        return 0
    }
    return 1
}
EOF
echo "Test 2: Multiple Imports"
if $WYN_BIN run main.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Pass"
    PASS=$((PASS + 1))
else
    echo "  ❌ Fail"
    FAIL=$((FAIL + 1))
fi

# Test 3: Transitive imports
run_test "Transitive Imports" "mod_test3"
cat > base.wyn << 'EOF'
pub fn base_val() -> int { return 5 }
EOF
cat > middle.wyn << 'EOF'
import root::base
pub fn middle_val() -> int { return base::base_val() * 2 }
EOF
cat > main.wyn << 'EOF'
import root::middle
fn main() -> int {
    if middle::middle_val() == 10 {
        print("PASS")
        return 0
    }
    return 1
}
EOF
echo "Test 3: Transitive Imports"
if $WYN_BIN run main.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Pass"
    PASS=$((PASS + 1))
else
    echo "  ❌ Fail"
    FAIL=$((FAIL + 1))
fi

# Test 4: Private functions
run_test "Private Functions" "mod_test4"
cat > private_mod.wyn << 'EOF'
pub fn public_func() -> int { return 1 }
fn private_func() -> int { return 2 }
EOF
cat > main.wyn << 'EOF'
import root::private_mod
fn main() -> int {
    var x = private_mod::private_func()
    return 0
}
EOF
echo "Test 4: Private Function Protection"
if $WYN_BIN run main.wyn 2>&1 | grep -q "private"; then
    echo "  ✅ Pass"
    PASS=$((PASS + 1))
else
    echo "  ❌ Fail"
    FAIL=$((FAIL + 1))
fi

# Test 5: Circular imports
run_test "Circular Imports" "mod_test5"
cat > circ_a.wyn << 'EOF'
import root::circ_b
pub fn func_a() -> int { return circ_b::func_b() }
EOF
cat > circ_b.wyn << 'EOF'
import root::circ_a
pub fn func_b() -> int { return circ_a::func_a() }
EOF
cat > main.wyn << 'EOF'
import root::circ_a
fn main() -> int { return 0 }
EOF
echo "Test 5: Circular Import Detection"
if $WYN_BIN run main.wyn 2>&1 | grep -q "Circular"; then
    echo "  ✅ Pass"
    PASS=$((PASS + 1))
else
    echo "  ❌ Fail"
    FAIL=$((FAIL + 1))
fi

# Test 6: Multiple functions per module
run_test "Multiple Functions" "mod_test6"
cat > ops.wyn << 'EOF'
pub fn add(a: int, b: int) -> int { return a + b }
pub fn sub(a: int, b: int) -> int { return a - b }
pub fn mul(a: int, b: int) -> int { return a * b }
EOF
cat > main.wyn << 'EOF'
import root::ops
fn main() -> int {
    if ops::add(5, 3) == 8 {
        if ops::sub(5, 3) == 2 {
            if ops::mul(5, 3) == 15 {
                print("PASS")
                return 0
            }
        }
    }
    return 1
}
EOF
echo "Test 6: Multiple Functions Per Module"
if $WYN_BIN run main.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Pass"
    PASS=$((PASS + 1))
else
    echo "  ❌ Fail"
    FAIL=$((FAIL + 1))
fi

# Test 7: Multi-layer application
run_test "Multi-Layer App" "mod_test7"
cat > data.wyn << 'EOF'
pub fn get_data() -> int { return 42 }
EOF
cat > logic.wyn << 'EOF'
import root::data
pub fn process() -> int { return data::get_data() * 2 }
EOF
cat > api.wyn << 'EOF'
import root::logic
pub fn handle() -> int { return logic::process() }
EOF
cat > main.wyn << 'EOF'
import root::api
fn main() -> int {
    if api::handle() == 84 {
        print("PASS")
        return 0
    }
    return 1
}
EOF
echo "Test 7: Multi-Layer Application"
if $WYN_BIN run main.wyn 2>&1 | grep -q "PASS"; then
    echo "  ✅ Pass"
    PASS=$((PASS + 1))
else
    echo "  ❌ Fail"
    FAIL=$((FAIL + 1))
fi

# Test 8: Compile mode
run_test "Compile Mode" "mod_test8"
cat > utils.wyn << 'EOF'
pub fn value() -> int { return 99 }
EOF
cat > main.wyn << 'EOF'
import root::utils
fn main() -> int {
    if utils::value() == 99 {
        print("PASS")
        return 0
    }
    return 1
}
EOF
echo "Test 8: Compile Mode"
if $WYN_BIN main.wyn 2>&1 | grep -q "Compiled successfully"; then
    if ./main.wyn.out 2>&1 | grep -q "PASS"; then
        echo "  ✅ Pass"
        PASS=$((PASS + 1))
    else
        echo "  ❌ Fail (execution)"
        FAIL=$((FAIL + 1))
    fi
else
    echo "  ❌ Fail (compilation)"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "==================================="
echo "Results: $PASS passed, $FAIL failed"
echo "==================================="
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✅ All enhanced modules tests passed!"
    echo ""
    echo "Features Validated:"
    echo "  ✅ Basic imports"
    echo "  ✅ Multiple imports"
    echo "  ✅ Transitive imports"
    echo "  ✅ Private function protection"
    echo "  ✅ Circular import detection"
    echo "  ✅ Multiple functions per module"
    echo "  ✅ Multi-layer applications"
    echo "  ✅ Compile mode"
    echo ""
    echo "Modules System: PRODUCTION READY ✅"
    exit 0
else
    echo "❌ $FAIL tests failed"
    exit 1
fi
