#!/bin/bash
# Every hand-maintained list of runtime source files must name a file that EXISTS.
#
# WHY THIS GATE EXISTS, precisely. Deleting one runtime .c file means editing FIFTEEN
# separate hand-maintained lists. Counted, not estimated - these are the lists that named
# src/net.c, src/net_runtime.c or src/arc_runtime.c on dev @ ecac48e5:
#
#   Makefile (6)      CORE_SRCS; the wyn-windows, wyn-linux and wyn-macos prerequisite
#                     lines (three separate lists); RT_SRCS; TCC_RT_SRCS
#   src/main.c (6)    wyn_runtime_sources[] (the one whose comment calls it "single source
#                     of truth"); the inline cross-compile runtime string for linux; the
#                     iOS clang line; rt_srcs[]; rt_srcs2[]; win_srcs[]
#   src/cmd_compile.c the gcc link line used when runtime/libwyn_rt.a is absent
#   src/tcc_backend.c srcs[] for the --fast path
#   scripts/build-tcc.sh  the loop that builds libwyn_rt_tcc.a
#
# wasm_srcs[] in src/main.c is a sixteenth list of the same kind that happened not to name
# any of the three, and TEN now-deleted Makefile test targets named src/arc_runtime.c as
# well. This gate guards all of them, including wasm_srcs[].
#
# and they do NOT agree with each other - some files are in a few of them and not the
# rest. The dangerous half is that MOST OF THESE FAIL SILENTLY. The three cross-compile
# paths in main.c pass their command to system() WITHOUT CHECKING THE RETURN VALUE and
# send the compiler's stderr to /dev/null; scripts/build-tcc.sh ends its per-file compile
# with `|| true`. So a name that no longer has a file behind it does not produce an error
# anywhere - it produces an archive quietly missing an object, or a link that drops a
# translation unit, on a platform the author was not building.
#
# Running those paths needs zig / emcc / an iOS SDK, so no local gate can execute them,
# and the repo rule is one cross-compiling agent at a time. But EXISTENCE needs no
# toolchain: it is text against the filesystem, it costs well under a second, and it
# catches the exact failure mode. CI's `Cross-compile (iOS)` job remains the gate for the
# iOS path actually WORKING (it runs `wyn cross ios` and greps `file` for arm64);
# `Cross-compile (Android)` cannot red because it ends in `|| echo skipped`, and NO CI job
# runs the linux, windows or wasm cross paths at all. This gate is what covers those.
#
# ASSERTS PRESENCE AND A FLOOR, never an absence and never an equality. The floors exist
# because the two discovery passes are the part that can rot: if a regex goes thin it
# compares near-empty sets and reports success, which is the shape of green this repo has
# been burned by. A floor sits BELOW today's count so that correctly removing a file stays
# green while a dead regex reds. The NEGATIVE CONTROL arms prove each pass can actually
# see a missing file before any green from it is worth anything.
#
# Zero compiles, no wyn binary: enrolled in BOTH the Makefile `test:` roster and the
# hand-maintained "portable subset of make test (Windows)" step in ci.yml, because the
# lists it guards are platform-independent and several of them exist ONLY for Windows and
# wasm - the targets a macOS or Linux contributor never builds. (2026-10)
set -uo pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)

command -v python3 >/dev/null 2>&1 || {
    echo "  FAIL  python3 is not on PATH - this gate cannot run"
    echo ""
    echo "runtime-source-lists: 0 pass, 1 fail"
    exit 1
}

cd "$ROOT" || { echo "cannot cd to $ROOT"; exit 1; }

python3 - <<'PY'
import os, re, sys, glob

PASS = FAIL = 0
def ok(m):
    global PASS; PASS += 1; print(f"  ok    {m}")
def bad(m):
    global FAIL; FAIL += 1; print(f"  FAIL  {m}")

# Floors, deliberately below today's counts (73 distinct path names, 5 bare-name arrays).
MIN_PATH_NAMES = 60
MIN_ARRAYS     = 4
MIN_ARRAY_ELEMS = 15

# `%s/src/<name>.c` is AMBIGUOUS: usually the %s is wyn_root, but `wyn add sqlite` copies
# a vendored amalgamation out of a PACKAGE's own src/ with the same spelling
# (main.c's install_cmd). Each entry here must NOT exist under src/ - a member that
# becomes a real compiler source is reported as a stale exclusion, so this cannot quietly
# grow into a way of silencing the gate.
NOT_COMPILER_SOURCES = {'sqlite3'}

def strip_c_comments(t):
    """Remove C comments, leaving string and char literals intact.

    This is a state machine and not two re.sub calls, and that is not fastidiousness.
    `re.sub(r'//[^\n]*','',t)` deletes from the `//` of a URL inside a string literal
    ("https://...") to end of line, which takes the closing quote and any `};` with it.
    On src/main.c that corrupted the text enough that the PASS B array regex matched one
    list instead of five, and PASS B reported ok on the single survivor - a gate quietly
    losing 80% of its coverage while printing green. Newlines are preserved so that the
    line numbers this gate prints still point at the real source line.
    """
    out = []
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == '"' or c == "'":
            q = c
            out.append(c); i += 1
            while i < n:
                if t[i] == '\\' and i + 1 < n:
                    out.append(t[i:i+2]); i += 2; continue
                out.append(t[i])
                if t[i] == q:
                    i += 1; break
                if t[i] == '\n':          # unterminated literal: do not run away
                    i += 1; break
                i += 1
            continue
        if c == '/' and i + 1 < n and t[i+1] == '/':
            while i < n and t[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and t[i+1] == '*':
            i += 2
            while i + 1 < n and not (t[i] == '*' and t[i+1] == '/'):
                if t[i] == '\n':
                    out.append('\n')
                i += 1
            i += 2
            continue
        out.append(c); i += 1
    return ''.join(out)

def strip_hash_comments(t):
    return re.sub(r'(?m)^\s*@?#[^\n]*', '', t)

# ------------------------------------------------------------------ PASS A: path spellings
# Matches `src/foo.c`, `%s/src/foo.c`, `$(VAR)/src/foo.c`. The leading alternation keeps
# `./packages/sqlite/src/sqlite3.c` and other longer paths from matching as a bare
# `src/...`: the character before must be start-of-line, whitespace, a quote, `=` or `:`.
PATH_RE = re.compile(
    r'''(?:^|[\s"'=:])(?:%s/|\$\([A-Za-z_]+\)/)?src/([A-Za-z0-9_]+)\.c(?![A-Za-z0-9_])''',
    re.M)

# --------------------------------- PASS A regex self-test, by PRESENCE on controlled input
SPELLINGS = [
    ('bare, start of line',        'src/foo.c\n',                       ['foo']),
    ('bare after whitespace',      'CORE_SRCS = src/foo.c src/bar.c\n',  ['foo', 'bar']),
    ('%s-prefixed in a C string',  '"%s/src/foo.c "\n',                 ['foo']),
    ('$(VAR)-prefixed',            '\t$(CC) $(ROOT)/src/foo.c\n',       ['foo']),
    ('after a colon (make dep)',   'wyn-linux: src/foo.c\n',            ['foo']),
    ('two on one line',            ' src/foo.c src/foo_bar.c\n',        ['foo', 'foo_bar']),
]
for label, text, expected in SPELLINGS:
    got = PATH_RE.findall(text)
    if got == expected:
        ok(f"path regex sees the `{label}` spelling")
    else:
        bad(f"path regex does NOT see the `{label}` spelling (matched {got!r}, expected "
            f"{expected!r}) - every list written this way becomes invisible to this gate. "
            f"Do not narrow PATH_RE.")

COMMENT_CASES = [
    ('a // comment',                 'int x; // src/ghost.c\n',                 'int x; \n'),
    ('a /* */ comment',              'int x; /* src/ghost.c */ int y;\n',       'int x;  int y;\n'),
    ('// INSIDE a string literal',   'f("https://x/src/keep.c");\n',            'f("https://x/src/keep.c");\n'),
    ('/* inside a string literal',   'f("/*src/keep.c*/");\n',                  'f("/*src/keep.c*/");\n'),
    ('an escaped quote in a string', 'f("a\\"b src/keep.c");\n',                'f("a\\"b src/keep.c");\n'),
]
for label, src_text, expected in COMMENT_CASES:
    got = strip_c_comments(src_text)
    if got == expected:
        ok(f"comment stripper handles {label}")
    else:
        bad(f"comment stripper mishandles {label}: {got!r} != {expected!r} - if it eats a "
            f"string literal it destroys the brace structure PASS B depends on, and PASS "
            f"B then silently guards fewer lists than it reports")

for label, text in [('a longer package path', ' ./packages/sqlite/src/sqlite3.c\n'),
                    ('a .h, not a .c',        ' src/foo.h\n'),
                    ('a longer identifier',   ' src/foo.cpp\n')]:
    if PATH_RE.findall(text) == []:
        ok(f"path regex ignores {label}")
    else:
        bad(f"path regex matches {label} ({PATH_RE.findall(text)!r}) - it will demand a "
            f"compiler source that was never meant to be one")

SCAN = ['Makefile'] + sorted(glob.glob('scripts/*.sh')) + sorted(glob.glob('src/*.c'))

# A file naming five or more compiler sources is MAINTAINING A LIST; a file naming one is
# making a REFERENCE, and references are a different problem with a different owner. The
# threshold is here because the first version of this gate scanned every scripts/*.sh
# wholesale and red on `grep -q ... src/llvm_codegen.c` in scripts/integration_gates.sh -
# a real defect (there is no LLVM backend in this tree, so that whole Day-5 stanza can
# only fail, and `make phase2-gates` is already red on dev because of it) but NOT this
# gate's subject, and not something to fix inside a runtime-deletion change.
# Skipped files are PRINTED, not silently dropped, so the scope is auditable.
MIN_NAMES_PER_FILE = 5

names = {}                              # name -> set of files that spell it
per_file, skipped = {}, []
for f in SCAN:
    try:
        raw = open(f, encoding='utf-8', errors='replace').read()
    except OSError as e:
        bad(f"cannot read {f}: {e} - this gate is vacuous without it")
        continue
    text = strip_c_comments(raw) if f.endswith('.c') else strip_hash_comments(raw)
    found = {m.group(1) for m in PATH_RE.finditer(text)}
    if not found:
        continue
    if len(found) < MIN_NAMES_PER_FILE:
        skipped.append((f, sorted(found)))
        continue
    per_file[f] = found
    for name in found:
        names.setdefault(name, set()).add(f)

for f, found in skipped:
    print(f"  skip  {f} names only {len(found)} compiler source(s) {found} - under the "
          f"list threshold of {MIN_NAMES_PER_FILE}")
for f in sorted(per_file):
    print(f"  list  {f}: {len(per_file[f])} distinct src/<name>.c spellings")

if len(per_file) >= 4:
    ok(f"{len(per_file)} files carry a runtime source list (floor is 4)")
else:
    bad(f"only {len(per_file)} files look like they carry a runtime source list, floor is "
        f"4 - the Makefile, src/main.c, src/cmd_compile.c, src/tcc_backend.c and "
        f"scripts/build-tcc.sh all do, so either PATH_RE went thin or "
        f"MIN_NAMES_PER_FILE is now excluding a real list")

if len(names) >= MIN_PATH_NAMES:
    ok(f"discovered {len(names)} distinct src/<name>.c spellings across "
       f"{len(SCAN)} files (floor is {MIN_PATH_NAMES})")
else:
    bad(f"discovered only {len(names)} distinct src/<name>.c spellings, floor is "
        f"{MIN_PATH_NAMES} - either PATH_RE has gone thin, in which case this gate is "
        f"comparing an empty set against the filesystem and cannot fail, or a great many "
        f"sources were genuinely removed, in which case lower MIN_PATH_NAMES in the same "
        f"change")

for name in sorted(names):
    if name in NOT_COMPILER_SOURCES:
        continue
    if not os.path.exists(f'src/{name}.c'):
        bad(f"src/{name}.c is named by {', '.join(sorted(names[name]))} but does not "
            f"exist. If this is a cross-compile or build-tcc.sh list the failure is "
            f"SILENT - system()'s status is discarded and stderr goes to /dev/null - so "
            f"the only symptom is an archive or link quietly missing a translation unit.")

for name in sorted(NOT_COMPILER_SOURCES):
    if os.path.exists(f'src/{name}.c'):
        bad(f"src/{name}.c now exists, so `{name}` must be removed from "
            f"NOT_COMPILER_SOURCES - leaving it there exempts a real compiler source "
            f"from this gate forever")
    elif name in names:
        ok(f"documented non-compiler source `{name}` is still spelled `%s/src/{name}.c` "
           f"and still absent from src/")

# --------------------------------------------------- PASS B: arrays of bare source names
# rt_srcs[] / rt_srcs2[] / win_srcs[] / wasm_srcs[] in main.c and srcs[] in tcc_backend.c
# hold BARE names that the caller formats into `%s/src/%s.c`, so PASS A cannot see them.
# An array qualifies as a runtime-source list when at least one element resolves to a real
# src/*.c; then EVERY element must. That direction is the safe one - it can only widen
# what is scrutinised - and the bug being hunted (one stale name among valid ones) is
# exactly what it catches.
ARRAY_RE = re.compile(r'const\s+char\s*\*\s*(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;', re.S)
ELEM_RE  = re.compile(r'"([A-Za-z0-9_]+)"')

arrays = 0
for f in ('src/main.c', 'src/tcc_backend.c'):
    try:
        text = open(f, encoding='utf-8', errors='replace').read()
    except OSError as e:
        bad(f"cannot read {f}: {e} - this gate is vacuous without it")
        continue
    # Count lines on the SAME text the match came from. strip_c_comments preserves the
    # newline COUNT but not byte OFFSETS, so counting on `text` while matching on the
    # stripped copy reported src/main.c:2383 for a list that is really at :3415 - a
    # citation a reader cannot follow is worse than none.
    stripped = strip_c_comments(text)
    for m in ARRAY_RE.finditer(stripped):
        ident, body = m.group(1), m.group(2)
        elems = ELEM_RE.findall(body)
        if not elems:
            continue
        if not any(os.path.exists(f'src/{e}.c') for e in elems):
            continue                      # not a runtime-source list
        arrays += 1
        lineno = stripped.count('\n', 0, m.start()) + 1
        absent = [e for e in elems if not os.path.exists(f'src/{e}.c')]
        if absent:
            bad(f"{f}:{lineno} `{ident}[]` names {absent} with no matching src/<name>.c. "
                f"This list is consumed as `%s/src/%s.c` on a cross-compile path whose "
                f"system() status is discarded, so the compile of the missing file fails "
                f"with no message and no non-zero exit.")
        else:
            ok(f"{f}:{lineno} `{ident}[]`: all {len(elems)} entries resolve to a "
               f"src/<name>.c that exists")
        if len(elems) < MIN_ARRAY_ELEMS:
            bad(f"{f}:{lineno} `{ident}[]` has only {len(elems)} entries (floor "
                f"{MIN_ARRAY_ELEMS}) - ELEM_RE may have gone thin, which would make the "
                f"check above vacuous for this list")

if arrays >= MIN_ARRAYS:
    ok(f"discovered {arrays} bare-name runtime-source arrays (floor is {MIN_ARRAYS})")
else:
    bad(f"discovered only {arrays} bare-name runtime-source arrays, floor is "
        f"{MIN_ARRAYS} - ARRAY_RE no longer matches the way these lists are written, so "
        f"rt_srcs/win_srcs/wasm_srcs are unguarded. Fix the regex, do not lower the floor "
        f"unless a list was genuinely deleted.")

# ----------------------------------------------- NEGATIVE CONTROLS: can each pass SEE it?
# Without these, every green above is unfalsifiable. Each control feeds a name that
# certainly has no file and asserts the pass flags it.
GHOST = 'definitely_not_a_wyn_source_zzz'
assert not os.path.exists(f'src/{GHOST}.c')

found = PATH_RE.findall(f'RT_SRCS = src/{GHOST}.c src/wyn_rc.c\n')
if GHOST in found and not os.path.exists(f'src/{GHOST}.c'):
    ok(f"negative control: PASS A extracts a nonexistent `src/{GHOST}.c` from a list")
else:
    bad(f"negative control FAILED: PASS A did not extract `{GHOST}` (got {found!r}) - it "
        f"cannot see a missing source, so its green above means nothing")

probe = f'const char* rt_srcs[] = {{ "wyn_rc", "{GHOST}", NULL }};'
m = ARRAY_RE.search(probe)
elems = ELEM_RE.findall(m.group(2)) if m else []
qualifies = any(os.path.exists(f'src/{e}.c') for e in elems)
absent = [e for e in elems if not os.path.exists(f'src/{e}.c')]
if qualifies and absent == [GHOST]:
    ok(f"negative control: PASS B flags `{GHOST}` inside an otherwise-valid array")
else:
    bad(f"negative control FAILED: PASS B did not flag `{GHOST}` "
        f"(elems={elems!r}, qualifies={qualifies}, absent={absent!r}) - it cannot see a "
        f"missing source in a bare-name array")

print("")
print(f"runtime-source-lists: {PASS} pass, {FAIL} fail")
sys.exit(1 if FAIL else 0)
PY
