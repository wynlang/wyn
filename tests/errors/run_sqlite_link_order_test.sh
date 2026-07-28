#!/bin/bash
# `Db.` programs must LINK against the system libsqlite3, not just compile.
#
# THE BUG: src/main.c put `-lsqlite3` in `sqlite_flags`, which is spliced in
# EARLY on the link line (right after -o, before the .c and the runtime archive).
# GNU ld resolves a `-l` only against objects listed BEFORE it, so every Db.*
# symbol came back undefined:
#
#   test_sqlite.wyn.c:(.text.Db_exec+0x88): undefined reference to `sqlite3_exec'
#   ... 18 of them for tests/stdlib/test_sqlite.wyn
#
# Apple's ld does not care about -l position, so this was INVISIBLE on macOS and
# broke Linux only: the 13 tests/stdlib files that use `Db.` all failed to build
# on ubuntu-latest while every macOS job stayed green. The fix moves -lsqlite3
# into `sqlite_src`, which is already appended at the END of every link line -
# the same treatment the FFI flags next to it already get, and for the same
# reason.
#
# This guards the BUILTIN `Db.` path against a system libsqlite3. It is NOT what
# tests/cpkg/run_sqlite_test.sh covers: that exercises `wyn add sqlite3` +
# `import sqlite3` (the FFI/bindgen route, whose libs come from wyn.toml [ffi]),
# and it passed green through this entire bug.
#
# Skips cleanly where a system libsqlite3 is not linkable, and deliberately skips
# when ./packages/sqlite/src/sqlite3.c exists, because that takes the vendored
# branch instead and would not exercise -lsqlite3 at all.
set -uo pipefail

WYN="${WYN:-./wyn}"
case "$WYN" in /*) ;; *) WYN="$(pwd)/$WYN" ;; esac

pass=0; fail=0
ok()   { echo "  ok    $1"; pass=$((pass+1)); }
bad()  { echo "  FAIL  $1"; fail=$((fail+1)); }

if [ -f ./packages/sqlite/src/sqlite3.c ]; then
    echo "sqlite-link-order: SKIP (./packages/sqlite vendored - -lsqlite3 branch not taken)"
    exit 0
fi
if ! echo '#include <sqlite3.h>
int main(void){return 0;}' | cc -x c - -lsqlite3 -o /dev/null 2>/dev/null; then
    echo "sqlite-link-order: SKIP (system sqlite3 not linkable)"
    exit 0
fi

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT

cat > "$work/db.wyn" <<'WYN'
fn main() {
    var db = Db.open(":memory:")
    Db.exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT)")
    Db.exec(db, "INSERT INTO t(v) VALUES('wyn')")
    var got = Db.query_one(db, "SELECT v FROM t")
    Db.close(db)
    println(got)
}
WYN

# Bounded: a regression must fail loudly, never hang the suite. Stock macOS has
# no `timeout`, so use perl's alarm (the convention in this directory).
build_out=$(perl -e 'alarm 90; exec @ARGV' \
    "$WYN" build "$work/db.wyn" -o "$work/db" 2>&1)
build_rc=$?

if [ "$build_rc" -eq 0 ] && [ -x "$work/db" ]; then
    ok "Db. program links against system libsqlite3"
else
    bad "Db. program failed to build (rc=$build_rc)"
    # Print the diagnostic in full - the stdlib runner's head -2 truncation is
    # what hid this error for a whole CI cycle.
    echo "$build_out" | sed 's/^/        /'
fi

# The undefined-reference signature specifically, so a future regression is
# named rather than just "build failed".
if echo "$build_out" | grep -q 'undefined reference to .sqlite3_'; then
    bad "link line has -lsqlite3 in the wrong position (undefined sqlite3_* refs)"
else
    ok "no undefined sqlite3_* references"
fi

if [ -x "$work/db" ]; then
    run_out=$(perl -e 'alarm 30; exec @ARGV' "$work/db" 2>&1)
    if [ "$run_out" = "wyn" ]; then
        ok "linked binary actually queries SQLite (got 'wyn')"
    else
        bad "expected 'wyn', got '$run_out'"
    fi
fi

echo ""
echo "sqlite-link-order: $pass pass, $fail fail"
[ "$fail" -eq 0 ] || exit 1
