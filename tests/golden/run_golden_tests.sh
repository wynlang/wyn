#!/bin/bash
# Golden-C snapshot tests.
#
# Each tests/golden/<name>.wyn is compiled with `wyn build <file> --debug`,
# which emits the generated C next to the source as <name>.wyn.c. That C is
# normalized and diffed against the checked-in snapshot <name>.c.golden.
# Purpose: catch ANY unintended change to emitted C during codegen refactors
# (heuristic deletions, c_type consolidation, registry rework).
#
# Determinism findings (2026-07, macOS arm64):
#   - The emitted C is byte-for-byte identical across repeated builds of the
#     same source (verified by double-building all 30 programs and diffing).
#   - The ONLY environment-dependent content is the source file path embedded
#     in `#line N "<path>"` directives and `/* @<path>:N */` comments. The
#     path is echoed exactly as passed to `wyn build` (absolute if invoked
#     with an absolute path, relative otherwise).
#   - No timestamps, version strings, tmp-dir names, or hashed identifiers
#     appear in the output.
# Normalization therefore rewrites any path ending in <name>.wyn down to the
# bare basename (and strips CR for portability). Nothing else is touched:
# snapshots are whole-file.
#
# Usage:
#   WYN=./wyn bash tests/golden/run_golden_tests.sh            # verify
#   WYN=./wyn bash tests/golden/run_golden_tests.sh --update   # regenerate
#     Use --update ONLY for intentional codegen changes; review the .golden
#     diff before committing.
set -uo pipefail

WYN="${WYN:-./wyn}"
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

UPDATE=0
[ "${1:-}" = "--update" ] && UPDATE=1

PASS=0
FAIL=0
UPDATED=0

# Normalize generated C: collapse the embedded source path (absolute or
# relative) to its basename so snapshots are location-independent.
normalize() {
    local src="$1" base="$2"
    sed -e "s|[^\"[:space:]]*${base}\\.wyn|${base}.wyn|g" -e 's|\r$||' "$src"
}

for wyn_src in "$DIR"/*.wyn; do
    [ -f "$wyn_src" ] || continue
    base=$(basename "$wyn_src" .wyn)
    golden="$DIR/$base.c.golden"

    # Build in a temp copy so we never litter tests/golden/ with artifacts.
    work="$TMP/$base.wyn"
    cp "$wyn_src" "$work"
    if ! "$WYN" build "$work" --debug >/dev/null 2>&1 || [ ! -f "$work.c" ]; then
        echo "  FAIL  $base - wyn build failed"
        FAIL=$((FAIL + 1))
        continue
    fi
    rm -f "$TMP/$base"   # the compiled binary; only the .c matters

    normalize "$work.c" "$base" > "$TMP/$base.norm.c"

    if [ "$UPDATE" -eq 1 ]; then
        if [ ! -f "$golden" ] || ! cmp -s "$TMP/$base.norm.c" "$golden"; then
            cp "$TMP/$base.norm.c" "$golden"
            echo "  UPDATED  $base"
            UPDATED=$((UPDATED + 1))
        else
            echo "  ok       $base (unchanged)"
        fi
        continue
    fi

    if [ ! -f "$golden" ]; then
        echo "  FAIL  $base - missing snapshot $base.c.golden (run with --update)"
        FAIL=$((FAIL + 1))
        continue
    fi

    if cmp -s "$TMP/$base.norm.c" "$golden"; then
        echo "  ok    $base"
        PASS=$((PASS + 1))
    else
        echo "  FAIL  $base - generated C differs from snapshot:"
        diff -u "$golden" "$TMP/$base.norm.c" | head -40 | sed 's/^/        /'
        FAIL=$((FAIL + 1))
    fi
done

if [ "$UPDATE" -eq 1 ]; then
    echo "Golden-C: regenerated $UPDATED snapshot(s)"
    exit 0
fi

echo "Golden-C: $PASS pass, $FAIL fail"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
