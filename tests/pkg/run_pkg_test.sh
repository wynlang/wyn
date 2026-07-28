#!/usr/bin/env bash
# Git-URL dependency tests. Fully offline: dependencies are served from local
# bare/working git repos over file://, never the network. Exercises the whole
# path - spec parsing → clone into cache → manifest+lock → import resolution →
# compile+run - plus C-binding [ffi] union, remove, and lockfile restore.
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

# --- 9. wyn.lock ENFORCEMENT -------------------------------------------------
# wyn.lock claims to pin exact commits. It used to only be *read* for its url+ref
# and the recorded sha was never compared against what got cloned, so a moved
# branch/tag silently produced different code than the lockfile recorded. These
# cases prove the sha now governs. All hermetic: file:// remotes, no network.

# fixture: 'pinlib' with tag v1 at commit A, then retagged to commit B.
mkdir -p "$work/pinlib"
git_init "$work/pinlib"
printf '[project]\nname = "pinlib"\n' > "$work/pinlib/wyn.toml"
printf 'pub fn v() -> int { return 1 }\n' > "$work/pinlib/pinlib.wyn"
git -C "$work/pinlib" add -A; git -C "$work/pinlib" commit -qm A; git -C "$work/pinlib" tag v1
SHA_A="$(git -C "$work/pinlib" rev-parse HEAD)"
printf 'pub fn v() -> int { return 2 }\n' > "$work/pinlib/pinlib.wyn"
git -C "$work/pinlib" add -A; git -C "$work/pinlib" commit -qm B
SHA_B="$(git -C "$work/pinlib" rev-parse HEAD)"

proj3="$work/app3"; mkdir -p "$proj3/src"; cd "$proj3"
printf '[project]\nname = "app3"\nversion = "0.1.0"\n' > wyn.toml
"$WYN_BIN" add "file://$work/pinlib@v1" --as pinlib >/dev/null 2>&1 || fail "add pinlib"
grep -q "^pinlib .* v1 $SHA_A\$" wyn.lock || fail "lock did not record commit A ($(cat wyn.lock | tail -1))"

# 9a. A lock sha that does not exist anywhere must be REJECTED, not silently
#     ignored. (Fabricated-but-well-formed sha == a tampered lockfile.)
BOGUS="deadbeefdeadbeefdeadbeefdeadbeefdeadbeef"
printf '%s file://%s v1 %s\n' pinlib "$work/pinlib" "$BOGUS" > wyn.lock
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -ne 0 ] || fail "restore accepted a lock sha that does not exist (rc=0)"
echo "$out" | grep -q "integrity check failed" || fail "no integrity error for bogus sha [$out]"
echo "$out" | grep -q "$BOGUS" || fail "integrity error omits the expected sha"
echo "$out" | grep -q "wyn pkg add" || fail "integrity error does not say how to update"

# 9b. ref written as '-' (meaningless, resolves to HEAD): the sha must still
#     govern - that is exactly the case where the ref proves nothing.
printf '%s file://%s - %s\n' pinlib "$work/pinlib" "$BOGUS" > wyn.lock
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -ne 0 ] || fail "restore accepted bogus sha with ref '-' (rc=0)"
echo "$out" | grep -q "integrity check failed" || fail "ref '-' bypassed the sha check [$out]"

# 9c. A MOVED tag whose locked commit is still fetchable must resolve to the
#     LOCKED commit (that is what pinning means), not to the tag's new tip.
git -C "$work/pinlib" tag -f v1 >/dev/null 2>&1   # v1: A -> B
printf '%s file://%s v1 %s\n' pinlib "$work/pinlib" "$SHA_A" > wyn.lock
rm -rf "$WYN_PKG_CACHE"
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -eq 0 ] || fail "restore of a moved tag with a fetchable locked commit failed [$out]"
cdir="$(find "$WYN_PKG_CACHE" -type d -name 'pinlib@v1' | head -1)"
[ -n "$cdir" ] || fail "moved-tag restore did not populate the cache"
got="$(git -C "$cdir" rev-parse HEAD)"
[ "$got" = "$SHA_A" ] || fail "moved tag: checkout is $got, lock pinned $SHA_A (pin not honored)"
[ "$got" != "$SHA_B" ] || fail "moved tag: followed the retagged tip instead of the lock"

# 9d. A matching sha still installs clean and says so.
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -eq 0 ] || fail "restore of an already-pinned dep failed [$out]"
echo "$out" | grep -qE "verified|pinned from lock" || fail "restore does not report verification [$out]"

# 9e. LEGACY 3-token lockfile (`name url sha`, no ref column): there is nothing
#     to verify a ref against, but the sha still must not be dropped. Must WARN
#     and keep working - never hard-fail an existing project - and must record a
#     4-token entry so the NEXT install is verified.
git -C "$work/pinlib" tag -f v1 "$SHA_A" >/dev/null 2>&1   # restore v1 -> A
rm -rf "$WYN_PKG_CACHE"
printf '%s file://%s\n' pinlib "$work/pinlib" > wyn.lock   # 2-token: no sha at all
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -eq 0 ] || fail "legacy lockfile (no sha) hard-failed - would break existing projects [$out]"
echo "$out" | grep -q "legacy lockfile" || fail "legacy lockfile did not warn [$out]"
grep -qE "^pinlib file://[^ ]+ [^ ]+ [0-9a-f]{40}\$" wyn.lock || fail "legacy lockfile was not upgraded to a pinned entry ($(tail -1 wyn.lock))"
# and now that it is pinned, a tampered sha is caught
sed -i.bak "s/[0-9a-f]\{40\}/$BOGUS/" wyn.lock && rm -f wyn.lock.bak
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -ne 0 ] || fail "upgraded legacy lock is still not enforced"

# 9f. LEGACY 3-token lockfile (`name url sha` - a sha but NO ref column). This
#     form DOES carry a commit, so it must be honored, not warned away: the
#     clone follows the default branch (bucket @HEAD) and then gets pinned back
#     to the recorded commit. SHA_A is one commit behind the branch tip.
rm -rf "$WYN_PKG_CACHE"
printf '%s file://%s %s\n' pinlib "$work/pinlib" "$SHA_A" > wyn.lock
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -eq 0 ] || fail "legacy 3-token lock with a real sha failed to install [$out]"
cdir="$(find "$WYN_PKG_CACHE" -type d -name 'pinlib@HEAD' | head -1)"
[ -n "$cdir" ] || fail "3-token legacy restore did not populate the cache"
got="$(git -C "$cdir" rev-parse HEAD)"
[ "$got" = "$SHA_A" ] || fail "3-token legacy lock: checkout is $got, lock pinned $SHA_A"
# a tampered 3-token sha is still rejected
printf '%s file://%s %s\n' pinlib "$work/pinlib" "$BOGUS" > wyn.lock
out="$("$WYN_BIN" restore 2>&1)"; rc=$?
[ "$rc" -ne 0 ] || fail "3-token legacy lock sha is not enforced"

echo "pkg: PASS"
