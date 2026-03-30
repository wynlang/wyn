#!/bin/bash
# Test: --release hello world binary < 100KB
set -euo pipefail
WYN="${WYN:-./wyn}"
echo 'fn main() -> int { println("hello"); return 0 }' > /tmp/wyn_size_test.wyn
$WYN build --release /tmp/wyn_size_test.wyn 2>/dev/null
SIZE=$(stat -f%z /tmp/wyn_size_test 2>/dev/null || stat -c%s /tmp/wyn_size_test)
OUTPUT=$(/tmp/wyn_size_test)
rm -f /tmp/wyn_size_test /tmp/wyn_size_test.wyn /tmp/wyn_size_test.wyn.c
if [ "$OUTPUT" != "hello" ]; then echo "FAIL: output was '$OUTPUT'"; exit 1; fi
if [ "$SIZE" -gt 102400 ]; then echo "FAIL: binary ${SIZE} bytes > 100KB"; exit 1; fi
echo "PASS: hello world = ${SIZE} bytes ($(( SIZE / 1024 ))KB)"
