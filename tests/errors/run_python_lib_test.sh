#!/bin/bash
# `wyn build x.wyn --python` produces a LOADABLE library, not an executable (2026-08).
#
# The documented quickstart (site/docs/guides/python-libraries.md), and the instructions
# `wyn init --lib python` prints, both said:
#
#     wyn build mathlib.wyn --python
#     python3 -c "import mathlib; print(mathlib.add(2, 3))"
#
# and it could not work. `build` printed a SUCCESS line -
#
#     ✓ Built: mathlib (51KB, 336ms)
#
# - and produced an ordinary executable. No libmathlib.dylib, no mathlib.py, exit 0.
# Python then failed with ModuleNotFoundError. Three independent causes:
#
# 1. `build` parsed --python into a string used only for the progress line and a TCC
#    gate. The real shared-library + wrapper generator lived inline under `run`, so
#    `build` could not reach it. --shared and --node were dead the same way.
#
# 2. `run --python` ALSO did nothing. wyn's flags are split positionally at the file
#    name, so in `wyn run mathlib.wyn --python` the flag lands in the PROGRAM's argv and
#    the flag scan's window was empty. --node additionally collapsed to --shared, so it
#    never wrote a .js wrapper.
#
# 3. Even with a library built by hand, the symbols were NOT EXPORTED. Codegen emits a
#    user function as `__attribute__((hot)) static inline` when its body is <= 5
#    statements (and, separately, when it is recursive) - a speed heuristic with no
#    notion of library mode. `static` means internal linkage, so the function is absent
#    from the .dylib and ctypes cannot find it. BOTH functions in the guide (`add`,
#    `factorial`) are under the threshold and `factorial` is recursive too, so the
#    guide's own examples were the worst case.
#
# NO TEST COVERED --python / --shared / --node ANYWHERE, which is why a headline,
# documented, scaffolded feature shipped completely non-functional. This is that test.
# It asserts the ARTIFACTS exist, that the symbols are really exported, and - where a
# python3 is available - that the wrapper actually imports and returns right answers.
set -uo pipefail
WYN="${WYN:-./wyn}"
WYN_ABS="$(cd "$(dirname "$WYN")" && pwd)/$(basename "$WYN")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

case "$(uname -s)" in
  Darwin) LIBEXT=dylib ;;
  MINGW*|MSYS*|CYGWIN*) LIBEXT=dll ;;
  *) LIBEXT=so ;;
esac

# The guide's own example, verbatim - both functions small enough to have been inlined
# away, and factorial recursive as well.
cat > "$TMP/mathlib.wyn" <<'EOF'
fn add(a: int, b: int) -> int {
    return a + b
}

fn factorial(n: int) -> int {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

fn describe(name: string) -> string {
    return "hello " + name
}
EOF

# ---- 1: build --python produces BOTH artifacts -----------------------------

out=$(cd "$TMP" && "$WYN_ABS" build mathlib.wyn --python 2>&1); code=$?
if [ $code -eq 0 ] && [ -f "$TMP/libmathlib.$LIBEXT" ]; then
  ok "build --python produces libmathlib.$LIBEXT"
else
  bad "no shared library"; printf '%s\n' "$out" | head -4 | sed 's/^/        /'
fi
if [ -f "$TMP/mathlib.py" ]; then
  ok "build --python produces the mathlib.py wrapper"
else
  bad "no .py wrapper - build fell through to the executable path again"
fi
# The exact wrong outcome: an ordinary executable and a misleading success line.
if [ -f "$TMP/mathlib" ] && [ ! -f "$TMP/libmathlib.$LIBEXT" ]; then
  bad "built a plain executable and called it success"
else
  ok "did not silently build an executable instead"
fi

# ---- 2: the symbols are actually EXPORTED ---------------------------------
# This is the check that would have caught cause 3 on its own: the library can exist
# and still be useless.
if command -v nm >/dev/null 2>&1 && [ -f "$TMP/libmathlib.$LIBEXT" ]; then
  # Extract the NAMES of defined-text symbols and match them exactly.
  #
  # Was `grep -qE "[0-9a-f]+ +T _?name$"` over the whole nm output, which was wrong twice:
  # `grep -q` exits at the first match and closes the pipe, so the feeding printf died with
  # "write error: Broken pipe" (visible in CI), and the address+column shape differs between
  # BSD nm and GNU binutils nm. Library mode also links the whole runtime in now, so the
  # symbol table is large and loose matching is risky. awk on the type column, then an
  # anchored compare, is portable and cannot half-match.
  syms=$(nm -g "$TMP/libmathlib.$LIBEXT" 2>/dev/null | awk '$2=="T"{print $3}' || true)
  missing=""
  # STILL a pipe race, even after the awk rewrite above: `printf | grep -q` lets grep exit
  # at the first match and close the pipe, so printf can die on EPIPE before finishing and
  # the pipeline reports failure for a symbol that IS present. Seen on macos-15-intel only
  # (CI job 92644637676):
  #     run_python_lib_test.sh: line 104: printf: write error: Broken pipe
  #     FAIL  NOT EXPORTED: describe - static inline hid them again
  # while the very NEXT case in the same run - python3 importing the wrapper and calling
  # describe - passed, which is what proves the export was fine and the check was broken.
  # Library mode links the whole runtime, so the table is large enough to make this likely.
  #
  # A case glob does the same anchored match with no pipe and no subprocess. Verified by
  # mutation that it still catches a genuinely absent symbol.
  for f in add factorial describe; do
    case "
$syms" in
      *"
$f"*|*"
_$f"*) ;;
      *) missing="$missing $f" ;;
    esac
  done
  if [ -z "$missing" ]; then
    ok "add, factorial and describe are all exported (T) from the library"
  else
    bad "NOT EXPORTED:$missing - static inline hid them again"
  fi
else
  echo "  skip  nm unavailable"
fi

# ---- 3: python can import it and the answers are right --------------------
if command -v python3 >/dev/null 2>&1 && [ -f "$TMP/mathlib.py" ]; then
  pyout=$(cd "$TMP" && python3 -c "
import mathlib
print('add', mathlib.add(2, 3))
print('fact', mathlib.factorial(10))
print('neg', mathlib.add(-5, 5))
print('str', mathlib.describe('wyn'))
" 2>&1); pyrc=$?
  if [ $pyrc -eq 0 ] &&
     printf '%s' "$pyout" | grep -q '^add 5$' &&
     printf '%s' "$pyout" | grep -q '^fact 3628800$' &&
     printf '%s' "$pyout" | grep -q '^neg 0$'; then
    ok "python3 imports the wrapper and every result is correct"
  else
    bad "the documented quickstart still fails"; printf '%s\n' "$pyout" | head -5 | sed 's/^/        /'
  fi
  if printf '%s' "$pyout" | grep -q 'ModuleNotFoundError'; then
    bad "ModuleNotFoundError - the wrapper was not importable"
  else
    ok "no ModuleNotFoundError"
  fi
else
  echo "  skip  python3 unavailable"
fi

# ---- 4: --shared and --node, which were broken the same way ---------------
rm -f "$TMP"/libmathlib.* "$TMP"/mathlib.py "$TMP"/mathlib.js "$TMP"/mathlib 2>/dev/null
out=$(cd "$TMP" && "$WYN_ABS" build mathlib.wyn --shared 2>&1); code=$?
if [ $code -eq 0 ] && [ -f "$TMP/libmathlib.$LIBEXT" ] && [ ! -f "$TMP/mathlib.py" ]; then
  ok "build --shared produces the library and NO python wrapper"
else
  bad "--shared wrong"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

rm -f "$TMP"/libmathlib.* "$TMP"/mathlib.js "$TMP"/mathlib 2>/dev/null
out=$(cd "$TMP" && "$WYN_ABS" build mathlib.wyn --node 2>&1); code=$?
if [ $code -eq 0 ] && [ -f "$TMP/libmathlib.$LIBEXT" ] && [ -f "$TMP/mathlib.js" ]; then
  ok "build --node produces the library AND a .js wrapper"
else
  bad "--node wrong (it used to collapse to --shared and emit no .js)"
  printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- 5: `run` accepts them too, after the file --------------------------
# The positional flag split is what made this spelling silently do nothing.
rm -f "$TMP"/libmathlib.* "$TMP"/mathlib.py "$TMP"/mathlib.js "$TMP"/mathlib 2>/dev/null
out=$(cd "$TMP" && "$WYN_ABS" run mathlib.wyn --python 2>&1); code=$?
if [ $code -eq 0 ] && [ -f "$TMP/libmathlib.$LIBEXT" ] && [ -f "$TMP/mathlib.py" ]; then
  ok "run --python (flag AFTER the file) also builds the library + wrapper"
else
  bad "run --python still ignores the flag"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi

# ---- 6: the SCAFFOLDED path, which is what a new user actually types ------
# `wyn init --lib python` prints exactly:
#     cd mylib && wyn build --python
#     python3 -c "from mylib import add; print(add(2, 3))"
# Two further bugs lived here, both invisible until --python worked at all:
#   * `wyn build --python` (no file) has argc==3, so the "no file argument" branch
#     never ran and the FLAG was taken as the file/dir.
#   * the artifact was named from the ENTRY FILE (libmain.dylib + main.py) while the
#     scaffold and wyn.toml both say `mylib`, so `from mylib import ...` still raised
#     ModuleNotFoundError.
mkdir -p "$TMP/proj/src"
cat > "$TMP/proj/wyn.toml" <<'EOF'
[project]
name = "mylib"
version = "0.1.0"
entry = "src/main.wyn"
EOF
cat > "$TMP/proj/src/main.wyn" <<'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}
EOF
out=$(cd "$TMP/proj" && "$WYN_ABS" build --python 2>&1); code=$?
if [ $code -eq 0 ] && [ -f "$TMP/proj/libmylib.$LIBEXT" ] && [ -f "$TMP/proj/mylib.py" ]; then
  ok "flag-only 'wyn build --python' names artifacts from wyn.toml, not the entry file"
else
  bad "scaffolded path wrong (looked for libmylib.$LIBEXT + mylib.py)"
  ls "$TMP/proj" 2>/dev/null | sed 's/^/        /' | head -6
fi
if [ -f "$TMP/proj/libmain.$LIBEXT" ] || [ -f "$TMP/proj/main.py" ]; then
  bad "named from the entry FILE again - 'from mylib import ...' would still fail"
else
  ok "no main.py / libmain.* leftover from file-based naming"
fi
if command -v python3 >/dev/null 2>&1 && [ -f "$TMP/proj/mylib.py" ]; then
  pyout=$(cd "$TMP/proj" && python3 -c "from mylib import add; print(add(2, 3))" 2>&1)
  if printf '%s' "$pyout" | grep -q '^5$'; then
    ok "the scaffold's own instruction ('from mylib import add') works verbatim"
  else
    bad "scaffold instruction still fails"; printf '%s\n' "$pyout" | head -3 | sed 's/^/        /'
  fi
fi

# ---- 6b: a hyphenated project name must still be importable ---------------
# A project name is free text and hyphens are idiomatic ("math-lib"), but
# `from math-lib import add` is a Python SYNTAX ERROR, so naming the wrapper straight
# from wyn.toml produced a file nobody could import. sample-apps/python/math-lib is
# exactly this case.
mkdir -p "$TMP/hyph/src"
cat > "$TMP/hyph/wyn.toml" <<'EOF'
[project]
name = "math-lib"
version = "0.1.0"
entry = "src/main.wyn"
EOF
cat > "$TMP/hyph/src/main.wyn" <<'EOF'
pub fn add(a: int, b: int) -> int {
    return a + b
}
EOF
out=$(cd "$TMP/hyph" && "$WYN_ABS" build --python 2>&1); code=$?
if [ $code -eq 0 ] && [ -f "$TMP/hyph/math_lib.py" ] && [ -f "$TMP/hyph/libmath_lib.$LIBEXT" ]; then
  ok "a hyphenated project name is sanitized to an importable identifier"
else
  bad "expected math_lib.py + libmath_lib.$LIBEXT"; ls "$TMP/hyph" | sed 's/^/        /' | head -5
fi
if [ -f "$TMP/hyph/math-lib.py" ]; then
  bad "wrote math-lib.py, which Python cannot import"
else
  ok "no un-importable hyphenated wrapper"
fi
if command -v python3 >/dev/null 2>&1 && [ -f "$TMP/hyph/math_lib.py" ]; then
  pyout=$(cd "$TMP/hyph" && python3 -c "from math_lib import add; print(add(2, 3))" 2>&1)
  if printf '%s' "$pyout" | grep -q '^5$'; then
    ok "and the sanitized module imports and works"
  else
    bad "sanitized module does not import"; printf '%s\n' "$pyout" | head -3 | sed 's/^/        /'
  fi
fi

# ---- 7: and an ORDINARY build is unchanged ------------------------------
# Suppressing static inline must apply ONLY in library mode - it is a real speed
# heuristic for executables. This is the control group for the codegen change.
cat > "$TMP/prog.wyn" <<'EOF'
fn double(n: int) -> int {
    return n * 2
}

fn main() {
    print("double ${double(21)}")
}
EOF
out=$(cd "$TMP" && "$WYN_ABS" build prog.wyn --debug 2>&1); code=$?
if [ $code -eq 0 ] && [ -f "$TMP/prog" ] && [ ! -f "$TMP/libprog.$LIBEXT" ]; then
  ok "a normal build still produces an executable and no library"
else
  bad "normal build changed"; printf '%s\n' "$out" | head -3 | sed 's/^/        /'
fi
if [ -f "$TMP/prog.wyn.c" ] && grep -q 'static inline' "$TMP/prog.wyn.c"; then
  ok "the static-inline speed heuristic is PRESERVED for executables"
else
  bad "inlining was suppressed for a normal build - the perf heuristic was lost"
fi
run_out=$(cd "$TMP" && ./prog 2>&1)
if printf '%s' "$run_out" | grep -q 'double 42'; then
  ok "and the executable still runs correctly"
else
  bad "executable broke"; printf '%s\n' "$run_out" | head -3 | sed 's/^/        /'
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
  echo "python-lib: $PASS pass, 0 fail"
  exit 0
fi
echo "python-lib: $PASS pass, $FAIL fail"
exit 1
