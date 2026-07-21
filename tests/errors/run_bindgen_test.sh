#!/bin/bash
# Bindgen robustness: typedef resolution, attributed prototypes, multi-decl
# lines, matching-paren params, and multi-package [ffi] accumulation. (2026-07)
set -uo pipefail
WYN="${WYN:-./wyn}"
# run_bdd.sh invokes us from the repo root; make WYN absolute so we can cd.
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. Typedef resolution: unsigned-int + opaque-pointer typedefs bind.
cat > "$TMP/td.h" << 'EOF'
typedef unsigned int my_u32;
typedef struct opaque_s *my_handle;
extern my_u32 get_version(void);
extern my_handle open_thing(const char *name);
extern void close_thing(my_handle h);
EOF
out=$("$WYN" bind "$TMP/td.h" 2>/dev/null)
n=$(echo "$out" | grep -c "^extern fn")
[ "$n" = "3" ] && ok "typedef resolution (3 fns)" || bad "typedef resolution: got $n [$out]"

# 2. Attributed prototype (availability contains '=' - used to be skipped).
cat > "$TMP/attr.h" << 'EOF'
__attribute__((availability(macos,introduced=10.4)))
int my_fn(int x);
EOF
out=$("$WYN" bind "$TMP/attr.h" 2>/dev/null)
echo "$out" | grep -q "extern fn my_fn" && ok "availability-attributed prototype" || bad "attributed prototype: [$out]"

# 3. Many decls on ONE physical line (pcre2-style macro expansion).
printf 'extern int f1(int); extern int f2(int); extern int f3(int);\n' > "$TMP/multi.h"
out=$("$WYN" bind "$TMP/multi.h" 2>/dev/null)
n=$(echo "$out" | grep -c "^extern fn")
[ "$n" = "3" ] && ok "multi-decl single line (3 fns)" || bad "multi-decl line: got $n"

# 4. Trailing __asm after the param list (macOS SDK style).
printf 'int renamed_fn(int x) __asm("_other_name");\n' > "$TMP/asm.h"
out=$("$WYN" bind "$TMP/asm.h" 2>/dev/null)
echo "$out" | grep -q "extern fn renamed_fn(a0: int) -> int" && ok "trailing __asm ignored" || bad "__asm: [$out]"

# 5. Multi-package [ffi]: repeated sections ACCUMULATE (later used to clobber
#    earlier - only the last package linked).
mkdir -p "$TMP/proj" && cd "$TMP/proj"
cat > wyn.toml << 'EOF'
[package]
name = "t"
version = "0.1.0"

[ffi]
libs = "m"

[ffi]
libs = "z"
EOF
cat > t.wyn << 'EOF'
extern fn sqrt(x: float) -> float;
extern fn zlibVersion() -> string;
fn main() {
    println(sqrt(49.0).to_string())
    println("zlib " + zlibVersion())
}
EOF
out=$("$WYN" run t.wyn 2>&1)
echo "$out" | grep -q "7.0" && echo "$out" | grep -q "zlib " && ok "multi-[ffi] accumulate + link" || bad "multi-ffi: [$out]"

echo ""; echo "bindgen: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
