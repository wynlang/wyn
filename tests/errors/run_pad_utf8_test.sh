#!/bin/bash
# pad_left / pad_right align by CHARACTER, not by byte (2026-08).
#
# They used strlen, so every non-ASCII string looked longer than it is. A box-drawing
# rule "────" is 4 characters and 12 bytes, so `pad_right(9)` saw a 12-wide string,
# decided it was already over-width, and padded NOTHING - silently breaking the
# alignment of every table in the sample apps that draws a rule, and of any column
# containing an accent or an emoji.
#
# There were TWO implementations to fix: wyn_string_pad_* in stdlib_string.c and
# string_pad_* in wyn_runtime.h. Generated code calls the runtime one, which is why
# fixing only the first changed nothing observable - that is worth knowing before
# touching these again.
#
# The allocation is sized in BYTES while the padding is counted in CHARACTERS: sizing
# by `width` truncates multi-byte content, which is the bug you get from fixing the
# count and forgetting the buffer.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

cat > "$TMP/a.wyn" <<'EOF'
fn main() {
    // ASCII: unchanged behaviour, and the control for everything below.
    print("[" + "abcd".pad_right(9, "-") + "]")
    print("[" + "abcd".pad_left(9, "-") + "]")

    // Box drawing: 4 characters, 12 bytes. This is the case that was broken.
    print("[" + "────".pad_right(9, "-") + "]")
    print("[" + "────".pad_left(9, "-") + "]")

    // Accented Latin: 2 bytes per character.
    print("[" + "café".pad_right(8, ".") + "]")

    // An emoji is 4 bytes and one character.
    print("[" + "ok✅".pad_right(6, ".") + "]")

    // Already at or over the requested width: returned unchanged, not truncated.
    print("[" + "────".pad_right(4, "-") + "]")
    print("[" + "────".pad_right(2, "-") + "]")

    // Mixed ASCII and multi-byte in one string.
    print("[" + "a─b".pad_right(6, ".") + "]")
}
EOF

out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
got=$(printf '%s' "$out" | grep '^\[')

# Every line is 9 visible columns wide except where stated. Written out in full rather
# than computed, so the expectation cannot drift with the implementation.
want='[abcd-----]
[-----abcd]
[────-----]
[-----────]
[café....]
[ok✅...]
[────]
[────]
[a─b...]'

if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "padding is counted in characters, for ASCII and multi-byte alike"
else
  bad "padding output wrong"
  echo "        got:"; printf '%s\n' "$got" | sed 's/^/          /'
  echo "        want:"; printf '%s\n' "$want" | sed 's/^/          /'
fi

# The alignment property on its own, independent of the exact bytes: a multi-byte
# string padded to N and an ASCII string padded to N must occupy the SAME number of
# characters. This is what a table needs, and it is checked by counting characters
# rather than by comparing text.
cat > "$TMP/b.wyn" <<'EOF'
fn main() {
    var ascii = "abcd".pad_right(12, " ")
    var boxes = "────".pad_right(12, " ")
    var emoji = "ok✅".pad_right(12, " ")
    // .chars() counts characters, so these must all be 12.
    print("${ascii.chars().len()} ${boxes.chars().len()} ${emoji.chars().len()}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^12 12 12$'; then
  ok "an ASCII and a multi-byte column padded to 12 are both 12 characters"
else
  bad "column widths disagree"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# NOTHING IS TRUNCATED. Fixing the count while sizing the buffer by `width` would drop
# bytes off the end of a multi-byte string - the padded value must still CONTAIN the
# original.
cat > "$TMP/c.wyn" <<'EOF'
fn main() {
    var p = "──é──".pad_right(20, ".")
    if p.contains("──é──") { print("intact") } else { print("TRUNCATED: ${p}") }
    var q = "──é──".pad_left(20, ".")
    if q.contains("──é──") { print("intact") } else { print("TRUNCATED: ${q}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] && [ "$(printf '%s' "$out" | grep -c '^intact$')" = "2" ]; then
  ok "the original string survives padding, both sides"
else
  bad "padding truncated a multi-byte string"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "pad-utf8: $PASS pass, 0 fail"
  exit 0
fi
echo "pad-utf8: $PASS pass, $FAIL fail"
exit 1
