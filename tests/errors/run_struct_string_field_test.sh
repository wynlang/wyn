#!/bin/bash
# A struct's string field initialised from an ARRAY ELEMENT survives (2026-08).
#
#     struct Row { name: string }
#     for line in lines {
#         var f = line.split(",")
#         rows.push(Row { name: f[0] })      // name came back EMPTY
#     }
#
# `array_get_str` returns a pointer INTO the array rather than a copy, so the struct
# stored a pointer owned by `f`. When the loop reassigned `f` on the next iteration that
# array was released, and every struct built earlier had an empty string field.
#
# SILENTLY, AT EXIT 0, AND ONLY THE STRINGS. In the CSV parser that found this, the
# numeric fields were all correct and the names were blank - which reads as a parsing
# bug in the program, not a compiler bug, and is exactly why it survived. Outside a loop
# it worked, because nothing released the array before the read.
#
# The fix copies such a value at the struct-init site, applied only to declared `string`
# fields whose initialiser is an index expression - a literal, a variable or a call
# result is already owned, and pays nothing.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# ---- the exact shape that lost its data ------------------------------------

cat > "$TMP/a.wyn" <<'EOF'
struct Row { name: string, score: int }

fn main() {
    var rows = []
    for line in ["ann,90", "bob,70", "cid,50"] {
        var f = line.split(",")
        rows.push(Row { name: f[0], score: f[1].to_int() })
    }
    for r in rows { print("${r.name}=${r.score}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run a.wyn 2>&1); code=$?
want='ann=90
bob=70
cid=50'
got=$(printf '%s' "$out" | grep '=')
if [ $code -eq 0 ] && [ "$got" = "$want" ]; then
  ok "string fields from split() survive the loop"
else
  bad "string field lost"; printf '%s\n' "$got" | head -4 | sed 's/^/        /'
fi

# The failure mode named specifically: the NUMBERS were always fine, so a test that
# only checked them would have passed throughout.
if printf '%s' "$out" | grep -q '^=90$'; then
  bad "a name came back EMPTY - the borrowed pointer dangled again"
else
  ok "no empty names"
fi

# ---- more than one string field, and a later read --------------------------
# Two borrowed fields from the same array, read after many more iterations, so a
# short-lived accident cannot pass.
cat > "$TMP/b.wyn" <<'EOF'
struct P { first: string, last: string, n: int }

fn main() {
    var ps = []
    var raw = ["ann,smith", "bob,jones", "cid,brown", "dot,white", "eve,green"]
    var i = 0
    for line in raw {
        var f = line.split(",")
        ps.push(P { first: f[0], last: f[1], n: i })
        i += 1
    }
    // Read only AFTER every push, so every element has outlived its source array.
    for p in ps { print("${p.n}:${p.first} ${p.last}") }
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run b.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^0:ann smith$' &&
   printf '%s' "$out" | grep -q '^4:eve green$'; then
  ok "two string fields per struct, read after the loop, both intact"
else
  bad "multi-field case failed"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

# ---- a nested index, and an array-of-arrays -------------------------------
cat > "$TMP/c.wyn" <<'EOF'
struct Cell { text: string }

fn main() {
    var cells = []
    for row in ["a|b", "c|d"] {
        var parts = row.split("|")
        for p in parts { cells.push(Cell { text: p }) }
    }
    var joined = ""
    for c in cells { joined = joined + c.text }
    print("joined=${joined}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run c.wyn 2>&1); code=$?
if [ $code -eq 0 ] && printf '%s' "$out" | grep -q '^joined=abcd$'; then
  ok "a struct field from a loop VARIABLE over a split also survives"
else
  bad "loop-variable case failed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- and nothing that already worked changed ------------------------------
# The fix touches only string fields fed by an index expression. Everything else must
# emit exactly as before, so these are the control group.
cat > "$TMP/d.wyn" <<'EOF'
struct S { a: string, n: int, f: float, b: bool }

fn make(v: string) -> string { return v.upper() }

fn main() {
    // A literal, a variable, a call result, and non-string fields.
    var v = "var"
    var s1 = S { a: "lit", n: 1, f: 1.5, b: true }
    var s2 = S { a: v, n: 2, f: 2.5, b: false }
    var s3 = S { a: make("call"), n: 3, f: 3.5, b: true }
    print("${s1.a}/${s1.n}/${s1.f}/${s1.b}")
    print("${s2.a}/${s2.n}")
    print("${s3.a}/${s3.n}")

    // An INT field from an index must not be wrapped as a string.
    var nums = [10, 20]
    var s4 = S { a: "x", n: nums[1], f: 0.0, b: false }
    print("${s4.a}/${s4.n}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" run d.wyn 2>&1); code=$?
if [ $code -eq 0 ] &&
   printf '%s' "$out" | grep -q '^lit/1/1.5/true$' &&
   printf '%s' "$out" | grep -q '^var/2$' &&
   printf '%s' "$out" | grep -q '^CALL/3$' &&
   printf '%s' "$out" | grep -q '^x/20$'; then
  ok "literals, variables, call results and non-string fields unaffected"
else
  bad "a control case broke"; printf '%s\n' "$out" | head -6 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "struct-string-field: $PASS pass, 0 fail"
  exit 0
fi
echo "struct-string-field: $PASS pass, $FAIL fail"
exit 1
