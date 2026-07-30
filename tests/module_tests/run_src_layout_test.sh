#!/usr/bin/env bash
# `import <name>` must find ./src/<name>.wyn.
#
# WHY THIS LAYOUT MATTERS. `wyn init --template lib` puts library code in src/,
# and `wyn test` compiles each test file individually (src/cmd_test.c shells out
# to `wyn build <file>`), so source_directory is always tests/. The resolver
# tried the source file's dir, its parent, modules/, cwd, ./modules/,
# ./wyn_modules/, the git-dep cache and ./packages/ - but never ./src/. So the
# one layout the tooling itself scaffolds could not be imported from a test.
#
# The resolver ALREADY searches src/ for git dependencies
# (`<cache>/src/<name>.wyn`, module.c), so this is a consistency fix: the local
# project gets the same treatment a fetched dependency already gets.
#
# Real-world cost before the fix: a downstream project (Wynshop) carried five
# committed root symlinks - ./pixel.wyn -> src/pixel.wyn and four more - purely
# to satisfy the cwd candidate. Every new module needed another one.
set -u
WYN_BIN="${WYN:-./wyn}"
case "$WYN_BIN" in /*) ;; *) WYN_BIN="$(pwd)/$WYN_BIN" ;; esac
ROOT="$(pwd)"

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cd "$work"
export WYN_ROOT="$ROOT"

fails=0

printf '[project]\nname = "srclayout"\nversion = "0.1.0"\n' > wyn.toml
mkdir -p src tests

cat > src/mathx.wyn <<'WYN'
pub fn triple(n: int) -> int { return n * 3 }
WYN

# 1. import from a test file: source_directory is tests/, so only a ./src/
#    candidate can resolve this.
cat > tests/test_from_test.wyn <<'WYN'
import mathx
test "import finds src/mathx.wyn" {
    assert_eq(mathx.triple(5), 15)
}
WYN

out="$("$WYN_BIN" test 2>&1)"
if printf '%s' "$out" | grep -q "1 passed, 0 failed"; then
    echo "src-layout: ok    import from tests/ resolves ./src/"
else
    echo "src-layout: FAIL  import from tests/ did not resolve ./src/"
    printf '%s\n' "$out" | grep -iE "not found|failed" | head -3 | sed 's/^/            /'
    fails=$((fails+1))
fi

# 2. module -> module import, both living in src/. Guards the case where the
#    importer is itself in src/ (source_directory == src/, so candidate 1 would
#    already cover it) - kept so a future refactor cannot regress only one path.
cat > src/wrapper.wyn <<'WYN'
import mathx
pub fn nine() -> int { return mathx.triple(3) }
WYN

cat > tests/test_chain.wyn <<'WYN'
import wrapper
test "src module imports another src module" {
    assert_eq(wrapper.nine(), 9)
}
WYN

out="$("$WYN_BIN" test 2>&1)"
if printf '%s' "$out" | grep -q "2 passed, 0 failed"; then
    echo "src-layout: ok    src module -> src module import resolves"
else
    echo "src-layout: FAIL  src module -> src module import failed"
    printf '%s\n' "$out" | grep -iE "not found|failed" | head -3 | sed 's/^/            /'
    fails=$((fails+1))
fi

# 3. A genuinely absent module must still fail, so a resolver change that
#    accidentally matched everything cannot pass tests 1-2 unnoticed.
#
#    The absent module must actually be USED. An unused `import` of a missing
#    module is silently ignored - verified identical on the shipped v1.20.0 and
#    on this build, so that permissiveness is pre-existing and orthogonal to the
#    ./src/ candidate. An earlier draft of this test imported without using, and
#    it "failed" against a working fix - the test was wrong, not the compiler.
rm -f tests/test_from_test.wyn tests/test_chain.wyn
cat > tests/test_absent.wyn <<'WYN'
import definitely_not_a_real_module_xyz
test "a missing module that is used must fail the build" {
    assert_eq(definitely_not_a_real_module_xyz.f(1), 1)
}
WYN

out="$("$WYN_BIN" test 2>&1)"
if printf '%s' "$out" | grep -qi "not found"; then
    echo "src-layout: ok    a used-but-missing module still reports 'not found'"
else
    echo "src-layout: FAIL  a used-but-missing module did NOT error - resolver too permissive"
    fails=$((fails+1))
fi

if [ "$fails" -eq 0 ]; then
    echo "src-layout: 3 pass, 0 fail"
    exit 0
fi
echo "src-layout: $fails fail"
exit 1
