#!/bin/bash
# Fuzz tester for Wyn lexer/parser
# Feeds random/malformed inputs to wyn check and verifies no crashes
# Default: 1000 rounds (industry standard for CI fuzz pass)
# Extended: 10000 rounds for pre-release validation
set -uo pipefail
WYN=${1:-./wyn}
ROUNDS=${2:-1000}
passed=0; crashed=0; hung=0

echo "Fuzzing Wyn parser ($ROUNDS rounds)..."

patterns=(
    'fn {{{{{{'
    'var x = "unterminated'
    'struct { } enum { } fn()()()'
    'if if if while for for match match'
    'var x = [[[[[[[[[['
    'fn a() -> int { return return return }'
    'import import import ""'
    'fn main() -> int { var x = 999999999999999999999999999999; return 0 }'
    'fn f() { while true { while true { while true { } } } }'
    'fn f<T, U, V>(a: T, b: U) -> V { return a }'
    'match x { 1 => 2, 3 => 4, _ => { } }'
    'struct A { x: int } struct B { a: A } struct C { b: B }'
    'spawn spawn spawn fn() {}'
    'async fn f() -> int { return await await 1 }'
    'var x = {"a": {"b": {"c": {"d": 1}}}}'
    'fn f(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int) -> int { return 0 }'
    '// comment only file'
    'var s = "${${${x}}}"'
    'for i in 0..0 { for j in 0..0 { for k in 0..0 { } } }'
    'enum E { A, B(int), C(string, int) } match E.A { }'
)

for i in $(seq 1 $ROUNDS); do
    idx=$((RANDOM % ${#patterns[@]}))
    src="${patterns[$idx]}"
    # Mix in random bytes every 5th round
    if [ $((i % 5)) -eq 0 ]; then
        src=$(head -c $((RANDOM % 200 + 10)) /dev/urandom | base64 | head -c $((RANDOM % 100 + 10)))
    fi
    # Mix in emoji/unicode every 7th round
    if [ $((i % 7)) -eq 0 ]; then
        src="fn 🐉() { var 🔥 = \"wynter\" }"
    fi
    echo "$src" > /tmp/__wyn_fuzz.wyn
    timeout 3 $WYN check /tmp/__wyn_fuzz.wyn > /dev/null 2>&1
    code=$?
    if [ $code -eq 124 ]; then
        echo "HANG on round $i: $src"
        hung=$((hung + 1))
    elif [ $code -ge 128 ]; then
        echo "CRASH (exit $code) on round $i: $src"
        crashed=$((crashed + 1))
    else
        passed=$((passed + 1))
    fi
done

rm -f /tmp/__wyn_fuzz.wyn
echo ""
echo "Results: $passed passed, $crashed crashed, $hung hung out of $ROUNDS"
if [ $crashed -eq 0 ] && [ $hung -eq 0 ]; then
    echo "✓ No crashes or hangs found"
    exit 0
else
    echo "✗ $crashed crashes, $hung hangs found"
    exit 1
fi
