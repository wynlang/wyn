#!/usr/bin/env bash
# `wyn add sqlite3` end-to-end dogfood: add the SQLite binding from the curated
# registry, then build+run a program that opens a DB, creates a table, inserts
# rows via prepared statements, and queries them back — proving the whole FFI
# pipeline (add → bindgen → import → [ffi] link → opaque handles via Ptr → build
# → run) against a real, non-trivial C library with opaque out-parameters.
#
# Skips cleanly if sqlite3 isn't installed (no header / not linkable).
set -u
WYN_BIN="${WYN:-./wyn}"
case "$WYN_BIN" in /*) ;; *) WYN_BIN="$(pwd)/$WYN_BIN" ;; esac
ROOT="$(pwd)"

# Skip if sqlite3 isn't available on this machine.
if ! pkg-config --exists sqlite3 2>/dev/null && ! echo '#include <sqlite3.h>
int main(void){return 0;}' | cc -x c - -lsqlite3 -o /dev/null 2>/dev/null; then
  echo "sqlite: SKIP (sqlite3 not installed)"
  exit 0
fi

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cd "$work"
export WYN_ROOT="$ROOT"

"$WYN_BIN" add sqlite3 >/dev/null 2>&1 || { echo "sqlite: FAIL (wyn add sqlite3)"; exit 1; }
grep -q 'extern fn sqlite3_open' packages/sqlite3/sqlite3.wyn 2>/dev/null || { echo "sqlite: FAIL (no sqlite3_open binding)"; exit 1; }
grep -q 'libs = "sqlite3"' wyn.toml 2>/dev/null || { echo "sqlite: FAIL (sqlite3 not linked)"; exit 1; }

cat > app.wyn <<'WYN'
import sqlite3

fn run(db: ptr, sql: string) {
    var cell = Ptr.cell()
    sqlite3_prepare_v2(db, sql, -1, cell, 0)
    var stmt = Ptr.read(cell)
    sqlite3_step(stmt)
    sqlite3_finalize(stmt)
    Ptr.free(cell)
}

fn main() -> int {
    var dbcell = Ptr.cell()
    if sqlite3_open(":memory:", dbcell) != 0 { println("open failed"); return 1 }
    var db = Ptr.read(dbcell)

    run(db, "CREATE TABLE nums (v INTEGER, label TEXT)")
    run(db, "INSERT INTO nums VALUES (10, 'ten')")
    run(db, "INSERT INTO nums VALUES (20, 'twenty')")
    run(db, "INSERT INTO nums VALUES (30, 'thirty')")

    var cell = Ptr.cell()
    sqlite3_prepare_v2(db, "SELECT v, label FROM nums ORDER BY v", -1, cell, 0)
    var stmt = Ptr.read(cell)
    var total = 0
    while sqlite3_step(stmt) == 100 {
        var v = sqlite3_column_int(stmt, 0)
        var label = sqlite3_column_text(stmt, 1)
        println("row ${v} ${label}")
        total = total + v
    }
    sqlite3_finalize(stmt)
    Ptr.free(cell)
    println("total ${total}")

    sqlite3_close(db)
    Ptr.free(dbcell)
    return 0
}
WYN

out="$("$WYN_BIN" build app.wyn >/dev/null 2>&1 && ./app 2>&1)"
expected="row 10 ten
row 20 twenty
row 30 thirty
total 60"
if [ "$out" != "$expected" ]; then
  echo "sqlite: FAIL"
  echo "--- expected ---"; echo "$expected"
  echo "--- got ---"; echo "$out"
  exit 1
fi
echo "sqlite: PASS"
