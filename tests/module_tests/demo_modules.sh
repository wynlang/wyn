#!/bin/bash
# Demo: Complete Module System Example

cd "$(dirname "$0")"

echo "=== Wyn v1.1.0 Module System Demo ==="
echo ""

# Create a realistic project structure
mkdir -p demo_project/modules

# Create a geometry module
cat > demo_project/modules/geometry.wyn << 'EOF'
pub struct Point {
    x: int,
    y: int
}

pub struct Rectangle {
    width: int,
    height: int
}

pub fn point_distance(p: Point) -> int {
    return p.x + p.y
}

pub fn rect_area(r: Rectangle) -> int {
    return r.width * r.height
}

pub fn rect_perimeter(r: Rectangle) -> int {
    return (r.width + r.height) * 2
}
EOF

# Create a calculator module that uses math
cat > demo_project/modules/calculator.wyn << 'EOF'
import math

pub fn sum_of_squares(a: int, b: int) -> int {
    var a_sq = math.multiply(a, a)
    var b_sq = math.multiply(b, b)
    return math.add(a_sq, b_sq)
}

pub fn average(a: int, b: int) -> int {
    var sum = math.add(a, b)
    return sum / 2
}
EOF

# Create main application
cat > demo_project/main.wyn << 'EOF'
import math
import geometry
import calculator

fn main() -> int {
    // Use built-in math module
    var sum = math.add(10, 20)
    print("math.add(10, 20) = ")
    print(sum)
    print("\n")
    
    // Use geometry module
    var p = geometry.Point { x: 3, y: 4 }
    var dist = geometry.point_distance(p)
    print("point_distance(3, 4) = ")
    print(dist)
    print("\n")
    
    var rect = geometry.Rectangle { width: 5, height: 10 }
    var area = geometry.rect_area(rect)
    print("rect_area(5, 10) = ")
    print(area)
    print("\n")
    
    // Use calculator module (which uses math)
    var squares = calculator.sum_of_squares(3, 4)
    print("sum_of_squares(3, 4) = ")
    print(squares)
    print("\n")
    
    // Return sum
    return math.add(sum, dist)
}
EOF

echo "Project structure:"
echo "demo_project/"
echo "├── main.wyn"
echo "└── modules/"
echo "    ├── geometry.wyn"
echo "    └── calculator.wyn"
echo ""

echo "Compiling..."
./wyn demo_project/main.wyn >/dev/null 2>&1

if [ -f demo_project/main.wyn.out ]; then
    echo "✓ Compilation successful"
    echo ""
    echo "Running..."
    demo_project/main.wyn.out 2>/dev/null
    result=$?
    echo ""
    echo "Exit code: $result"
    
    # sum=30, dist=7, result = 30 + 7 = 37
    if [ $result -eq 37 ]; then
        echo "✓ Correct result!"
    else
        echo "Note: Result is $result"
    fi
else
    echo "✗ Compilation failed"
    ./wyn demo_project/main.wyn 2>&1 | grep "error:" | head -5
fi

echo ""
echo "=== Demo Complete ==="
