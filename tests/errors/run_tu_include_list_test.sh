#!/bin/bash
# The Makefile's TU_INCLUDED_SRCS must equal the set of files that src/ actually
# `#include`s as C source. Both directions matter, and both have a failure mode.
#
# A MISSING ENTRY makes `make` lie. src/codegen.c:2820 does `#include "codegen_gpu.c"`,
# but src/codegen_gpu.c was in neither CORE_SRCS nor TU_INCLUDED_SRCS for its entire
# life. A #included .c file is not a prerequisite of anything, so:
#
#   $ touch src/codegen_gpu.c
#   $ make -q wyn ; echo $?
#   0                      # "nothing to do" - and `make` then reports SUCCESS
#                          # while every test runs the OLD binary
#
# That is the worst shape of build bug available: a green gate over code that was never
# compiled. `$(wildcard src/*.h)` on the `wyn:` rule does not help - these are .c files.
#
# A STALE ENTRY is the quieter half: a file no longer #included anywhere but still listed
# stays a prerequisite forever, so an unrelated edit to a dead file keeps relinking the
# compiler, and the list stops describing the build.
#
# So this gate does not carry a copy of the list. It DERIVES the list from the `#include
# "<name>.c"` sites themselves and compares that against TU_INCLUDED_SRCS as parsed out of
# the Makefile. The only authority is the source.
#
# THE DISCOVERY REGEX IS ITSELF UNDER TEST, and that is not belt-and-braces. The first
# version of this gate anchored the pattern with `[ \t]*$` right after the closing quote,
# which meant
#
#   #include "codegen_newthing.c"   // the new one
#
# was INVISIBLE to it: the file was #included, absent from TU_INCLUDED_SRCS, and the gate
# reported `3 pass, 0 fail`. A derived-list gate is only as good as its discovery, so the
# REGEX SELF-TEST arm below asserts, spelling by spelling, that each form a human would
# actually type is matched - and that a commented-out include is not. Re-anchoring the
# pattern now reds a named arm instead of quietly shrinking what the gate can see.
#
# THE COUNT FLOOR IS NOT DECORATION EITHER, but it is a narrower net than it looks. Any
# partial thinning of the regex already reds the set-difference arm (the surviving
# TU_INCLUDED_SRCS entries become "stale"). The one hole only the count can close is both
# sets emptying together - a dead regex AND an emptied list - which would otherwise read as
# "the 0 #included source files match exactly". MIN_SITES is a FLOOR, not an equality: an
# equality reds on a correct future change (add a seventh file AND list it), which is a
# chore that teaches the next reader to treat this gate's red as noise.
#
# Zero compiles, no wyn binary, no perl: this is text analysis and runs in well under a
# second. That is why it is enrolled in BOTH rosters - the Makefile `test:` target and the
# hand-maintained "portable subset of make test (Windows)" step in ci.yml. The list it
# guards is platform-independent, so a Windows-only contributor must not be the one person
# the gate cannot see. (2026-10)
set -uo pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)

# Fail loudly if the interpreter is missing rather than skipping quietly - a gate that
# can be absent is a gate that is absent. (`python3` is already proven present on the
# Windows runner: ci.yml drives tests/lsp/lsp_client.py with it in a `shell: bash` step.)
command -v python3 >/dev/null 2>&1 || {
    echo "  FAIL  python3 is not on PATH - this gate cannot run"
    echo ""
    echo "tu-include-list: 0 pass, 1 fail"
    exit 1
}

# cd first and let Python take the root from its own cwd. Passing $ROOT as an argv
# would hand a native Windows python.exe an MSYS path (/d/a/wyn/wyn) under Git Bash
# and rely on the runtime's argument mangling to undo it.
cd "$ROOT" || { echo "cannot cd to $ROOT"; exit 1; }

python3 - <<'PY'
import os, re, sys

root = os.getcwd()

# A FLOOR, deliberately not an equality. Six `#include "<file>.c"` sites exist today:
# five in src/codegen.c (codegen_gpu, codegen_expr, codegen_stmt, codegen_lambda,
# codegen_program) and one in src/checker.c (checker_builtins). The floor sits just
# below that so a correct ADDITION (seventh file, listed in TU_INCLUDED_SRCS) stays
# green and so does the removal of one file, while a regex that loses two or more
# sites - or dies entirely next to an emptied list - reds loudly.
MIN_SITES = 5

PASS = FAIL = 0
def ok(m):
    global PASS; PASS += 1; print(f"  ok    {m}")
def bad(m):
    global FAIL; FAIL += 1; print(f"  FAIL  {m}")

# ---------------------------------------------------------------- derive from the source
# NOTE ON THE TAIL: `[ \t]*(?://|/\*|$)` and NOT `[ \t]*$`. The end-of-line anchor made
# every include carrying a trailing comment invisible (see the header). Keep a tail guard
# so `#include "x.c" something_else` is still rejected, but let a C comment follow.
INC = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"\n]+\.c)"[ \t]*(?://|/\*|$)', re.M)

# ------------------------------------------------------------------ REGEX SELF-TEST arm
# Every spelling a human would plausibly type, asserted by PRESENCE of a match on a
# controlled input. If someone re-anchors INC, the arm that reds names the spelling that
# stopped being seen. The negative case is last and cannot carry the arm on its own: a
# regex that matches nothing would also pass it, which is what the positive cases are for.
SPELLINGS = [
    ('plain',                      '#include "a.c"\n',                      'a.c'),
    ('no trailing newline',        '#include "a.c"',                        'a.c'),
    ('leading whitespace',         '\t  #include "a.c"\n',                  'a.c'),
    ('space after hash',           '#  include "a.c"\n',                    'a.c'),
    ('trailing // comment',        '#include "a.c"  // why\n',              'a.c'),
    ('trailing /* comment */',     '#include "a.c" /* why */\n',            'a.c'),
    ('trailing tab then comment',  '#include "a.c"\t// why\n',              'a.c'),
    ('subdirectory path',          '#include "sub/a.c"\n',                  'sub/a.c'),
    ('mid-file, not first line',   'int x;\n#include "a.c"\n int y;\n',     'a.c'),
]
for label, text, expected in SPELLINGS:
    got = INC.findall(text)
    if got == [expected]:
        ok(f"discovery regex sees the `{label}` spelling")
    else:
        bad(f"discovery regex does NOT see the `{label}` spelling "
            f"(matched {got!r}, expected [{expected!r}]) - the gate is blind to any "
            f"TU-included file written this way, which is exactly the silent-stale-binary "
            f"hole it exists to close. Do not narrow INC.")

commented_out = '// #include "a.c"\n'
if INC.findall(commented_out) == []:
    ok("discovery regex ignores a commented-out include")
else:
    bad("discovery regex matches a commented-out `// #include` line - it will demand a "
        "Makefile prerequisite for a file nothing includes")

# --------------------------------------------------------------- walk src/ for real sites
sites = []           # (including file rel path, line no, resolved rel path of included file)
for dirpath, _dirs, files in os.walk('src'):
    for name in sorted(files):
        if not name.endswith(('.c', '.h', '.m')):
            continue
        path = os.path.join(dirpath, name)
        # Forward slashes everywhere: os.walk/normpath yield `src\codegen.c` on Windows,
        # and the Makefile spells every entry `src/codegen.c`, so without this the set
        # difference would report all six files missing AND all six stale on the Windows
        # roster - a gate that is red for a reason that has nothing to do with the build.
        rel = path.replace(os.sep, '/')
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError as e:
            bad(f"cannot read {rel}: {e} - this gate is vacuous without it")
            continue
        for m in INC.finditer(text):
            lineno = text.count('\n', 0, m.start()) + 1
            # An #include is resolved relative to the including file's own directory
            # (and -I src, which is the same directory for every site here).
            target = os.path.normpath(os.path.join(os.path.dirname(path), m.group(1)))
            sites.append((rel, lineno, target.replace(os.sep, '/')))

for rel, lineno, target in sites:
    print(f"  site  {rel}:{lineno} -> {target}")

if len(sites) >= MIN_SITES:
    ok(f"discovered {len(sites)} `#include \"*.c\"` sites in src/ (floor is {MIN_SITES})")
else:
    bad(f"discovered only {len(sites)} `#include \"*.c\"` sites in src/, floor is "
        f"{MIN_SITES} - either this gate's discovery regex has gone thin (in which case it "
        f"is comparing two near-empty sets and cannot fail) or TU-included files were "
        f"genuinely removed, in which case lower MIN_SITES in the same change")

derived = {t for _f, _l, t in sites}
for target in sorted(derived):
    if not os.path.exists(target):
        bad(f"{target} is #included but does not exist on disk - the include name is "
            f"wrong, or this gate resolved the path wrongly")

# ------------------------------------------------------- parse TU_INCLUDED_SRCS, verbatim
mk = open('Makefile', encoding='utf-8').read()

# Fold `\<newline>` continuations away FIRST, so the assignment is one physical line.
# Matching continuations inside the capture instead needs a non-greedy head and gets this
# wrong in a way that still "passes" (it captured a lone `\` as a filename), which is the
# same class of bug this gate exists to catch - so it is done the boring way.
mk_joined = re.sub(r'\\\n', ' ', mk)
ASSIGN = re.compile(r'^[ \t]*TU_INCLUDED_SRCS[ \t]*[:+?]?=(.*)$', re.M)
assigns = ASSIGN.findall(mk_joined)
if len(assigns) == 1:
    ok("Makefile defines TU_INCLUDED_SRCS exactly once")
else:
    bad(f"Makefile has {len(assigns)} TU_INCLUDED_SRCS assignments, expected 1 - with "
        f"more than one (or none) this gate is parsing the wrong thing")

listed = set()
for body in assigns:
    listed |= set(body.split())

for entry in sorted(listed):
    print(f"  listed {entry}")

# ----------------------------------------------------------------- the set difference, both ways
missing = sorted(derived - listed)     # #included but not a prerequisite -> stale binary
stale   = sorted(listed - derived)     # a prerequisite for nothing -> the list has rotted

for e in missing:
    bad(f"{e} is #included as C source but is NOT in TU_INCLUDED_SRCS (Makefile) - "
        f"editing it leaves `make` believing wyn is up to date and reporting success "
        f"over the old binary. Add it to TU_INCLUDED_SRCS.")
for e in stale:
    bad(f"{e} is in TU_INCLUDED_SRCS but no `#include \"...\"` site in src/ pulls it in "
        f"any more - remove it, or restore the include it is standing in for.")

if not missing and not stale:
    ok(f"TU_INCLUDED_SRCS matches the {len(derived)} #included source files exactly")

print("")
print(f"tu-include-list: {PASS} pass, {FAIL} fail")
sys.exit(1 if FAIL else 0)
PY
