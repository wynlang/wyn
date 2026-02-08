#!/bin/bash

# Test HashMap functionality for v1.5.0

set -e

WYN=${WYN_ROOT}/wyn

echo "Testing HashMap with OO syntax..."

# Test 1: Basic HashMap operations
cat > /tmp/test_hashmap_basic.wyn << 'EOF'
fn main() -> int {
    var m = {}
    m.insert("name", 42)
    m.insert("age", 25)
    
    var name_val = m.get("name")
    var age_val = m.get("age")
    
    if name_val != 42 {
        print("FAIL: Expected name=42, got " + name_val.to_string())
        return 1
    }
    
    if age_val != 25 {
        print("FAIL: Expected age=25, got " + age_val.to_string())
        return 1
    }
    
    m.free()
    print("PASS: Basic HashMap operations")
    return 0
}
EOF

$WYN run /tmp/test_hashmap_basic.wyn || exit 1

# Test 2: HashMap length and contains
cat > /tmp/test_hashmap_len.wyn << 'EOF'
fn main() -> int {
    var m = {}
    
    if m.len() != 0 {
        print("FAIL: Empty map should have length 0")
        return 1
    }
    
    m.insert("key1", 100)
    m.insert("key2", 200)
    m.insert("key3", 300)
    
    if m.len() != 3 {
        print("FAIL: Map should have length 3")
        return 1
    }
    
    if m.contains("key1") != 1 {
        print("FAIL: Map should contain key1")
        return 1
    }
    
    if m.contains("nonexistent") != 0 {
        print("FAIL: Map should not contain nonexistent key")
        return 1
    }
    
    m.free()
    print("PASS: HashMap length and contains")
    return 0
}
EOF

$WYN run /tmp/test_hashmap_len.wyn || exit 1

# Test 3: HashMap with different values
cat > /tmp/test_hashmap_values.wyn << 'EOF'
fn main() -> int {
    var m = {}
    
    m.insert("zero", 0)
    m.insert("negative", -42)
    m.insert("large", 999999)
    
    if m.get("zero") != 0 {
        print("FAIL: Expected zero=0")
        return 1
    }
    
    if m.get("negative") != -42 {
        print("FAIL: Expected negative=-42")
        return 1
    }
    
    if m.get("large") != 999999 {
        print("FAIL: Expected large=999999")
        return 1
    }
    
    m.free()
    print("PASS: HashMap with different values")
    return 0
}
EOF

$WYN run /tmp/test_hashmap_values.wyn || exit 1

# Test 4: HashMap update existing key
cat > /tmp/test_hashmap_update.wyn << 'EOF'
fn main() -> int {
    var m = {}
    
    m.insert("counter", 1)
    if m.get("counter") != 1 {
        print("FAIL: Initial value should be 1")
        return 1
    }
    
    m.insert("counter", 2)
    if m.get("counter") != 2 {
        print("FAIL: Updated value should be 2")
        return 1
    }
    
    m.insert("counter", 100)
    if m.get("counter") != 100 {
        print("FAIL: Updated value should be 100")
        return 1
    }
    
    m.free()
    print("PASS: HashMap update existing key")
    return 0
}
EOF

$WYN run /tmp/test_hashmap_update.wyn || exit 1

# Test 5: Multiple HashMaps
cat > /tmp/test_multiple_hashmaps.wyn << 'EOF'
fn main() -> int {
    var m1 = {}
    var m2 = {}
    
    m1.insert("key", 111)
    m2.insert("key", 222)
    
    if m1.get("key") != 111 {
        print("FAIL: m1 should have value 111")
        return 1
    }
    
    if m2.get("key") != 222 {
        print("FAIL: m2 should have value 222")
        return 1
    }
    
    m1.free()
    m2.free()
    print("PASS: Multiple HashMaps")
    return 0
}
EOF

$WYN run /tmp/test_multiple_hashmaps.wyn || exit 1

# Test 6: HashMap with initialization
cat > /tmp/test_hashmap_init.wyn << 'EOF'
fn main() -> int {
    var m = {"name": 100, "age": 25}
    
    if m.get("name") != 100 {
        print("FAIL: Expected name=100")
        return 1
    }
    
    if m.get("age") != 25 {
        print("FAIL: Expected age=25")
        return 1
    }
    
    if m.len() != 2 {
        print("FAIL: Map should have length 2")
        return 1
    }
    
    m.free()
    print("PASS: HashMap with initialization")
    return 0
}
EOF

$WYN run /tmp/test_hashmap_init.wyn || exit 1

echo ""
echo "All HashMap tests passed!"
