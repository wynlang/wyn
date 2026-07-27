# Wyn Reference-Counting Ownership Model — Map & Fix Spec

**Status:** design/mapping pass (read-only). No code changed by this document.
**Repo:** `repos/wyn` @ `99eedc7`.
**Purpose:** precise, evidence-backed map of the RC memory model so the
follow-up implementation of bugs **#32** (string-RC use-after-free) and **#25**
(recursive-enum payload leak) can be done safely. Two prior attempts stalled by
patching codegen without this map; this is the spec they were missing.

All claims below are grounded in the actual generated C (`./wyn build x.wyn
--debug` → `x.wyn.c`) and verified under AddressSanitizer against the runtime
(`make runtime-asan` → `runtime/libwyn_rt_asan.a`).

---

## 0. TL;DR

- There are **two independent "RC" systems** in the tree. Only one is live for
  strings: the `wyn_rc_*` refcount in `src/wyn_rc.c`. The `wyn_arc_*` /
  `WynObject` system in `src/arc_runtime.c` is legacy and is **not** on the
  string or enum path (grep shows codegen never emits `wyn_arc_retain/release`
  for user strings; the string releases are all `wyn_rc_release`).
- The compiler has **no move/alias analysis**. Ownership is inferred by a set of
  **syntactic heuristics in codegen** (per-`STMT_VAR` init-expression shape),
  layered over a single-owner "release every tracked string once at scope/return
  exit" model.
- The root defect both bugs share: **the ownership heuristics assume every
  producer of an rc value creates a fresh +1 reference, and every binding
  becomes an independent owner that must release once.** When a value is
  *aliased* rather than freshly produced — a function that returns its argument
  (#32), or an enum payload box shared by a struct copy (#25) — the retain/
  release counts do not balance.
  - #32 is **over-release** (double free → UAF).
  - #25 is **under-release** (never freed → leak); the naive "recursively free"
    fix converts it into over-release (double free of shared subtrees).

---

## 1. The RC primitives

### 1.1 Header layout — `src/wyn_rc.c:16-22`, `src/wyn_rc.h:17-23`

```c
typedef struct {
    uint32_t        magic;      // 0x57594E52 "WYNR"
    _Atomic int32_t refcount;
    uint32_t        capacity;   // allocated bytes (for realloc-in-place)
    uint32_t        length;     // cached string length (avoid O(n) strlen)
    uint32_t        magic2;     // ~magic == 0xA8A6B1AD, complement sentinel
} WynRcHeaderFull;
```

The header sits **immediately before** the user pointer; user code always holds
a pointer to the payload, and `rc_full_header(ptr) = ptr - sizeof(header)`
(`src/wyn_rc.c:28-30`). This exact layout is **hand-mirrored in three places** and
they must stay byte-identical (comment `wyn_rc.c:6-14`):
- `WynRcHeaderFull` (`src/wyn_rc.c:16`)
- `WynRcHeader` (`src/wyn_rc.h:17`)
- `RcHdr` inside `wyn_string_concat_safe` (`src/wyn_runtime.h:225`)

### 1.2 `wyn_rc_alloc` — `src/wyn_rc.c:32-55`

`malloc(sizeof(header)+size)`, sets `magic`/`magic2`, `refcount = 1` (atomic
store), `capacity = size`, `length = 0`. Returns `hdr+sizeof(header)`. Also
maintains a global `[heap_low, heap_high]` range via atomic CAS (`wyn_rc.c:43-53`)
used by `wyn_rc_is_heap`.

### 1.3 String allocation — `src/wyn_arena.c:7-20`

`wyn_str_alloc(len)` = `wyn_rc_alloc(len+1)` then NUL-terminates
(`wyn_arena.c:7-11`). `wyn_strdup` allocs, copies, and caches `length`
(`wyn_arena.c:13-20`). **Every heap Wyn string is therefore an rc object with
refcount initialized to 1.** All the `wyn_runtime.h` string builders
(`wyn_string_concat_safe`, substring, replace, split, …) allocate via
`wyn_str_alloc`, so they all return a **fresh +1** rc string.

### 1.4 `wyn_rc_is_heap` — `src/wyn_rc.c:57-65`

NULL → 0; out-of-range pointer → 0; otherwise requires **both** `magic` and
`magic2` to match. This is what makes `retain`/`release` **safe no-ops on string
literals** (which live in `.rodata`, outside the heap range) and on any
non-rc pointer. Critical: string literals are *immortal by construction* — they
are never rc-tracked, so releasing one is a silent no-op.

### 1.5 retain / release — `src/wyn_rc.c:80-99`

```c
void wyn_rc_retain(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return;
    if (refcount == WYN_RC_IMMORTAL) return;             // INT32_MAX sentinel
    atomic_fetch_add(&refcount, 1, relaxed);
}
void wyn_rc_release(const void* ptr) {
    if (!ptr || !wyn_rc_is_heap(ptr)) return;
    if (refcount == WYN_RC_IMMORTAL) return;
    if (atomic_fetch_sub(&refcount, 1, acq_rel) == 1) {   // was 1 → now 0
        atomic_thread_fence(acquire);
        hdr->magic = 0; hdr->magic2 = 0;                  // poison
        free(hdr);
    }
}
```

- **The refcount IS atomic** (`_Atomic int32_t`, `atomic_fetch_add/sub`). retain
  uses `relaxed`; release uses `acq_rel` + an acquire fence before `free`.
- `WYN_RC_IMMORTAL = INT32_MAX` (`wyn_rc.h:15`) marks objects that must never be
  freed. Nothing in the string path currently *sets* immortal — it's reserved for
  stack/static values.
- On drop to zero, the header magic is zeroed **before** free, so a second
  release of the same pointer will fail `wyn_rc_is_heap` (magic no longer
  matches) — **but only if the memory hasn't been reused.** Under ASan the block
  is quarantined and the load of `refcount`/`magic` itself is the reported
  use-after-free (see §4.1). Without ASan the double-release is a latent
  heap-corruption / double-free depending on allocator reuse. So the magic-poison
  is a **best-effort** guard, not a correctness guarantee.

### 1.6 The other (legacy) system — `src/arc_runtime.c`

`WynArc` (`arc_runtime.c:5-20`) and `WynObject`/`wyn_arc_alloc`/`wyn_arc_retain`/
`wyn_arc_release` (`arc_runtime.c:23-44`) use a **non-atomic** `ref_count` and a
destructor hook. `pop_scope()` in `src/codegen.c:1439-1443` calls
`wyn_arc_release` on tracked `string_objects[]`, but that array is only populated
by an ARC-object path that the string codegen no longer uses (strings are emitted
as `const char*`, not `WynObject*`). Treat `arc_runtime.c` as **out of scope** for
#32/#25; the live model is `wyn_rc_*`.

---

## 2. Every emission site (retain / release)

Codegen is split across `codegen.c`, `codegen_stmt.c`, `codegen_expr.c`,
`codegen_program.c`. The scope/tracking machinery lives in `codegen.c`.

### 2.1 The tracking data structures (`src/codegen.c:633-890`)

Two parallel lists of string variable names:
- **`string_var_names[]`** (`codegen.c:634`) — *all* string locals, used for
  type detection (`is_string_var`, `codegen.c:829`) and **inner-block** scope
  release. Scoped via a count-stack `scope_var_count_stack[]` (`codegen.c:640`),
  pushed/popped by `push_string_scope`/`pop_string_scope_and_release`
  (`codegen.c:644-661`).
- **`string_var_releasable[]`** (`codegen.c:819`) — only **top-level** string
  locals (registration is gated `if (string_var_scope_depth > 0) return;`,
  `codegen.c:822-823`). These are released at **function return**.

`string_var_scope_depth` (`codegen.c:637`) starts at `-1` after
`reset_string_vars` (`codegen.c:862`, called at each function entry via
`codegen_stmt.c:170`), is `++`/`--` around every `STMT_BLOCK`
(`codegen_stmt.c:2309,2370`). So depth `0` = function top level (releasable),
`>0` = nested block (released by the block's `pop_string_scope`).

### 2.2 RELEASE emission sites

| # | Where (file:line) | Trigger / rule |
|---|---|---|
| R1 | `codegen.c:648-661` `pop_string_scope_and_release` | End of an **inner** `STMT_BLOCK`: release every `string_var_names[]` registered since block entry. Emits `wyn_rc_release(<var>); `. |
| R2 | `codegen.c:663-674` `emit_block_string_releases` | `break`/`continue` (`codegen_stmt.c:2227,2234`): release current inner block's strings before the jump. |
| R3 | `codegen.c:863-872` `emit_string_releases` | Declared but **no live caller** (grep: only the prototype). Effectively dead. |
| R4 | `codegen.c:880-889` `emit_string_releases_for_return` | **Function return** (`codegen_stmt.c:2158-2163`): release every top-level `string_var_releasable[]` **except** any var textually referenced by the return expression (liveness via `expr_references_var`, `codegen.c:893-925`). This is the "return the value you were about to free" guard (bug M3). |
| R5 | `codegen_expr.c:4260` | String **reassignment** `a = <fresh>` (RHS is BINARY/CALL/METHOD/INTERP): `if (__rc_tmp != a) wyn_rc_release(a); a = __rc_tmp;` — release old owner unless concat reused the buffer. |
| R6 | `codegen_expr.c:4265` | String **reassignment** `a = <shared>` (RHS other, e.g. ident): `wyn_rc_retain(__rc_tmp); wyn_rc_release(a); a = __rc_tmp;`. |
| R7 | `codegen.c:770,785` (`pop_closure_scope_and_release`, `emit_block_closure_releases`) | Closure locals: release `<var>.env` at scope exit / break / return. |
| R8 | `codegen_expr.c:807-808` | String `+` operands that are temporaries (`__cl`/`__cr`): release after concat. |
| R9 | `codegen_expr.c:1297,1443,1472` | Temp printed strings (`__ps`): release after `print`. |
| R10 | `codegen_expr.c:3748` | `match` scrutinee temp `__mo` release. |
| R11 | `codegen_expr.c:5347`, `codegen_stmt.c:421,426` | Temp string-arg cleanup (`__si`/`__sa`) at call sites. |
| R12 | `codegen_program.c:1289-1370` | Spawn-wrapper: release string args (`__a0`, `args->aN`) **after** the wrapped call runs (task owns a retained copy — see T-side below). |

### 2.3 RETAIN emission sites

| # | Where (file:line) | Trigger / rule |
|---|---|---|
| T1 | `codegen_stmt.c:2007` | `var b = a` where `a` is a **live** (or outer-scope) string var: `wyn_rc_retain(b)`. The *copy* path of the move/copy decision (§3). |
| T2 | `codegen_stmt.c:1832` | Closure copy `var g = f`: `wyn_rc_retain(g.env)`. |
| T3 | `codegen_stmt.c:2264,2296` | Fire-and-forget `spawn f(str)`: retain the string arg for the task (`wyn_rc_retain(__sarg)` / `__sa_N->aN`). |
| T4 | `codegen_expr.c:84` | String pushed into an array while **still live**: `wyn_rc_retain(<var>)` so the array co-owns (see `codegen_string_push_transfer`, `codegen_expr.c:64-85`). |
| T5 | `codegen_expr.c:4265,4283` | Reassignment "shared" paths (string R6, closure). |
| T6 | `codegen_expr.c:5470,5520` | Spawn/await string-arg retains at call sites. |

### 2.4 The "String cleanup handled by ARC" comment

`src/codegen.c:1429-1431`: inside `pop_scope()`, a tracked `char*`/`const char*`
local emits only the **comment** `/* String cleanup handled by ARC */` — the
actual `wyn_rc_release` for that local comes from the parallel
`string_var_names`/`string_var_releasable` machinery (R1/R4), *not* from
`pop_scope`. So `pop_scope` is a no-op for strings; the two systems coexist and
the release you see in generated C is R1 or R4.

---

## 3. The ownership model as-is — per-operation trace

**Model in one sentence:** each rc string producer yields a fresh **+1**; each
string local is registered as an owner and released **exactly once** at its scope
or function exit; a copy of a *live* source adds a retain (so both owners
release), a copy of a *dead* source is a **move** (source un-tracked so it isn't
released). There is **no accounting of what a function returns** — a call result
is always assumed fresh.

The move/copy decision for `var b = a` is `codegen_stmt.c:1984-2010`:
- if `a` is top-level releasable → **copy** (retain `b`);
- else if `a` is **not live after** this statement in the same block
  (`var_is_live_after`, `codegen.c:959`) → **move** (`unregister_string_var(a)`);
- else → **copy** (retain).

Trace table (✓ = emitted, ✗ = not; "bal" = allocs==retains==releases):

| Operation | retain? | release @ exit? | allocs | net | verdict |
|---|---|---|---|---|---|
| **literal** `var a = "x"` | ✗ | ✗ effective (release is no-op: literal not heap, `is_heap`=0) | 0 | 0/0 | **balanced** (immortal) |
| **fresh** `var a = "x"+"y"` | ✗ | ✓ (R1/R4) | 1 | 1 alloc / 1 rel | **balanced** |
| **direct alias** `var b = a` (a live) | ✓ T1 on b | ✓ both a,b | 1 | 1 alloc / 2 ret-of-which-1-initial / 2 rel | **balanced** — verified `alias_loop.wyn`, ASan clean, RSS flat ~170MB |
| **alias `var b = a` (a dead)** | ✗ (move: a un-tracked) | ✓ b only | 1 | 1/1 | **balanced** |
| **`var b = f(a)`, f returns its arg** | ✗ (call assumed fresh) | ✓ both a,b | **1** (only `a`'s alloc; f returns `a`) | **1 alloc / 2 releases** | **IMBALANCED — over-release → UAF (#32)** |
| **`var b = f(a)`, f returns NEW string** | ✗ | ✓ both a,b | 2 (a + f's fresh) | 2/2 | **balanced** |
| **pass to fn** `f(a)` (param is `const char*`, borrowed) | ✗ | caller releases a at its scope | 1 | 1/1 | **balanced** (callee borrows, never releases param) |
| **return from fn** `return a` (a is param) | ✗ (no retain-on-return) | callee doesn't own param; caller frees the arg AND the "fresh" result | see #32 | | **IMBALANCED — this is the #32 mechanism** |
| **return local** `return s` (s local, fresh) | — | R4 skips s (liveness), caller owns it | 1 | 1/1 | **balanced** (move-out via liveness skip) |
| **store in struct field** `P{name: s}` | ✗ (field stores raw) | if s dead → **moved** (`codegen_stmt.c:2024-2041`); if live → s released at scope, field left dangling unless struct dies first | 1 | conditional | **balanced only via the dead-move heuristic**; struct does NOT own/release its string fields (struct `_cleanup` frees no string fields) |
| **push to array** `arr.push(s)` | ✓ T4 if live, else move | `array_free` releases elements (`wyn_runtime.h:625-629`) | 1 | 1/1 | **balanced via move-or-retain heuristic** |
| **reassign** `a = <fresh>` | ✗ | R5 releases old, unless buffer reused | 1 new | old rel / new owned | **balanced** |
| **reassign** `a = b` (shared) | ✓ R6 retain new | R6 release old | 0 new | bal | **balanced** |
| **enum payload** `Tree.Node(l,r)` | ✗ | **never** (`wyn_malloc` box, no free anywhere) | N boxes | N alloc / 0 free | **IMBALANCED — leak (#25)** |

The imbalanced rows are the bugs. Everything else is held together by the
move/copy liveness heuristics scattered through `codegen_stmt.c` (lines
1984-2095, 2127-2153) — each a targeted patch for a specific escape pattern
(struct field, Some/Ok/Err, enum-variant arg, returned enum arg).

---

## 4. The alias patterns that break

### 4.1 `var b = f(a)` where `f` returns its argument — **#32, UAF (over-release)**

Minimal repro (`/tmp/repro32.wyn`):
```wyn
fn passthru(s: string) -> string { return s }
fn main() {
    var i = 0
    while i < 2000000 {
        var a = "hello" + "world"    // fresh rc string, refcount=1
        var b = passthru(a)          // b == a (same pointer!)
        i = i + 1
    }
    print("done")
}
```
Generated C (`repro32.wyn.c`, loop body):
```c
const char* a = wyn_string_concat_safe("hello", "world");
const char* b = passthru(a);
i = (i + 1);
wyn_rc_release(a); wyn_rc_release(b);   /* String cleanup handled by ARC */  // R1
```
`passthru` is `return s;` (identity). `a` and `b` are the **same** rc pointer,
refcount only ever 1. Two releases → first frees, second is UAF.

**Why the heuristics miss it:** `b`'s initializer is `EXPR_CALL`, not `EXPR_IDENT`,
so the move/copy logic at `codegen_stmt.c:1985-1986` (which only fires for
`EXPR_IDENT` inits) never runs. The call result is registered as an independent
releasable owner (`codegen_stmt.c:1761-1766`). There is no analysis of what
`passthru` returns.

**ASan (verified):**
```
==ERROR: AddressSanitizer: heap-use-after-free ... READ of size 4
  #0 wyn_rc_release      (the atomic load of refcount on the freed header)
  #1 wyn_main
freed by:  wyn_rc_release <- wyn_main   (the first release)
allocated: wyn_str_alloc <- wyn_string_concat_safe
```
Confirmed with `clang -fsanitize=address -I src repro32.wyn.c
runtime/libwyn_rt_asan.a -lpthread -lm`.

### 4.2 `var b = a` (direct alias) — **balanced (control case)**

Generated C emits `wyn_rc_retain(b)` (T1) and releases both. ASan clean, RSS flat.
This is the *correct* shape the #32 fix must make `f(a)` match.

### 4.3 Enum payload boxing + match binding — **#25, leak (under-release)**

Minimal repro (`/tmp/tree.wyn`):
```wyn
enum Tree { Leaf(int)  Node(Tree, Tree) }
fn build(depth: int) -> Tree {
    if depth <= 0 { return Tree.Leaf(depth) }
    return Tree.Node(build(depth - 1), build(depth - 1))
}
fn main() {
    var i = 0
    while i < 200000 { var t = build(6); i = i + 1 }
    print("done")
}
```
Generated C:
```c
Tree Tree_Node(Tree f0, Tree f1) {
    Tree result; result.tag = Tree_Node_TAG;
    result.data.Node_value.f0 = (Tree*)wyn_malloc(sizeof(Tree));  // plain malloc, no RC header
    *result.data.Node_value.f0 = f0;
    result.data.Node_value.f1 = (Tree*)wyn_malloc(sizeof(Tree));
    *result.data.Node_value.f1 = f1;
    return result;
}
```
The enum value itself is a **by-value struct**; recursive payloads are heap
**boxes** (`wyn_malloc`, `wyn_runtime.h:42` — a bare `malloc`, **no rc header**).
Nothing ever frees them — grep for any enum-payload free returns nothing. `t`
goes out of scope each iteration and the whole subtree leaks.

**Measured:** RSS = **811 MB** for 200k iterations of depth-6 trees.

**Why the naive fix double-frees:** a recursive free at `t`'s scope exit works
*until* the tree is aliased. `Tree t2 = t` (or passing `t` by value, or storing it
in two fields) **copies the struct**, so `t` and `t2` share the same `f0`/`f1`
box pointers. Freeing both subtrees double-frees. Same root as #32: a copy is
treated as an independent owner with no shared-ownership accounting.

### 4.4 Struct field holding a string, then struct copied — **latent, same root**

`struct` `_cleanup` frees no string fields (only `WynArray.data`,
`codegen.c:1426-1435`). Structs do **not** own their string fields; the string
local that was stored is the sole owner and is released at its scope. The
`codegen_stmt.c:2024-2041` heuristic **moves** a dead local into the field to
avoid a scope-exit release that would dangle the field. But if the struct is
**copied by value** and both copies outlive the local, or two structs share one
string via copy, there is no retain — so a later `array_free`/manual release of
one copy's field can double-free the other's. Not the target of this task, but it
is the **same missing-alias-analysis root** and any general fix should subsume it.

### 4.5 Array of strings copied — **balanced within array, unbalanced across copies**

`array_push_str` stores the pointer raw and `array_free` releases each element
(`wyn_runtime.h:625-629,688-699`). Within one array this is balanced by the
push-transfer heuristic (T4/move). But `WynArray` copied by value shares
`data`, so two `array_free`s double-free — again the same root.

---

## 5. Proposed fix, evaluated

### Option A — retain-on-bind (ARC-canonical)

**Rule:** whenever codegen binds a new owner to an rc value that is *not a fresh
+1 producer*, emit `wyn_rc_retain`. Concretely, adopt the invariant **"a
function returning an rc type returns a +1 (owned) reference"**, enforced by
emitting `wyn_rc_retain(<retval>)` in the callee when it returns a *borrowed*
value (a parameter or an alias of one), and keep the caller's "result is an owner,
release once" assumption. Then:
- `var b = f(a)` where f is identity: f does `return (wyn_rc_retain(s), s)` →
  refcount becomes 2; caller releases `a` and `b` → back to 0. **Balanced.**
- `var b = f(a)` where f returns fresh: f's result is already +1; no extra retain
  (the return value is not a borrowed param) → 2 allocs, 2 releases. **Balanced.**

**Emission sites that change:** the function-return path
(`codegen_stmt.c` STMT_RETURN, around 2154-2224) gains a "if the returned
expression is a borrowed rc value (param, or ident aliasing a param, or a
field/element read that doesn't transfer) then `wyn_rc_retain` it before
returning." This requires the checker/codegen to answer **"is this return value a
freshly-owned +1, or a borrow?"** — a small, local escape question, far cheaper
than full liveness.

- **Risk of new leak (over-retain):** if we retain a return value that was
  *already* fresh (+1), the caller's single release leaves refcount 1 → leak.
  Mitigation: retain **only** when the returned expression is provably a borrow
  (an `EXPR_IDENT` resolving to a parameter, or a param passed through). Fresh
  producers (`EXPR_CALL` to a string builder, `EXPR_BINARY` concat, literals) are
  never retained.
- **Risk of new UAF (under-retain):** if a borrow-return is missed, we get #32
  back. Mitigation: make the classifier **conservative** — when unsure whether a
  return value is fresh or borrowed, retain (leak-leaning, not UAF-leaning). A
  leak is a latent bug; a UAF is a crash/security issue. Leak-lean is the correct
  default for a first cut.
- **Blast radius:** one new emission decision at STMT_RETURN for rc-typed returns,
  plus the classifier. ~1 codegen site changed; the caller side is unchanged
  (already "own the result, release once"). The 405 string-touching tests and
  `tests/memory/*` exercise this path.

### Option B — move-nulling

**Rule:** on every move, null the source pointer so only one live owner remains;
release is unconditional but a nulled pointer releases nothing.

- Requires mutating the source variable at every alias/return/store, and the
  compiler must prove single-threaded, no-later-use to null safely — i.e. it
  needs the **same liveness/alias analysis** it's trying to avoid, plus it breaks
  the "value semantics" users expect (`var b = a; print(a)` would read null).
  The existing model already *simulates* move by un-tracking (not nulling), which
  is safe precisely because it only drops a release, never nulls a live pointer.
- **Rejected** as the primary mechanism: null-on-move changes observable
  semantics and needs analysis Wyn doesn't have. (The existing "un-track dead
  source" is a compile-time-only pseudo-move and is fine to keep.)

### Option C — escape/liveness analysis

Full dataflow: compute, per rc value, the set of owners and escape points, insert
exactly balanced retain/release. Correct and general (subsumes all of §4), but
**large**: a new analysis pass over the AST/checker, and it is what stalled prior
attempts when attempted implicitly inside codegen. Defer as the long-term target.

### RECOMMENDATION — **Option A, conservative retain-on-return, leak-leaning.**

It directly closes #32 with a single, local, well-typed decision (the callee
knows its own return expression), matches the ARC convention ("+1 out"), and
composes with the existing caller-owns-result model without touching the ~12
release sites. Failure mode is a bounded leak (over-retain), never a UAF.

**How to prove balance:**
1. **ASan both directions.** For each pattern in §4, build with `--debug`, compile
   the `.wyn.c` against `runtime/libwyn_rt_asan.a`, run in a 1–2M-iteration loop:
   - #32 identity-return: must be **ASan-clean** (currently UAF).
   - fresh-return, direct-alias, pass-by-ref: must stay ASan-clean (no regression).
2. **RSS-flat.** Same loops under `/usr/bin/time -l`: max-RSS must stay flat
   (bounded), not grow with iteration count. Current #32 crashes; current #25 hits
   811 MB. Target: both flat (direct-alias control is ~170 MB flat today).
3. **Refcount assertion harness.** `wyn_rc_count` (`wyn_rc.c:101`) lets a test
   assert refcount returns to the pre-op value after a scope — add to
   `tests/memory/`.
4. **`make test`** (`run_bdd.sh`) green: 0 fail, no new leaks in the 405
   string tests.

---

## 6. Does the same fix generalize to the enum-payload leak (#25)?

**Partially — the *discipline* generalizes, but #25 needs one prerequisite that
strings already have: the payload must be an rc object.**

Today enum payload boxes are `wyn_malloc` (`Tree_Node`, `tree.wyn.c:26,28`) —
**plain malloc, no rc header**, so `wyn_rc_retain/release` are no-ops on them
(`wyn_rc_is_heap` fails: no magic). So you cannot apply retain-on-bind to them
as-is.

**If** enum payload boxes are allocated with `wyn_rc_alloc` instead of
`wyn_malloc` (giving each box a refcount), **then** the identical discipline
fixes #25 without double-free:
- **release at scope exit:** when an enum-typed local dies, emit a recursive
  release that `wyn_rc_release`s each box pointer (the box's own recursive
  payloads are released transitively when the box refcount hits 0 via a
  destructor, mirroring `array_free`'s element loop, `wyn_runtime.h:625-629`).
- **retain on bind/copy:** `var t2 = t` (struct copy sharing boxes) emits
  `wyn_rc_retain` on the shared box pointers, so the two scope-exit releases
  balance the one extra owner. Shared subtrees are freed **once**, when the last
  owner drops — **no double-free** (which is exactly what the naive recursive-free
  fix got wrong).

**Answer: YES**, retain-on-bind + release-at-scope fixes #25 **provided** enum
boxes become rc-allocated first. The two bugs are genuinely one root (missing
alias/ownership accounting); the *only* extra work for enums vs strings is
(a) switch `wyn_malloc` → `wyn_rc_alloc` for recursive payload boxes in the enum
constructor codegen, and (b) emit a per-enum-type recursive release helper. The
retain/move discipline is then identical to strings. Non-recursive / by-value
payloads (int, bool, non-boxed) are unaffected.

Caveat: this is more invasive than #32 (touches enum-constructor codegen +
adds a per-type destructor), so stage it **after** #32 proves the discipline on
strings.

---

## 7. Concrete staged implementation plan

**Stage 0 — lock in the evidence (no code).**
Land these repros as regression fixtures: `tests/memory/rc_identity_return.wyn`
(#32), `tests/memory/rc_enum_tree.wyn` (#25), plus the balanced controls
(direct-alias, fresh-return). Each with an `// EXPECT:` line and an ASan/RSS
check in a memory-suite script. **Verify:** #32 fixture currently ASan-fails, #25
fixture currently RSS-blows-up — these are the red tests.

**Stage 1 — #32 retain-on-return (Option A, strings only).**
1. Add a classifier `return_value_is_borrowed(Expr* ret, FnContext*)` answering:
   is the returned rc value a parameter (or an ident bound to one), i.e. a borrow
   the callee does not own? Conservative: unknown → treat as borrow → retain.
2. At STMT_RETURN for rc-typed (`const char*`) functions
   (`codegen_stmt.c` ~2198-2223), if borrowed, emit
   `return (wyn_rc_retain(<ret>), <ret>);` (or a temp) instead of `return <ret>;`.
3. Leave every existing release site (R1/R4/R5…) and the caller side unchanged.
**Verify:** #32 fixture ASan-clean + RSS-flat; direct-alias/fresh-return controls
still clean; `make` 0 warnings; `make test` 0 fail; spot-check generated C for the
identity case shows `wyn_rc_retain` before the return.

**Stage 2 — audit the borrow classifier against the escape heuristics.**
The existing move-out heuristics (struct field, Some/Ok/Err, enum-arg,
return-liveness in `codegen_stmt.c:1984-2153`) must not *both* move-out and
retain-on-return the same value (would leak). Reconcile: return-liveness skip (R4)
already avoids releasing a returned local; ensure retain-on-return fires only for
**borrowed** returns (params), which R4's liveness skip does not cover, so they
compose. **Verify:** targeted fixtures for `return E.A(local)`,
`return P{name: s}`, `return OptionString_Some(s)` stay balanced.

**Stage 3 — #25 enum payloads become rc-managed.**
1. In enum-constructor codegen, allocate recursive payload boxes with
   `wyn_rc_alloc(sizeof(T))` instead of `wyn_malloc` (find the emission that
   writes `(T*)wyn_malloc(sizeof(T))` for recursive variants).
2. Emit a per-enum-type recursive destructor (`__wyn_free_enum_<T>`) that
   `wyn_rc_release`s each box (release drops refcount; free-at-zero recurses via
   the destructor). Model the element loop on `array_free`
   (`wyn_runtime.h:621-633`).
3. Register enum-typed locals for scope-exit release (parallel to the string
   `string_var_releasable` machinery) and emit `__wyn_free_enum_<T>(&t)` at scope
   exit.
4. On enum copy/bind (`var t2 = t`, pass-by-value, store-in-field), emit a
   recursive `wyn_rc_retain` of the shared boxes (or move via the existing dead-
   source liveness check).
**Verify:** #25 fixture RSS-flat (target: bounded, not 811 MB); add an
alias fixture (`var t2 = t; use both`) that must be ASan-clean (proves no
double-free of shared subtrees); `make test` 0 fail.

**Stage 4 — generalize / defer.**
Struct-field-shared-string (§4.4) and array-copy-shared (§4.5) are the same root.
Do **not** hand-patch each; note them as the motivation for the eventual **Option
C** escape analysis and defer to a dedicated epic (this is where prior attempts
over-reached). Record in `internal-docs/ROADMAP.md`.

**Defer explicitly:** the legacy `arc_runtime.c` / `WynObject` system (dead for
strings/enums) — leave untouched; consider removal in a separate cleanup once
confirmed unreferenced.

---

## Appendix — reproduction commands (all verified this pass)

```bash
cd repos/wyn && make                       # build compiler (0 warnings)
make runtime-asan                          # build runtime/libwyn_rt_asan.a

# #32 UAF (currently crashes under ASan):
./wyn build /tmp/repro32.wyn --debug       # emits repro32.wyn.c
clang -fsanitize=address -I src -o /tmp/r32 /tmp/repro32.wyn.c \
      runtime/libwyn_rt_asan.a -lpthread -lm
/tmp/r32                                    # → heap-use-after-free in wyn_rc_release

# balanced control (direct alias, currently clean):
./wyn build /tmp/alias_loop.wyn --debug
clang -fsanitize=address -I src -o /tmp/ral /tmp/alias_loop.wyn.c \
      runtime/libwyn_rt_asan.a -lpthread -lm
/tmp/ral                                    # → clean, RSS ~170MB flat

# #25 leak (currently 811MB RSS):
./wyn build /tmp/tree.wyn --debug
/usr/bin/time -l /tmp/tree                  # → maximum resident set size ~811MB
```

### Key file:line index
- Primitives: `src/wyn_rc.c:16-105`, `src/wyn_rc.h:15-45`, `src/wyn_arena.c:7-20`
- Header mirror (concat): `src/wyn_runtime.h:211-246`
- Scope/tracking: `src/codegen.c:633-925` (esp. release R1 `648-661`, R4 `880-889`,
  move/copy consumers via `is_string_var`/`var_is_live_after`)
- Var-decl string reg + move/copy: `src/codegen_stmt.c:1761-2095`
- Return releases + enum-arg move: `src/codegen_stmt.c:2127-2224`
- Reassignment retain/release: `src/codegen_expr.c:4246-4287`
- Array push transfer: `src/codegen_expr.c:55-85`
- Block scope push/pop: `src/codegen_stmt.c:2308-2371`
- Enum payload box (leak site): generated `Tree_Node` via enum-constructor codegen
  (`wyn_malloc(sizeof(Tree))`); runtime `wyn_malloc` = `src/wyn_runtime.h:42`
- Array element release model (for enum destructor): `src/wyn_runtime.h:621-633`
```
