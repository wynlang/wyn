#!/bin/bash
# The unused-variable warning must not fire for a variable that IS read.
#
# add_symbol always APPENDS, so re-declaring a name in an inner block creates a
# SECOND slot in the same per-function symbol table. find_symbol returns only one
# of them, so mark_used marked one and left the other permanently unused:
#
#     var line_start = 0            # outer
#     while ... {
#         var line_start = i + 1    # inner, shadows
#         print("${line_start}")    # READS it - yet it was reported unused
#     }
#
# Why this is worth a test rather than a shrug: a FALSE warning on correct,
# committed code teaches people to ignore all warnings, at which point the real
# ones stop working too. Measured across sample-apps: 5 unused-variable warnings
# on working code, 4 of them false (netstat-lite x3, dockermon).
#
# The risk in fixing it is over-correction - silencing the diagnostic entirely -
# so the genuine cases below are asserted just as hard as the false ones, and the
# mutation test for this fix is "does a real unused variable still warn".
set -uo pipefail
WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# $1=name  $2=file  $3=want-warning(yes/no)  $4=varname
expect_warn() {
    local name="$1" file="$2" want="$3" var="$4" out
    out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$file" 2>&1)
    if echo "$out" | grep -q "unused variable '$var'"; then
        if [ "$want" = "yes" ]; then ok "$name"
        else bad "$name (FALSE warning for '$var', which is read)"; fi
    else
        if [ "$want" = "no" ]; then ok "$name"
        else bad "$name (expected a warning for '$var' and got none)"; fi
    fi
}

# --- the false positive: an inner shadow that IS read ---------------------
cat > "$TMP/shadow.wyn" <<'WYN'
fn scan(s: string) -> int {
    var line_start = 0
    var i = 0
    while i < s.len() {
        if s.char_at(i) == "\n" {
            var line_start = i + 1
            print("${line_start}")
        }
        i = i + 1
    }
    return line_start
}
fn main() { print("${scan("a\nb\n")}") }
WYN
expect_warn "an inner shadowed variable that is read gets NO warning" "$TMP/shadow.wyn" no line_start

# --- sibling scopes reusing one name, both read --------------------------
cat > "$TMP/siblings.wyn" <<'WYN'
fn main() {
    if 1 == 1 {
        var v = 5
        print("${v}")
    }
    if 2 == 2 {
        var v = 9
        print("${v}")
    }
}
WYN
expect_warn "sibling scopes reusing a name get NO warning" "$TMP/siblings.wyn" no v

# --- THE CONTROL: a genuinely unused variable MUST still warn ------------
# Without this the fix could "pass" by disabling the diagnostic altogether.
cat > "$TMP/genuine.wyn" <<'WYN'
fn f() -> int {
    var genuinely_unused = 42
    var used = 7
    return used
}
fn main() { print("${f()}") }
WYN
expect_warn "a genuinely unused variable STILL warns" "$TMP/genuine.wyn" yes genuinely_unused

# --- and one that is unused in an INNER scope only ----------------------
cat > "$TMP/inner_dead.wyn" <<'WYN'
fn main() {
    if 1 == 1 {
        var dead_inner = 3
    }
    print("done")
}
WYN
expect_warn "an unused variable in an inner scope STILL warns" "$TMP/inner_dead.wyn" yes dead_inner

# --- the underscore convention still suppresses -------------------------
cat > "$TMP/underscore.wyn" <<'WYN'
fn main() {
    var _ignored = 5
    print("done")
}
WYN
expect_warn "an _-prefixed unused variable is still suppressed" "$TMP/underscore.wyn" no _ignored

# --- FOUR sibling declarations, the shape found in the wild --------------
# sysadmin/netstat-lite declares `line_start` four times in one function and
# reads all four; before the fix the FIRST was marked used and the other three
# warned. This is the case that required walking the parent chain, because the
# reads happen in a child scope while the declarations sit in the function scope -
# marking only the scope handed to mark_used was not enough.
cat > "$TMP/four.wyn" <<'WYN'
fn main() {
    var line_start = 0
    print("${line_start}")
    if 1 == 1 {
        var line_start = 0
        for idx in 0..3 {
            var line = "x"
            line_start = idx + 1
            print("${line}${line_start}")
        }
    }
    if 2 == 2 {
        var line_start = 0
        for idx in 0..3 {
            line_start = idx + 2
            print("${line_start}")
        }
    }
}
WYN
expect_warn "four sibling declarations, all read, get NO warning" "$TMP/four.wyn" no line_start

# --- and the programs above must actually still RUN CORRECTLY -----------
# A diagnostics change must not alter behaviour; assert the values too.
out=$(perl -e 'alarm(60); exec @ARGV' -- "$WYN" run "$TMP/shadow.wyn" 2>&1)
if echo "$out" | grep -qxF "2" && echo "$out" | grep -qxF "4" && echo "$out" | grep -qxF "0"; then
    ok "shadowing program still computes the right values"
else
    bad "shadowing program output changed: $(echo "$out" | grep -vE 'Compiled in|^$|Warning:' | tr '\n' '|')"
fi

echo ""; echo "unused-shadow: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
