#!/bin/bash
# THE MOST-READ FILE IN THE REPOSITORY HAD NO GATE.
#
# `examples/` is swept and the book's snippets are checked, but README.md was only
# ever touched by `sed` for the version string (scripts/update-version.sh,
# .github/workflows/release.yml). So the second code sample a visitor reads - the
# "Features" block, the project's pitch - had two independent defects at once:
#
#   1. `result = 5 |> double` used the pipe operator, which the compiler REMOVED.
#      Its own diagnostic says so: "the pipe operator '|>' has been removed".
#   2. `print(Shape.Circle.to_string())` where `Circle(float)` carries a payload.
#      `Shape.Circle` without an argument is a constructor FUNCTION, not a value, so
#      the generated C passed a function designator where a struct was wanted and
#      clang refused it.
#
# THE SECOND ONE IS WHY THIS GATE BUILDS AND RUNS RATHER THAN CHECKING. `wyn check`
# passed that block. It does not reach codegen, so it cannot see a codegen-to-C
# failure - the same blind spot that let a cleanup-attribute change ship past twelve
# green checks. A check-only README gate would have reported the pitch as fine.
#
# Each ```wyn block must be a SELF-CONTAINED program: it is extracted verbatim,
# built, and run, and it must exit 0. That is a deliberate constraint on the README
# rather than an inconvenience - a block a reader cannot paste and run is a block
# that will be wrong eventually, which is how both defects above arrived.
#
# A FLOOR, because a gate that tests nothing must not pass. If the fence language is
# renamed or the blocks are moved, the discovered count drops and this fails loudly
# instead of reporting success over zero programs. suite.sh called a zero-test run
# green for months; that is not repeated here.
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
README="${README:-README.md}"
FLOOR="${README_BLOCK_FLOOR:-2}"

TMP=$(mktemp -d)
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

PASS=0; FAIL=0
ok(){   echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){  echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

echo "=== README code blocks must build and run ==="

if [ ! -f "$README" ]; then
    echo "  FAIL  $README not found (run from the repository root)"
    exit 1
fi

# Extraction is deliberately in python3 rather than awk/sed: the fence language tag
# must be matched exactly ("```wyn", not "```wynx"), and the line number of each
# block is reported so a failure names a place in the README.
python3 - "$README" "$TMP" <<'PY'
import re, sys
src, out = sys.argv[1], sys.argv[2]
lines = open(src, encoding='utf-8').read().split("\n")
n = 0
i = 0
while i < len(lines):
    if lines[i].strip() == "```wyn":
        start = i + 1
        body = []
        i += 1
        while i < len(lines) and lines[i].strip() != "```":
            body.append(lines[i])
            i += 1
        n += 1
        with open("%s/block_%02d.wyn" % (out, n), "w", encoding='utf-8') as f:
            f.write("\n".join(body) + "\n")
        with open("%s/block_%02d.line" % (out, n), "w") as f:
            f.write(str(start))
    i += 1
PY

COUNT=$(ls "$TMP"/block_*.wyn 2>/dev/null | wc -l | tr -d ' ')
if [ "$COUNT" -lt "$FLOOR" ]; then
    bad "discovered $COUNT wyn block(s) in $README, floor is $FLOOR - the gate would be vacuous"
    echo ""
    echo "readme: $PASS pass, $FAIL fail"
    exit 1
fi
echo "  discovered $COUNT wyn block(s) (floor: $FLOOR)"

for src in "$TMP"/block_*.wyn; do
    base=$(basename "$src" .wyn)
    line=$(cat "${src%.wyn}.line" 2>/dev/null || echo '?')
    bin="$TMP/$base.out"
    if ! TMPDIR="$TMP" perl -e 'alarm(120); exec @ARGV' -- "$WYN" build "$src" -o "$bin" > "$TMP/$base.build" 2>&1; then
        bad "$README:$line does not build"
        # The C diagnostic is the useful half; wyn echoes it before its own banner.
        grep -E 'error|Error' "$TMP/$base.build" | head -4 | sed 's/^/          /'
        continue
    fi
    if ! TMPDIR="$TMP" perl -e 'alarm(60); exec @ARGV' -- "$bin" > "$TMP/$base.run" 2>&1; then
        bad "$README:$line built but exited nonzero"
        head -4 "$TMP/$base.run" | sed 's/^/          /'
        continue
    fi
    ok "$README:$line builds and runs"
done

echo ""
echo "readme: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
