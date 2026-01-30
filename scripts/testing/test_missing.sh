#!/bin/bash
# Comprehensive feature test

cd "$(dirname "$0")"
WYN="./wyn"
PASS=0
FAIL=0

test_feature() {
    local name="$1"
    local code="$2"
    local expected="$3"
    
    echo -n "Testing $name... "
    echo "$code" > /tmp/test_$$.wyn
    
    if ! $WYN /tmp/test_$$.wyn >/dev/null 2>&1; then
        echo "FAIL (compile error)"
        ((FAIL++))
        return
    fi
    
    /tmp/test_$$.out >/dev/null 2>&1
    local result=$?
    
    if [ "$result" -eq "$expected" ]; then
        echo "PASS"
        ((PASS++))
    else
        echo "FAIL (expected $expected, got $result)"
        ((FAIL++))
    fi
    
    rm -f /tmp/test_$$.wyn /tmp/test_$$.out
}

echo "=== Critical Missing Features ==="

# EXPR_CHAR (already works)
test_feature "char_expr" \
"fn main() -> int { var c = 'A'; return c }" 65

# EXPR_STRUCT_INIT
test_feature "struct_init" \
"struct Point { x: int, y: int }
fn main() -> int { var p = Point { x: 10, y: 20 }; return 10 }" 10

# EXPR_FIELD_ACCESS  
test_feature "field_access" \
"struct Point { x: int, y: int }
fn main() -> int { var p = Point { x: 10, y: 20 }; return p.x }" 10

# EXPR_METHOD_CALL
test_feature "method_call" \
"struct Point { x: int, y: int }
impl Point { fn get_x(self) -> int { return self.x } }
fn main() -> int { var p = Point { x: 10, y: 20 }; return p.get_x() }" 10

# STMT_STRUCT
test_feature "struct_def" \
"struct Point { x: int, y: int }
fn main() -> int { return 0 }" 0

# STMT_ENUM
test_feature "enum_def" \
"enum Color { Red, Green, Blue }
fn main() -> int { return 0 }" 0

# STMT_MATCH
test_feature "match_expr" \
"fn main() -> int {
    var x = 5
    return match x {
        5 => 10
        _ => 0
    }
}" 10

echo
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
