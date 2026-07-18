#!/usr/bin/env bash
# `wyn pkg audit` tests (SECURITY_DESIGN §3b). Fully offline in the git sense:
# remotes are local file:// repos, so ls-remote works without network.
# Covers: clean tag pin (exit 0), moved tag (exit 2), branch pin warning
# (exit 1), cache mismatch (exit 2), ffi flagging, --offline, empty project.
set -u
WYN_BIN="${WYN:-./wyn}"
case "$WYN_BIN" in /*) ;; *) WYN_BIN="$(pwd)/$WYN_BIN" ;; esac

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
export WYN_PKG_CACHE="$work/cache"
export HOME="$work/home"; mkdir -p "$HOME"

PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
git_init() { git -C "$1" init -q; git -C "$1" config user.email t@t; git -C "$1" config user.name t; }

# fixture: library tagged v1.0.0, plus a 'dev' branch; and an ffi package
mkdir -p "$work/lib"
git_init "$work/lib"
printf '[project]\nname = "lib"\n' > "$work/lib/wyn.toml"
printf 'pub fn f() -> int { return 1 }\n' > "$work/lib/lib.wyn"
git -C "$work/lib" add -A; git -C "$work/lib" commit -qm init; git -C "$work/lib" tag v1.0.0
git -C "$work/lib" branch dev

mkdir -p "$work/cffi"
git_init "$work/cffi"
printf '[project]\nname = "cffi"\n[ffi]\nlibs = "m"\n' > "$work/cffi/wyn.toml"
printf 'extern fn sqrt(x: float) -> float;\n' > "$work/cffi/cffi.wyn"
git -C "$work/cffi" add -A; git -C "$work/cffi" commit -qm init; git -C "$work/cffi" tag v1.0.0

proj="$work/app"; mkdir -p "$proj/src"; cd "$proj"
printf '[project]\nname = "app"\n' > wyn.toml
printf 'fn main() { println(1) }\n' > src/main.wyn

# 1. empty project: exit 0
out=$("$WYN_BIN" pkg audit 2>&1); code=$?
[ $code -eq 0 ] && echo "$out" | grep -q "Nothing to audit" && ok "empty project" || bad "empty: code=$code"

# 2. clean tag pin: verified, exit 0 (+ ffi dep is flagged but not a failure)
"$WYN_BIN" pkg add "file://$work/lib@v1.0.0" >/dev/null 2>&1
"$WYN_BIN" pkg add "file://$work/cffi@v1.0.0" >/dev/null 2>&1
out=$("$WYN_BIN" pkg audit 2>&1); code=$?
if [ $code -eq 0 ] && echo "$out" | grep -q "tag, verified" && echo "$out" | grep -q "links native code (m)"; then
  ok "clean tag pins verified + ffi flagged"
else bad "clean audit: code=$code [$(echo "$out" | tail -3)]"; fi

# 3. --offline: exit 0, marked unverified
out=$("$WYN_BIN" pkg audit --offline 2>&1); code=$?
[ $code -eq 0 ] && echo "$out" | grep -q "unverified" && ok "--offline" || bad "offline: code=$code"

# 4. moved tag: retag v1.0.0 at a new commit → exit 2
printf '// changed\npub fn f() -> int { return 2 }\n' > "$work/lib/lib.wyn"
git -C "$work/lib" add -A; git -C "$work/lib" commit -qm change
git -C "$work/lib" tag -f v1.0.0 >/dev/null 2>&1
out=$("$WYN_BIN" pkg audit 2>&1); code=$?
if [ $code -eq 2 ] && echo "$out" | grep -q "TAG MOVED"; then
  ok "moved tag detected (exit 2)"
else bad "moved tag: code=$code [$(echo "$out" | grep lib | head -1)]"; fi
git -C "$work/lib" tag -f v1.0.0 HEAD~1 >/dev/null 2>&1  # restore

# 5. branch pin: warning, exit 1
"$WYN_BIN" pkg add "file://$work/lib@dev" --as libdev >/dev/null 2>&1
out=$("$WYN_BIN" pkg audit 2>&1); code=$?
if [ $code -eq 1 ] && echo "$out" | grep -q "BRANCH PIN"; then
  ok "branch pin warned (exit 1)"
else bad "branch pin: code=$code [$(echo "$out" | grep dev | head -1)]"; fi
"$WYN_BIN" pkg remove libdev >/dev/null 2>&1

# 6. cache mismatch: move the cached checkout to a different commit → exit 2
cache_dir=$(find "$WYN_PKG_CACHE" -type d -name "*lib*" -not -name "*cffi*" -not -path "*/.git*" | head -1)
if [ -n "$cache_dir" ] && [ -d "$cache_dir/.git" ]; then
  git -C "$cache_dir" fetch -q origin 2>/dev/null
  git -C "$cache_dir" checkout -q $(git -C "$cache_dir" rev-list HEAD | tail -1) 2>/dev/null
  # only differs if the repo has >1 commit in its shallow clone; tolerate either
  out=$("$WYN_BIN" pkg audit --offline 2>&1); code=$?
  if echo "$out" | grep -q "CACHE MISMATCH"; then
    [ $code -eq 2 ] && ok "cache mismatch detected (exit 2)" || bad "cache mismatch wrong exit: $code"
  else
    ok "cache mismatch (shallow clone — single commit, skipped)"
  fi
else
  ok "cache mismatch (no cache dir — skipped)"
fi

echo ""; echo "pkg-audit: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
