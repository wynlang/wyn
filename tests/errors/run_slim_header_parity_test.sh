#!/bin/bash
# src/wyn_runtime_slim.h is HAND-MAINTAINED and is the only runtime header
# `wyn run --release` / `wyn build` (non-debug) emit. Every disagreement between
# it and the real runtime is a release-only defect that debug builds cannot see,
# because a debug build pastes the whole of wyn_runtime.h - definitions included -
# into the program's translation unit.
#
# This gate has FIVE arms. Arms 2-5 were added for #468; arm 1 is the original.
#
#   1. COMPLETENESS (names).   Every function DEFINED in wyn_runtime.h that
#      codegen can emit a call to must be NAMED in the slim header.
#
#   2. SIGNATURES vs DEFINITIONS.  Every DECLARATION in the slim header must
#      match the DEFINITION, parameter for parameter and return type included.
#
#   3. A DECLARATION MUST HAVE A SYMBOL.  Every name the slim header declares
#      must have a defining symbol in runtime/libwyn_rt.a, because --release
#      takes every runtime symbol from that archive and nowhere else.
#
#   4. wyn_runtime.h's OWN DECLARATIONS vs the definitions, same comparison as
#      arm 2.
#
#   5. PLATFORM-SPLIT DUPLICATES must agree with each other.
#
# WHY ARM 1 WAS NOT ENOUGH - the measured #468 findings. Arm 1 compares NAMES. A
# name can be present and the declaration still be a lie, and the `cc` line
# carries `-w -Wno-int-conversion`, so the C compiler said nothing:
#
#   Http_accept    declared `int`, defined `char*`    - a 64-bit pointer returned
#                                                       through a 32-bit prototype
#   Http_respond   declared 3 params, defined 4       - `too many arguments to
#                  function call, expected 3, have 4`: NO four-argument
#                  Http_respond program could be built with --release AT ALL,
#                  while the same program ran in debug
#   Http_method    declared `(int)`, defined `(const char*)`   - x2 (Http_path)
#   Http_body      declared `char* (int)`, defined `const char* (HttpResponse*)`
#   Http_status    declared `int (int)`,   defined `int (HttpResponse*)`
#   Db_exec        declared `long long`, defined `int`
#   Db_exec_p      declared `(..., ...)` variadic, defined `(..., WynArray)` -
#                  a by-value struct through a variadic prototype, a different
#                  argument-passing convention on AAPCS
#   Db_query_p     same
#   wyn_time_now   declared `long` in BOTH headers, defined `long long` in
#                  stdlib_time.c - invisible on LP64, half the width on Windows
#
# WHY THE DEFINITION SIDE READS THE .c FILES AND THE INCLUDE CLOSURE. The old
# gate read exactly two files, both headers. wyn_time_now was therefore
# unfindable: the declaration agreed with the OTHER DECLARATION, and only the
# definition in src/stdlib_time.c disagreed with both. Gui_*/Audio_* are defined
# in src/gui.h, reached through wyn_runtime.h's #include list, so a two-file read
# could not see those either. The definition side here is:
#
#   wyn_runtime.h + every "..." header it includes, transitively
#   + every .c file in the Makefile's RT_SRCS (the archive's single source of truth)
#
# WHY ARM 3 EXISTS SEPARATELY FROM ARM 2. A declaration can be perfectly
# well-formed and name a function that does not exist. Twenty-four did:
# Ws_connect/send/recv/close, eleven wyn_time_* accessors, HashMap_clear,
# HashMap_remove, Http_respond_with_header, range, Db_exec_p, Db_query_p, and the
# three bare http_status/http_error/http_clear_headers aliases - which are
# `static inline` in wyn_runtime.h, so there is nothing in the archive to link and
# `print(http_status())` ran in debug and failed at LINK under --release with
# `Undefined symbols: _http_status`. Arm 2 cannot see any of these: a name with no
# definition anywhere has nothing to disagree with.
#
# WHY THERE IS NO HARDCODED DUPLICATE COUNT. Arm 5 reports how many names have
# more than one definition (the #ifdef WYN_USE_SQLITE / _WIN32 splits) and asserts
# only that the duplicates AGREE. A pinned number would go red on a correct future
# change that adds or removes a platform branch.
#
# WHY EVERY ARM HAS A FLOOR. Each arm prints what it found and fails if that is
# below a floor. Without this, breaking an extraction regex makes the gate compare
# two empty sets and report success - which is how a green gate hid twelve missing
# declarations once already. The floors are checked BEFORE the comparisons, so a
# damaged regex fails loudly instead of passing vacuously.
set -uo pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
PASS=0; FAIL=0
ok(){ echo "  ok    $1"; PASS=$((PASS+1)); }
bad(){ echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

for f in src/wyn_runtime.h src/wyn_runtime_slim.h src/types.c src/codegen.c \
         src/codegen_expr.c src/codegen_stmt.c src/codegen_program.c src/codegen_lambda.c \
         Makefile; do
  if [ ! -f "$ROOT/$f" ]; then
    bad "cannot read $f - this gate is vacuous without it"
    echo ""; echo "slim-header-parity: $PASS pass, $FAIL fail"; exit 1
  fi
done

# Arm 3 needs the archive. Do not skip when it is missing: a skipped arm is a
# green gate that checked nothing. `make test` depends on runtime/libwyn_rt.a for
# exactly this reason.
if [ ! -f "$ROOT/runtime/libwyn_rt.a" ]; then
  bad "runtime/libwyn_rt.a is missing - arm 3 cannot run and this gate would be vacuous. Run 'make runtime'."
  echo ""; echo "slim-header-parity: $PASS pass, $FAIL fail"; exit 1
fi
NM_BIN="${NM:-nm}"
if ! command -v "$NM_BIN" >/dev/null 2>&1; then
  bad "no '$NM_BIN' on PATH - arm 3 cannot run. Set NM=<nm-like tool>."
  echo ""; echo "slim-header-parity: $PASS pass, $FAIL fail"; exit 1
fi

# Captured to a file, then echoed, so the arm count can be read back without a pipe
# (a pipe would make $? belong to the reader, not to python).
GATE_OUT=$(mktemp) || exit 2
trap 'rm -f "$GATE_OUT"' EXIT
/usr/bin/env python3 - "$ROOT" "$NM_BIN" > "$GATE_OUT" 2>&1 <<'PY'
import re, sys, os, subprocess
root, nm_bin = sys.argv[1], sys.argv[2]
def rd(p):
    with open(os.path.join(root, p), encoding='utf-8', errors='replace') as f:
        return f.read()

FAILS = []
def fail(msg): FAILS.append(msg)

# ---------------------------------------------------------------- lexing
def strip_comments(s):
    """Remove comments, keep string literals and the line count intact."""
    out = []; i = 0; n = len(s)
    while i < n:
        c = s[i]
        if c == '/' and i+1 < n and s[i+1] == '/':
            j = s.find('\n', i)
            if j < 0: break
            i = j
        elif c == '/' and i+1 < n and s[i+1] == '*':
            j = s.find('*/', i+2)
            if j < 0: break
            out.append('\n' * s.count('\n', i, j))
            i = j+2
        elif c in '"\'':
            q = c; j = i+1
            while j < n:
                if s[j] == '\\': j += 2; continue
                if s[j] == q or s[j] == '\n': break
                j += 1
            out.append(s[i:j+1]); i = j+1
        else:
            out.append(c); i += 1
    return ''.join(out)

# RT_SRCS, read out of the Makefile rather than restated here. That variable is
# already documented as the single source of truth for the archive's members, so
# a runtime source added there is covered by this gate with no edit.
m = re.search(r'^RT_SRCS\s*=\s*((?:.*\\\n)*.*)$', rd('Makefile'), re.M)
rt_srcs = m.group(1).replace('\\\n', ' ').split() if m else []

def include_closure(path, seen=None):
    if seen is None: seen = set()
    if path in seen or not os.path.exists(os.path.join(root, path)): return []
    seen.add(path)
    out = [path]
    for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', rd(path), re.M):
        out += include_closure('src/' + os.path.basename(inc), seen)
    return out

full_files = include_closure('src/wyn_runtime.h')

# ------------------------------------------------- declaration / definition regexes
# A leading type-ish token run, a name, a parenthesised parameter list, then `{`
# (definition) or `;` (declaration). Anchored at column 0 for definitions, which is
# this codebase's convention and keeps function bodies out of the results.
_RET   = r'([A-Za-z_][A-Za-z0-9_]*(?:\s+[A-Za-z_][A-Za-z0-9_]*)*[\s*]+)'
_PARMS = r'([^;{)]*(?:\([^()]*\)[^;{)]*)*)'
DEF_RE  = re.compile(r'^' + _RET + r'([A-Za-z_][A-Za-z0-9_]*)\s*\(' + _PARMS + r'\)\s*\{', re.M)
DECL_RE = re.compile(r'^(?:extern\s+)?' + _RET + r'([A-Za-z_][A-Za-z0-9_]*)\s*\(' + _PARMS + r'\)\s*;', re.M)
CTRL = {'if','for','while','switch','return','else','do','case','sizeof','typedef'}

# ------------------------------------------------- type normalisation
all_text = ''.join(strip_comments(rd(f)) for f in full_files + rt_srcs
                   + ['src/wyn_runtime_slim.h'])
# `struct X` and `X` are the same type exactly when `typedef struct X ... X;`
# exists. Collected, not assumed: wyn_runtime.h:26 must say `struct Future*`
# because future.h is included 160 lines later, so the two spellings of that one
# type are both correct and must not be reported.
STRUCT_ALIAS = set(re.findall(r'typedef\s+struct\s+([A-Za-z_][A-Za-z0-9_]*)\s+\1\s*;', all_text))
STRUCT_ALIAS |= set(re.findall(
    r'typedef\s+struct\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{.*?\}\s*\1\s*;', all_text, re.S))
# Function-pointer typedefs, so `TaskFuncWithReturn` and `void*(*)(void*)` compare
# equal - spawn_fast.c's two platform branches spell the same parameter both ways.
FNPTR_TYPEDEF = {
    name: (ret, parms)
    for ret, name, parms in re.findall(
        r'typedef\s+([A-Za-z_][A-Za-z0-9_ *]*?)\(\s*\*\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)'
        r'\s*\(([^;]*)\)\s*;', all_text)
}

def norm_type(t):
    t = re.sub(r'\b(restrict|__restrict|__restrict__)\b', ' ', t)
    t = re.sub(r'\s*\*\s*', '*', t)
    t = re.sub(r'\s+', ' ', t).strip()
    for a in STRUCT_ALIAS:
        t = re.sub(r'\bstruct\s+' + re.escape(a) + r'\b', a, t)
    return t.strip()

def split_params(p):
    out = []; depth = 0; cur = ''
    for ch in p:
        if ch in '([': depth += 1
        elif ch in ')]': depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur); cur = ''
        else:
            cur += ch
    if cur.strip(): out.append(cur)
    return [x.strip() for x in out]

TYPEKW = {'void','char','short','int','long','float','double','signed','unsigned',
          'const','volatile','struct','union','enum','_Bool','bool','restrict',
          '__restrict','__restrict__','_Atomic'}

def norm_param(p, depth=0):
    p = re.sub(r'\b(restrict|__restrict|__restrict__)\b', ' ', p).strip()
    if p in ('', 'void'): return 'void'
    if p == '...': return '...'
    fp = re.match(r'^(.*?)\(\s*\*\s*([A-Za-z_][A-Za-z0-9_]*)?\s*\)\s*\((.*)\)\s*$', p, re.S)
    if fp and depth < 4:
        inner = [norm_param(x, depth+1) for x in split_params(fp.group(3))] or ['void']
        return norm_type(fp.group(1)) + '(*)(' + ', '.join(inner) + ')'
    arr = ''
    am = re.search(r'\[\s*[^\]]*\]\s*$', p)
    if am:
        p = p[:am.start()]; arr = '*'
    toks = re.findall(r'[A-Za-z_][A-Za-z0-9_]*|\*', p)
    # Drop a trailing identifier that is the PARAMETER NAME, not part of the type.
    if len(toks) >= 2 and toks[-1] not in TYPEKW and toks[-1] != '*':
        rest = toks[:-1]
        if any(t not in ('const','volatile','*') for t in rest):
            toks = rest
    s = ''
    for t in toks:
        s += t if t == '*' else (('' if (not s or s.endswith('*')) else ' ') + t)
    s = norm_type(s + arr)
    if depth < 4:
        base = s.rstrip('*')
        if base in FNPTR_TYPEDEF and base == s:   # the bare typedef name, not a pointer to it
            exp = FNPTR_TYPEDEF[base]
            inner = [norm_param(x, depth+1) for x in split_params(exp[1])] or ['void']
            s = norm_type(exp[0]) + '(*)(' + ', '.join(inner) + ')'
    return s

def signature(ret, params):
    ps = [norm_param(x) for x in split_params(params)] or ['void']
    # `static` / `inline` are not part of the callable signature, but they decide
    # whether a SYMBOL exists - arm 3's job, not arm 2's.
    r = ' '.join(t for t in norm_type(ret).replace('*', ' * ').split()
                 if t not in ('static', 'inline', '__inline', '_Noreturn'))
    return re.sub(r'\s*\*\s*', '*', r) + ' (' + ', '.join(ps) + ')'

def scan(text, rx, where=''):
    res = {}
    for mm in rx.finditer(text):
        ret, name, params = mm.group(1), mm.group(2), mm.group(3)
        rtoks = re.findall(r'[A-Za-z_][A-Za-z0-9_]*', ret)
        if name in CTRL or (rtoks and rtoks[-1] in CTRL): continue
        if 'typedef' in rtoks: continue
        is_static = 'static' in rtoks
        res.setdefault(name, []).append(
            (signature(ret, params), where, text.count('\n', 0, mm.start()) + 1,
             is_static))
    return res

slim_txt  = strip_comments(rd('src/wyn_runtime_slim.h'))
slim_decls = scan(slim_txt, DECL_RE, 'src/wyn_runtime_slim.h')
slim_defs  = scan(slim_txt, DEF_RE,  'src/wyn_runtime_slim.h')  # `static inline` copies
slim_macros = set(re.findall(r'^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(', slim_txt, re.M))

defs = {}
full_decls = {}
for f in full_files + rt_srcs:
    t = strip_comments(rd(f))
    for name, lst in scan(t, DEF_RE, f).items():
        defs.setdefault(name, []).extend(lst)
    if f == 'src/wyn_runtime.h':
        full_decls = scan(t, DECL_RE, f)

# --------------------------------------------------------------- arm 3 input
nm_out = subprocess.run([nm_bin, '-g', os.path.join(root, 'runtime/libwyn_rt.a')],
                        capture_output=True, text=True).stdout
arch_syms = set()
for line in nm_out.splitlines():
    # Keep the symbol EXACTLY as the archive spells it. Do not strip a leading
    # underscore here: Mach-O prefixes every symbol with one, ELF does not, so a
    # `_?` in this pattern silently renames an ELF symbol that is genuinely called
    # `_foo` into `foo`. Normalising at the comparison site instead (see
    # `defines()` below) is correct on both and loses nothing.
    sm = re.match(r'^[0-9a-fA-F]*\s*([A-Za-z])\s+(\S+)$', line.strip())
    if sm and sm.group(1) in 'TtDdBbSsCiRr':
        arch_syms.add(sm.group(2))

def defines(name):
    """Is `name` defined in the archive, under either platform's convention?

    Mach-O emits `_Http_respond` for C `Http_respond`; ELF emits `Http_respond`.
    Accepting both here keeps one gate correct on both platforms without
    rewriting the symbol table, which is what made the first version of this arm
    destructive on ELF.
    """
    return name in arch_syms or ('_' + name) in arch_syms

# ------------------------------------------------------------------- FLOORS
# Checked before any comparison. A damaged regex, a renamed Makefile variable or
# an unreadable archive must fail HERE and say so, not quietly compare two empty
# sets and print a pass.
FLOORS = [
    ("RT_SRCS .c files parsed from the Makefile", len(rt_srcs),              25),
    ("headers in wyn_runtime.h's include closure", len(full_files),          12),
    ("declarations extracted from the slim header", len(slim_decls),        700),
    ("declarations extracted from wyn_runtime.h",  len(full_decls),         150),
    ("function definitions found across header closure + RT_SRCS",
                                                   len(defs),               900),
    ("defining symbols read out of runtime/libwyn_rt.a", len(arch_syms),    800),
    ("typedef'd struct aliases collected",         len(STRUCT_ALIAS),         8),
]
print("  extraction:")
for label, got, floor in FLOORS:
    print(f"    {got:>5}  {label}  (floor {floor})")
    if got < floor:
        fail(f"extraction floor: only {got} {label}, floor is {floor}. The "
             f"extraction is broken - this gate would compare near-empty sets "
             f"and report a pass.")

if FAILS:
    for f_ in FAILS: print("  FAIL  " + f_)
    sys.exit(1)

# ============================================================ ARM 1: completeness
types_txt = strip_comments(rd('src/types.c'))
cg_txt = ''.join(strip_comments(rd(f)) for f in
                 ('src/codegen.c', 'src/codegen_expr.c', 'src/codegen_stmt.c',
                  'src/codegen_program.c', 'src/codegen_lambda.c'))
# Every STRING LITERAL in the dispatch tables and codegen units, concatenated.
# Searching the literal TEXT and not the raw source is the 2026-09-30 widening:
# the old rule looked for the token `"name"` with quotes, which only matches a
# name that is a string literal all by itself. codegen writes
#     emit("({ const char* __pms = map_to_string(")
# so most names sit mid-literal and were invisible.
_literal_text = " ".join(re.findall(r'"((?:[^"\\\n]|\\.)*)"', types_txt)
                         + re.findall(r'"((?:[^"\\\n]|\\.)*)"', cg_txt))
def reachable(n):
    return re.search(r'(?<![A-Za-z0-9_])' + re.escape(n) + r'(?![A-Za-z0-9_])',
                     _literal_text) is not None

slim_names = set(re.findall(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', slim_txt))
# DELIBERATELY still scoped to wyn_runtime.h's own definitions, unlike arms 2-5.
# Widening it to the whole definition universe reports 93 further names - the
# WynIter family in src/wyn_iter.h, the hashset/optional/result helpers in their
# .c files - which is a real and larger gap (a Wyn GENERATOR does not compile under
# --release at all: `use of undeclared identifier '__iter'`), but it is a different
# change with a different fix, and turning it on here would red CI on work this gate
# is not about. Measured 2026-10-07, worth re-measuring before acting on.
runtime_h_defs = {n for n, lst in defs.items()
                  if any(e[1] == 'src/wyn_runtime.h' for e in lst)}
missing = sorted(n for n in runtime_h_defs if n not in slim_names and reachable(n))
print(f"\n  arm 1 - completeness: {len(runtime_h_defs)} functions defined in wyn_runtime.h, "
      f"{sum(1 for n in runtime_h_defs if reachable(n))} reachable from codegen")
for n in missing:
    fail(f"arm 1: {n} is defined in wyn_runtime.h and reachable from Wyn code, but "
         f"wyn_runtime_slim.h does not mention it")
if not missing:
    ok1 = True
    print("    ok    every reachable runtime function is named in both headers")

# ====================================================== ARM 2: slim decl vs definition
compared = 0
for name in sorted(slim_decls):
    dsig = slim_decls[name][0][0]
    if name not in defs: continue
    compared += 1
    got = sorted({e[0] for e in defs[name]})
    if dsig not in got:
        where = ', '.join(sorted({f"{e[1]}:{e[2]}" for e in defs[name]}))
        fail(f"arm 2: {name} - wyn_runtime_slim.h declares  {dsig}  but the "
             f"definition is  {' | '.join(got)}  ({where})")
print(f"\n  arm 2 - slim declarations compared against a definition: {compared}")
if compared < 600:
    fail(f"arm 2 floor: only {compared} slim declarations could be matched to a "
         f"definition, floor is 600 - the name matching is broken")
elif not any(f_.startswith('arm 2:') for f_ in FAILS):
    print("    ok    every slim declaration matches its definition's signature")

# ============================= ARM 3: a declaration must have a defining symbol
nosym = sorted(n for n in slim_decls
               if not defines(n) and n not in slim_defs and n not in slim_macros)
print(f"\n  arm 3 - slim declarations checked against runtime/libwyn_rt.a: {len(slim_decls)}")
for n in nosym:
    fail(f"arm 3: {n} is declared in wyn_runtime_slim.h but has no defining symbol "
         f"in runtime/libwyn_rt.a - every --release build takes its runtime symbols "
         f"from that archive, so a program calling it fails to LINK. Either define it "
         f"in an RT_SRCS .c file, duplicate it here as `static inline`, or delete the "
         f"declaration.")
if not nosym:
    print("    ok    every slim declaration has a defining symbol in the archive")

# ==================================== ARM 4: wyn_runtime.h decl vs its definition
compared4 = 0
for name in sorted(full_decls):
    dsig = full_decls[name][0][0]
    if name not in defs: continue
    compared4 += 1
    got = sorted({e[0] for e in defs[name]})
    if dsig not in got:
        where = ', '.join(sorted({f"{e[1]}:{e[2]}" for e in defs[name]}))
        fail(f"arm 4: {name} - wyn_runtime.h declares  {dsig}  but the definition "
             f"is  {' | '.join(got)}  ({where})")
print(f"\n  arm 4 - wyn_runtime.h declarations compared against a definition: {compared4}")
if compared4 < 100:
    fail(f"arm 4 floor: only {compared4} wyn_runtime.h declarations could be matched "
         f"to a definition, floor is 100")
elif not any(f_.startswith('arm 4:') for f_ in FAILS):
    print("    ok    every wyn_runtime.h declaration matches its definition")

# ============================ ARM 5: platform-split duplicates must agree
# `static` definitions are EXCLUDED. A file-scope static has internal linkage, so
# two translation units may legitimately define unrelated functions with the same
# name - wyn_iter.h's `static inline void wyn_yield(long long)` (the generator
# yield) and spawn.c's external `void wyn_yield(void)` (sched_yield) are two
# different functions that happen to share a spelling, and reporting that pair as
# a conflict would be wrong. Only external-linkage definitions can collide at link
# time, which is what this arm is about.
ext = {n: [e for e in lst if not e[3]] for n, lst in defs.items()}
dups = {n: lst for n, lst in ext.items() if len(lst) > 1}
disagree = {n: lst for n, lst in dups.items() if len({e[0] for e in lst}) > 1}
n_static = sum(1 for lst in defs.values() for e in lst if e[3])
# The COUNT is reported, never asserted: #ifdef WYN_USE_SQLITE / _WIN32 branches
# are legitimately added and removed, and a pinned number reds on a correct change.
print(f"\n  arm 5 - external-linkage names with more than one definition "
      f"(platform/#ifdef splits): {len(dups)}   [{n_static} static definitions excluded]")
for n, lst in sorted(disagree.items()):
    sigs = ' | '.join(sorted({e[0] for e in lst}))
    where = ', '.join(sorted({f"{e[1]}:{e[2]}" for e in lst}))
    fail(f"arm 5: {n} has definitions that DISAGREE with each other: {sigs} ({where})")
if not disagree:
    print("    ok    every duplicated definition agrees with its twins")

print("")
for f_ in FAILS:
    print("  FAIL  " + f_)
sys.exit(1 if FAILS else 0)
PY
rc=$?
cat "$GATE_OUT"

# Count the ARMS, not the python invocation. The old tally printed "1 pass, 0 fail"
# whether five arms ran or one, which is the exact shape this gate exists to catch:
# a green number that cannot tell you what was checked. The python block prints one
# "ok" per arm it completed, so counting those makes the tally self-describing, and
# the floor below means a thinned run cannot report success.
ARMS_OK=$(grep -c '^    ok    ' "$GATE_OUT" 2>/dev/null) || ARMS_OK=0
if [ $rc -eq 0 ]; then PASS=$((PASS+ARMS_OK)); else FAIL=$((FAIL+1)); fi

ARM_FLOOR=5
if [ $rc -eq 0 ] && [ "$ARMS_OK" -lt "$ARM_FLOOR" ]; then
  bad "only $ARMS_OK of $ARM_FLOOR arms reported a result - the gate ran short and its green is not a result"
fi

echo ""; echo "slim-header-parity: $PASS pass, $FAIL fail"; [ "$FAIL" -eq 0 ]
