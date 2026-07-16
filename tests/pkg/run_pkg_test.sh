#!/usr/bin/env bash
# Git-URL dependency tests. Fully offline: dependencies are served from local
# bare/working git repos over file://, never the network. Exercises the whole
# path — spec parsing → clone into cache → manifest+lock → import resolution →
# compile+run — plus C-binding [ffi] union, remove, and lockfile restore.
set -u
WYN_BIN="${WYN:-./wyn}"
case "$WYN_BIN" in /*) ;; *) WYN_BIN="$(pwd)/$WYN_BIN" ;; esac

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
export WYN_PKG_CACHE="$work/cache"
export HOME="$work/home"; mkdir -p "$HOME"

fail() { echo "pkg: FAIL ($1)"; exit 1; }

git_init() { git -C "$1" init -q; git -C "$1" config user.email t@t; git -C "$1" config user.name t; }

# --- fixture 1: a pure-Wyn library 'greet', tagged v1.0.0 -------------------
mkdir -p "$work/greet"
git_init "$work/greet"
cat > "$work/greet/wyn.toml" <<'EOF'
[project]
name = "greet"
version = "1.0.0"
EOF
cat > "$work/greet/greet.wyn" <<'EOF'
pub fn hello() -> string { return "hi from greet" }
EOF
git -C "$work/greet" add -A; git -C "$work/greet" commit -qm init; git -C "$work/greet" tag v1.0.0

# --- fixture 2: a C-binding package 'cmath' with [ffi] libs=m --------------
mkdir -p "$work/cmath"
git_init "$work/cmath"
cat > "$work/cmath/wyn.toml" <<'EOF'
[project]
name = "cmath"
[ffi]
libs = "m"
EOF
cat > "$work/cmath/cmath.wyn" <<'EOF'
extern fn sqrt(x: float) -> float;
EOF
git -C "$work/cmath" add -A; git -C "$work/cmath" commit -qm init

# --- consumer project -------------------------------------------------------
proj="$work/app"; mkdir -p "$proj/src"; cd "$proj"
cat > wyn.toml <<'EOF'
[project]
name = "app"
version = "0.1.0"
entry = "src/main.wyn"
EOF

# 1. add the Wyn lib pinned at a tag ----------------------------------------
"$WYN_BIN" add "file://$work/greet@v1.0.0" --as greet >/dev/null 2>&1 || fail "wyn add greet"
grep -q 'greet = "file://.*greet@v1.0.0"' wyn.toml || fail "greet not in [dependencies]"
grep -q "^greet .*greet v1.0.0 [0-9a-f]" wyn.lock || fail "greet not pinned in wyn.lock"

# 2. import + run -----------------------------------------------------------
cat > src/main.wyn <<'EOF'
import greet
fn main() { println(greet.hello()) }
EOF
out="$("$WYN_BIN" run src/main.wyn 2>/dev/null | grep -v 'Compiled in')"
[ "$out" = "hi from greet" ] || fail "import/run greet (got '$out')"

# 3. bare-name expansion → github.com/wynlang/* (no clone; check the error URL)
#    A bare name must resolve to the official org. We don't hit the network:
#    a bogus name fails to clone, but the attempted URL proves the expansion.
add_err="$("$WYN_BIN" add __wyn_nope_zzz__ 2>&1)"
echo "$add_err" | grep -q "github.com/wynlang/__wyn_nope_zzz__" || fail "bare name should expand to github.com/wynlang/*"

# 4. C-binding package: [ffi] libs=m must be unioned into the link line ------
"$WYN_BIN" add "file://$work/cmath" --as cmath >/dev/null 2>&1 || fail "wyn add cmath"
cat > src/main.wyn <<'EOF'
import cmath
fn main() { println("${sqrt(169.0)}") }
EOF
out="$("$WYN_BIN" run src/main.wyn 2>/dev/null | grep -v 'Compiled in')"
[ "$out" = "13.0" ] || fail "dep [ffi] should link -lm (got '$out')"

# 5. list shows both, marked present ----------------------------------------
"$WYN_BIN" list 2>/dev/null | grep -q "greet" || fail "list missing greet"
"$WYN_BIN" list 2>/dev/null | grep -q "cmath" || fail "list missing cmath"

# 6. remove drops from manifest + lock --------------------------------------
"$WYN_BIN" remove greet >/dev/null 2>&1 || fail "wyn remove greet"
grep -q "greet" wyn.toml && fail "greet still in wyn.toml after remove"
grep -q "^greet " wyn.lock && fail "greet still in wyn.lock after remove"

# 7. restore reinstalls the remaining dep from the lock into a fresh cache ---
rm -rf "$WYN_PKG_CACHE"
"$WYN_BIN" restore >/dev/null 2>&1 || fail "wyn restore"
find "$WYN_PKG_CACHE" -name 'cmath.wyn' | grep -q . || fail "restore did not repopulate cache"
# and it still builds after a pure-lock restore
out="$("$WYN_BIN" run src/main.wyn 2>/dev/null | grep -v 'Compiled in')"
[ "$out" = "13.0" ] || fail "run after restore (got '$out')"

# 8. cache reuse: a second project adding the same url@ref reuses the clone --
proj2="$work/app2"; mkdir -p "$proj2"; cd "$proj2"
printf '[project]\nname = "app2"\nversion = "0.1.0"\n' > wyn.toml
before="$(find "$WYN_PKG_CACHE" -maxdepth 6 -name '*@*' -type d | wc -l | tr -d ' ')"
"$WYN_BIN" add "file://$work/cmath" --as cmath >/dev/null 2>&1 || fail "second-project add"
after="$(find "$WYN_PKG_CACHE" -maxdepth 6 -name '*@*' -type d | wc -l | tr -d ' ')"
[ "$before" = "$after" ] || fail "cache not reused across projects ($before -> $after)"

echo "pkg: PASS"
