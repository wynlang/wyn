#!/bin/bash
# Package manager validation tests

WYN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export WYN_ROOT
WYN_BIN="$WYN_ROOT/wyn"

echo "=== Wyn Package Manager Validation ==="
echo ""

# Clean up any previous test data
rm -rf ~/.wyn/packages/testpkg* ~/.wyn/registry.txt 2>/dev/null

cd /tmp

# Test 1: List packages (empty)
echo "Test 1: List Packages (Empty)"
if $WYN_BIN pkg list 2>&1 | grep -q "No packages"; then
    echo "  ✅ Empty package list works"
else
    echo "  ❌ Empty package list failed"
    exit 1
fi

# Test 2: Create a test package
echo "Test 2: Create Test Package"
mkdir -p /tmp/testpkg1
cat > /tmp/testpkg1/wyn.toml << 'EOF'
[project]
name = "testpkg1"
version = "1.0.0"
entry = "main.wyn"
EOF
cat > /tmp/testpkg1/main.wyn << 'EOF'
fn hello() {
    print("Hello from testpkg1")
}
EOF
echo "  ✅ Test package created"

# Test 3: Install from local path
echo "Test 3: Install from Local Path"
mkdir -p /tmp/test_project1
cat > /tmp/test_project1/wyn.toml << 'EOF'
[project]
name = "test_project1"
version = "1.0.0"

[dependencies]
testpkg1 = "/tmp/testpkg1"
EOF

cd /tmp/test_project1
if $WYN_BIN install 2>&1 | grep -q "installed successfully"; then
    echo "  ✅ Local package install works"
else
    echo "  ❌ Local package install failed"
    exit 1
fi

# Test 4: List installed packages
echo "Test 4: List Installed Packages"
if $WYN_BIN pkg list 2>&1 | grep -q "testpkg1"; then
    echo "  ✅ Package listing works"
else
    echo "  ❌ Package listing failed"
    exit 1
fi

# Test 5: Install already installed package (should skip)
echo "Test 5: Skip Already Installed"
cd /tmp/test_project1
if $WYN_BIN install 2>&1 | grep -q "already installed"; then
    echo "  ✅ Skip already installed works"
else
    echo "  ❌ Skip already installed failed"
    exit 1
fi

# Test 6: Dependency resolution
echo "Test 6: Dependency Resolution"
mkdir -p /tmp/testpkg2
cat > /tmp/testpkg2/wyn.toml << 'EOF'
[project]
name = "testpkg2"
version = "1.0.0"

[dependencies]
testpkg1 = "/tmp/testpkg1"
EOF
cat > /tmp/testpkg2/main.wyn << 'EOF'
fn world() {
    print("Hello from testpkg2")
}
EOF

mkdir -p /tmp/test_project2
cat > /tmp/test_project2/wyn.toml << 'EOF'
[project]
name = "test_project2"
version = "1.0.0"

[dependencies]
testpkg2 = "/tmp/testpkg2"
EOF

cd /tmp/test_project2
if $WYN_BIN install 2>&1 | grep -q "dependencies for testpkg2"; then
    echo "  ✅ Dependency resolution works"
else
    echo "  ❌ Dependency resolution failed"
    exit 1
fi

# Test 7: Package registry persistence
echo "Test 7: Registry Persistence"
if [ -f ~/.wyn/registry.txt ]; then
    if grep -q "testpkg" ~/.wyn/registry.txt; then
        echo "  ✅ Registry persistence works"
    else
        echo "  ❌ Registry persistence failed"
        exit 1
    fi
else
    echo "  ❌ Registry file not created"
    exit 1
fi

echo ""
echo "==================================="
echo "✅ All package manager tests passed!"
echo "==================================="
echo ""
echo "Summary:"
echo "  ✅ Package listing"
echo "  ✅ Local package install"
echo "  ✅ Already installed detection"
echo "  ✅ Dependency resolution"
echo "  ✅ Registry persistence"
echo ""
echo "Wyn Package Manager: FULLY FUNCTIONAL ✅"

# Cleanup
rm -rf /tmp/testpkg* /tmp/test_project*
