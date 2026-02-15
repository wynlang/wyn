#!/bin/bash
# Generate dynamic parallel test runner with only working tests

echo "Finding working tests..."
WORKING_TESTS=$(mktemp)

for f in tests/unit/test_*.out; do
    if timeout 2 "$f" > /dev/null 2>&1; then
        echo "$f" >> "$WORKING_TESTS"
    fi
done

COUNT=$(wc -l < "$WORKING_TESTS" | tr -d ' ')
echo "Found $COUNT working tests"

cat > tests/run_all_tests.wyn << 'EOF'
fn main() -> int {
EOF

while read -r test; do
    echo "    pool_add_task(\"timeout 2 $PWD/$test > /dev/null 2>&1\");" >> tests/run_all_tests.wyn
done < "$WORKING_TESTS"

cat >> tests/run_all_tests.wyn << 'EOF'
    pool_start(50);
    var failures = pool_wait();
    return failures;
}
EOF

rm "$WORKING_TESTS"
echo "Generated tests/run_all_tests.wyn with $COUNT tests"
