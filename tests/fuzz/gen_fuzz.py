#!/usr/bin/env python3
"""Grammar-aware fuzz generator for Wyn (T1.4).

Emits syntactically plausible Wyn programs from a weighted grammar, plus
mutation-based variants (token deletion/duplication/swap) of the generated
corpus. The harness (run_fuzz.sh) feeds them to `wyn check` / `wyn build`:
a crash (signal), a hang, or an "internal codegen error" is ALWAYS a bug -
the checker must reject bad programs cleanly, and accepted programs must
compile.

Deterministic per seed: `gen_fuzz.py <seed> <count> <outdir>`.

COVERAGE, and why it grew (2026-08-25). The oracle in run_fuzz.sh has always
asserted "check passes => build succeeds", and it gates through `make test`. But
this GENERATOR only emitted ints, strings, arrays, if/for and int->int helpers -
no structs, no enums, no match, no collections, no Option/Result. That is why it
reported 0 crashes while the whole #43 cluster was live, and why it caught none of
the seven check-passes/build-fails defects found on 2026-08-25 (#308-#314).

An oracle can only see what the generator writes. So the generator now emits the
constructs those defects actually lived in:

  * structs - fields of every scalar type plus a NESTED struct, field reads,
    passing across a function boundary, and "${s}" interpolation (#309)
  * enums - payload-less AND data-carrying, deliberately including variants named
    Ok / Err / Some / None, which are legal identifiers and were unusable (#311)
  * match - on enums via ALL THREE spellings (E::V, E.V, bare V), and on ints
  * HashMap / HashSet - typed setters in BOTH the namespace and method spellings
    (#310, #312), which is the key generalisation from that day: FOUR of the seven
    defects were "one spelling works and the other does not", so a generator that
    picks one spelling per construct is blind to half of them
  * Option / Result - `?` propagation across DIFFERENT ok-types, is_ok/unwrap,
    and match on Some/None

Generated programs are meant to be mostly VALID: their job is to exercise the
check-passes-must-build contract. The mutants (below) cover clean rejection.
"""
import random
import sys
import os

INT_OPS = ["+", "-", "*", "/", "%"]
CMP_OPS = ["==", "!=", "<", ">", "<=", ">="]
STR_METHODS = ["upper()", "lower()", "trim()", "len()", "reverse()"]
ARR_METHODS = ["len()", "sum()", "min()", "max()", "first()", "last()",
               "sort()", "reverse()", "unique()"]

NAMES = ["a", "b", "c", "x", "y", "z", "n", "m", "val", "acc", "tmp",
         "item", "total", "count", "res"]

# Variant names deliberately include the Option/Result constructor spellings.
# The lexer dropped TOKEN_OK/TOKEN_ERR so these ARE ordinary identifiers, and all
# four were unusable as enum variants until #311 - a defect a generator using only
# neutral names can never produce.
VARIANT_NAMES = ["Alpha", "Beta", "Gamma", "Ok", "Err", "Some", "None",
                 "Red", "Green", "Warm", "Cold", "Over", "Under"]
FIELD_NAMES = ["x", "y", "n", "name", "label", "size", "flag", "ratio", "count"]
SCALAR_FIELD_TYPES = ["int", "string", "float", "bool"]


class Gen:
    def __init__(self, rng):
        self.r = rng
        self.depth = 0
        self.vars_int = []
        self.vars_str = []
        self.vars_arr = []
        self.fns = []
        # Declared top-level types, and locals holding values of them.
        self.structs = []      # [(Name, [(field, type)])]
        self.enums = []        # [(Name, [(Variant, payload_type_or_None)])]
        self.vars_struct = []  # [(varname, StructName)]
        self.vars_enum = []    # [(varname, EnumName)]
        self.vars_map = []
        self.vars_set = []
        self.res_fns = []      # [(fname, ok_type)]
        self.opt_fns = []      # [(fname, payload_type)]

    def name(self, pool):
        # fresh name not colliding across pools. EVERY pool must be here: a name
        # handed out for a struct/map/set and then re-picked as an int produced
        # "Type mismatch in assignment", which the checker rightly rejects - and a
        # rejected program teaches the build oracle nothing.
        used = (set(self.vars_int) | set(self.vars_str) | set(self.vars_arr) |
                set(self.fns) | set(self.vars_map) | set(self.vars_set) |
                set(n for n, _t in self.vars_struct) | set(n for n, _t in self.vars_enum))
        cands = [n for n in NAMES if n not in used]
        if not cands:
            n = "v%d" % self.r.randrange(1000)
            while n in used:
                n = "v%d" % self.r.randrange(1000)
            return n
        return self.r.choice(cands)

    # ---- expressions ----
    def int_expr(self):
        self.depth += 1
        try:
            roll = self.r.random()
            if self.depth > 4 or roll < 0.35:
                return str(self.r.randrange(-100, 1000))
            if roll < 0.5 and self.vars_int:
                return self.r.choice(self.vars_int)
            if roll < 0.7:
                return "(%s %s %s)" % (self.int_expr(),
                                       self.r.choice(INT_OPS), self.int_expr())
            if roll < 0.8 and self.vars_str:
                return "%s.len()" % self.r.choice(self.vars_str)
            if roll < 0.9 and self.vars_arr:
                return "%s.%s" % (self.r.choice(self.vars_arr),
                                  self.r.choice(["len()", "sum()"]))
            return str(self.r.randrange(0, 100))
        finally:
            self.depth -= 1

    def str_expr(self):
        self.depth += 1
        try:
            roll = self.r.random()
            base = '"%s"' % "".join(self.r.choice("abcxyz 123_") for _ in range(self.r.randrange(0, 8)))
            if self.depth > 4 or roll < 0.4:
                return base
            if roll < 0.55 and self.vars_str:
                return self.r.choice(self.vars_str)
            if roll < 0.7:
                return "(%s + %s)" % (self.str_expr(), self.str_expr())
            if roll < 0.85 and self.vars_str:
                return "%s.%s" % (self.r.choice(self.vars_str),
                                  self.r.choice([m for m in STR_METHODS if m != "len()"]))
            if roll < 0.95:
                return '"v=${%s}"' % (self.r.choice(self.vars_int) if self.vars_int else self.int_expr())
            return base
        finally:
            self.depth -= 1

    def bool_expr(self):
        return "%s %s %s" % (self.int_expr(), self.r.choice(CMP_OPS), self.int_expr())

    def arr_expr(self):
        items = ", ".join(self.int_expr() for _ in range(self.r.randrange(1, 6)))
        return "[%s]" % items


    # ---- type declarations ----
    def decl_struct(self, idx):
        """A struct with scalar fields and, sometimes, a NESTED struct field.

        Nesting matters: a struct-typed field is what forced __wyn_str_ to be
        two-pass (prototypes first) in #309, and what the println emitter still
        cannot render."""
        name = "S%d" % idx
        fields = []
        used = set()
        for _ in range(self.r.randrange(1, 4)):
            f = self.r.choice([n for n in FIELD_NAMES if n not in used] or ["f%d" % len(used)])
            used.add(f)
            if self.structs and self.r.random() < 0.25:
                fields.append((f, self.structs[self.r.randrange(len(self.structs))][0]))
            else:
                fields.append((f, self.r.choice(SCALAR_FIELD_TYPES)))
        self.structs.append((name, fields))
        body = ", ".join("%s: %s" % (f, t) for f, t in fields)
        return "struct %s { %s }\n" % (name, body)

    def decl_enum(self, idx):
        name = "E%d" % idx
        variants = []
        used = set()
        for _ in range(self.r.randrange(2, 4)):
            cands = [v for v in VARIANT_NAMES if v not in used]
            if not cands:
                break
            v = self.r.choice(cands)
            used.add(v)
            # A data-carrying variant roughly a third of the time. Mixing them in
            # one enum is the shape that broke #298/#311.
            payload = self.r.choice(["int", "string"]) if self.r.random() < 0.3 else None
            variants.append((v, payload))
        if not variants:
            variants = [("Alpha", None), ("Beta", None)]
        self.enums.append((name, variants))
        parts = [(v if p is None else "%s(%s)" % (v, p)) for v, p in variants]
        return "enum %s { %s }\n" % (name, ", ".join(parts))

    def scalar_of(self, t):
        if t == "int":
            return self.int_expr()
        if t == "string":
            return self.str_expr()
        if t == "float":
            return "%d.%d" % (self.r.randrange(0, 40), self.r.randrange(0, 99))
        if t == "bool":
            return self.r.choice(["true", "false"])
        # a struct-typed field: build one inline
        return self.struct_literal(t)

    def struct_literal(self, name):
        for sname, fields in self.structs:
            if sname == name:
                inner = ", ".join("%s: %s" % (f, self.scalar_of(t)) for f, t in fields)
                return "%s { %s }" % (name, inner)
        return "0"

    def variant_expr(self, ename):
        """Pick a variant and one of the THREE spellings.

        `E::V`, `E.V` and bare `V` are all accepted, and they do NOT lower
        identically - the qualified forms failed while bare worked (#311). A
        generator that always picks one spelling is blind to that whole class."""
        for name, variants in self.enums:
            if name == ename:
                v, payload = variants[self.r.randrange(len(variants))]
                style = self.r.random()
                # `Some` and `None` are lexer keywords, so a BARE reference to a
                # variant with those names cannot parse. The qualified spellings
                # accept them, and those are the ones that were broken (#311).
                bare_ok = v not in ("Some", "None", "Ok", "Err")
                if payload is None:
                    if style < 0.45 or not bare_ok:
                        return "%s::%s" % (name, v), v, payload
                    if style < 0.8:
                        return "%s.%s" % (name, v), v, payload
                    return v, v, payload
                arg = self.int_expr() if payload == "int" else self.str_expr()
                if style < 0.5:
                    return "%s::%s(%s)" % (name, v, arg), v, payload
                return "%s.%s(%s)" % (name, v, arg), v, payload
        return "0", None, None

    def match_enum(self, indent, var, ename):
        """Exhaustive match over every variant, arms in a randomly chosen spelling."""
        pad = "    " * indent
        for name, variants in self.enums:
            if name != ename:
                continue
            style = self.r.random()
            arms = []
            # A bare arm is only emitted when EVERY variant can be spelled bare -
            # mixing spellings inside one match is legal but the bare form cannot
            # express a keyword-named variant.
            all_bare_ok = all(v not in ("Some", "None", "Ok", "Err") for v, _p in variants)
            for v, payload in variants:
                if payload is None:
                    if style < 0.4 or not all_bare_ok:
                        pat = "%s::%s" % (name, v)
                    elif style < 0.75:
                        pat = "%s.%s" % (name, v)
                    else:
                        pat = v
                else:
                    b = "p%d" % self.r.randrange(100)
                    pat = ("%s::%s(%s)" % (name, v, b)) if style < 0.5 else ("%s.%s(%s)" % (name, v, b))
                arms.append('%s    %s => { println("%s") }' % (pad, pat, v))
            return "%smatch %s {\n%s\n%s}" % (pad, var, "\n".join(arms), pad)
        return "%sprintln(0)" % pad

    # ---- statements ----
    def stmt(self, indent):
        pad = "    " * indent
        roll = self.r.random()
        if roll < 0.22:
            n = self.name(self.vars_int)
            self.vars_int.append(n)
            return "%s%s = %s" % (pad, n, self.int_expr())
        if roll < 0.38:
            n = self.name(self.vars_str)
            self.vars_str.append(n)
            return "%s%s = %s" % (pad, n, self.str_expr())
        if roll < 0.48:
            n = self.name(self.vars_arr)
            self.vars_arr.append(n)
            return "%s%s = %s" % (pad, n, self.arr_expr())
        if roll < 0.58 and self.vars_int:
            v = self.r.choice(self.vars_int)
            return "%s%s = %s" % (pad, v, self.int_expr())
        if roll < 0.68:
            arg = self.r.choice([self.int_expr(), self.str_expr(),
                                 "%s.to_string()" % self.int_expr()])
            return "%sprintln(%s)" % (pad, arg)
        if roll < 0.78 and indent < 3:
            body = self.scoped_block(indent + 1)
            els = ""
            if self.r.random() < 0.5:
                els = "%s else {\n%s\n%s}" % ("", self.scoped_block(indent + 1), pad)
            return "%sif %s {\n%s\n%s}%s" % (pad, self.bool_expr(), body, pad, els)
        if roll < 0.86 and indent < 3:
            v = self.name(self.vars_int)
            self.vars_int.append(v)
            body = self.scoped_block(indent + 1)
            self.vars_int.remove(v)
            return "%sfor %s in 0..%d {\n%s\n%s}" % (pad, v, self.r.randrange(1, 10), body, pad)
        if roll < 0.90 and self.vars_arr:
            a = self.r.choice(self.vars_arr)
            lam = self.r.choice(["(v) => v * 2", "(v) => v + 1", "|v| v - 1"])
            return "%sprintln(%s.map(%s).sum().to_string())" % (pad, a, lam)

        # --- constructs added 2026-08-25; see the module docstring ---
        if roll < 0.925 and self.structs:
            sname = self.structs[self.r.randrange(len(self.structs))][0]
            v = self.name(self.vars_int)
            self.vars_struct.append((v, sname))
            lines = ["%s%s = %s" % (pad, v, self.struct_literal(sname))]
            # Interpolating a struct is the #309 shape; reading a field and
            # passing it across a boundary are the S3/S6 shapes.
            if self.r.random() < 0.6:
                lines.append('%sprintln("s=${%s}")' % (pad, v))
            for fname, ftype in self.structs[[n for n, _ in self.structs].index(sname)][1]:
                if self.r.random() < 0.4:
                    if ftype in ("int", "float"):
                        lines.append("%sprintln(%s.%s)" % (pad, v, fname))
                    elif ftype == "string":
                        lines.append("%sprintln(%s.%s.len())" % (pad, v, fname))
                    elif ftype == "bool":
                        lines.append('%sif %s.%s { println("t") }' % (pad, v, fname))
            return "\n".join(lines)

        if roll < 0.95 and self.enums:
            ename = self.enums[self.r.randrange(len(self.enums))][0]
            expr, _v, _p = self.variant_expr(ename)
            v = self.name(self.vars_int)
            self.vars_enum.append((v, ename))
            return "%s%s = %s\n%s" % (pad, v, expr, self.match_enum(indent, v, ename))

        if roll < 0.975:
            # BOTH spellings of the same collection write. Four of the seven
            # 2026-08-25 defects were one-spelling-works, so the spelling is
            # itself a fuzz dimension.
            m = self.name(self.vars_int)
            self.vars_map.append(m)
            k = self.str_expr()
            lines = ["%s%s = HashMap.new()" % (pad, m)]
            if self.r.random() < 0.5:
                lines.append("%sHashMap.set_int(%s, %s, %s)" % (pad, m, k, self.int_expr()))
            else:
                lines.append("%s%s.set_int(%s, %s)" % (pad, m, k, self.int_expr()))
            if self.r.random() < 0.5:
                lines.append("%sHashMap.set(%s, %s, %s)" % (pad, m, self.str_expr(), self.str_expr()))
            lines.append("%sprintln(HashMap.len(%s))" % (pad, m))
            return "\n".join(lines)

        if roll < 0.99:
            st = self.name(self.vars_int)
            self.vars_set.append(st)
            return ("%s%s = HashSet.new()\n%sHashSet.add(%s, %s)\n%sprintln(HashSet.contains(%s, %s))"
                    % (pad, st, pad, st, self.str_expr(), pad, st, self.str_expr()))

        return "%sprintln(%s)" % (pad, self.int_expr())

    def block(self, indent):
        return "\n".join(self.stmt(indent) for _ in range(self.r.randrange(1, 4)))

    def scoped_block(self, indent):
        """A block whose declarations do NOT escape into the enclosing scope.

        This was the single biggest reason generated programs were rejected: a
        variable declared inside an if/for body was appended to the pools and
        stayed there, so a LATER statement at outer scope referenced it and the
        checker (correctly) said "Undefined variable". Most of the corpus was
        therefore thrown away at the check stage, and the check-passes =>
        build-succeeds oracle only ever ran on a handful of programs - it reported
        "0 crashes" over almost nothing. Snapshotting the pools is what makes the
        generated corpus mostly VALID, which is what gives the oracle something to
        assert on."""
        snap = (list(self.vars_int), list(self.vars_str), list(self.vars_arr),
                list(self.vars_map), list(self.vars_set),
                list(self.vars_struct), list(self.vars_enum))
        try:
            return self.block(indent)
        finally:
            (self.vars_int, self.vars_str, self.vars_arr, self.vars_map,
             self.vars_set, self.vars_struct, self.vars_enum) = snap

    def fn(self, idx):
        fname = "fn_%d" % idx
        self.fns.append(fname)
        # int -> int helper
        return ("fn %s(p: int) -> int {\n    return %s\n}\n"
                % (fname, self.int_expr().replace("(", "(", 1)))

    def fn_struct(self, idx, sname):
        """A struct crossing a function boundary in BOTH directions - the shape of
        S3 (`fn -> [Struct]` then field access) and #309."""
        fname = "mk_%d" % idx
        fields = self.structs[[n for n, _ in self.structs].index(sname)][1]
        take = "fn take_%d(v: %s) -> int { return %s }\n" % (
            idx, sname,
            next(("v.%s" % f for f, t in fields if t == "int"), "1"))
        return ("fn %s() -> %s {\n    return %s\n}\n%s"
                % (fname, sname, self.struct_literal(sname), take)), fname, "take_%d" % idx

    def fn_result(self, idx):
        """Result-returning pair whose ok-types DIFFER, so `?` has to re-wrap
        across families - the S4 shape, which the simplest test of `?` misses."""
        ok = self.r.choice(["int", "string"])
        inner = "res_in_%d" % idx
        outer = "res_out_%d" % idx
        self.res_fns.append((outer, ok))
        val = self.int_expr() if ok == "int" else self.str_expr()
        src = ("fn %s(f: bool) -> Result<int, string> {\n"
               "    if f { return Err(\"bad\") }\n"
               "    return Ok(%s)\n}\n" % (inner, self.r.randrange(0, 50)))
        if ok == "int":
            src += ("fn %s(f: bool) -> Result<int, string> {\n"
                    "    var v = %s(f)?\n"
                    "    return Ok(v + %s)\n}\n" % (outer, inner, self.r.randrange(1, 9)))
        else:
            src += ("fn %s(f: bool) -> Result<string, string> {\n"
                    "    var v = %s(f)?\n"
                    "    return Ok(\"v${v}\")\n}\n" % (outer, inner))
        return src

    def fn_option(self, idx):
        fname = "opt_%d" % idx
        self.opt_fns.append((fname, "int"))
        return ("fn %s(f: bool) -> int? {\n"
                "    if f { return Some(%s) }\n"
                "    return None\n}\n" % (fname, self.r.randrange(0, 50)))

    def program(self):
        saved = (self.vars_int, self.vars_str, self.vars_arr)
        parts = []
        # Type declarations come first so every later reference resolves.
        for i in range(self.r.randrange(0, 3)):
            parts.append(self.decl_struct(i))
        for i in range(self.r.randrange(0, 3)):
            parts.append(self.decl_enum(i))
        for i in range(self.r.randrange(0, 3)):
            self.vars_int, self.vars_str, self.vars_arr = ["p"], [], []
            parts.append(self.fn(i))
        struct_fns = []
        if self.structs and self.r.random() < 0.6:
            sname = self.structs[self.r.randrange(len(self.structs))][0]
            src, mk, take = self.fn_struct(0, sname)
            parts.append(src)
            struct_fns.append((mk, take))
        if self.r.random() < 0.5:
            parts.append(self.fn_result(0))
        if self.r.random() < 0.4:
            parts.append(self.fn_option(0))

        self.vars_int, self.vars_str, self.vars_arr = [], [], []
        body_lines = [self.stmt(1) for _ in range(self.r.randrange(3, 12))]
        for f in self.fns:
            if self.r.random() < 0.5:
                body_lines.append("    println(%s(%s).to_string())" % (f, self.r.randrange(0, 50)))
        for mk, take in struct_fns:
            body_lines.append("    println(%s(%s()))" % (take, mk))
        for fname, ok in self.res_fns:
            body_lines.append("    var r_%s = %s(%s)" % (fname, fname, self.r.choice(["true", "false"])))
            body_lines.append("    if r_%s.is_ok() { println(r_%s.unwrap()) } else { println(\"e\") }"
                              % (fname, fname))
        for fname, _p in self.opt_fns:
            body_lines.append("    match %s(%s) {\n        Some(v) => { println(v) }\n"
                              "        None => { println(\"none\") }\n    }"
                              % (fname, self.r.choice(["true", "false"])))
        parts.append("fn main() {\n%s\n}\n" % "\n".join(body_lines))
        self.vars_int, self.vars_str, self.vars_arr = saved
        return "\n".join(parts)


def mutate(src, rng):
    """Token-level mutation: delete/dup/swap a random small slice."""
    lines = src.split("\n")
    if len(lines) < 3:
        return src
    op = rng.random()
    i = rng.randrange(len(lines))
    if op < 0.35:
        del lines[i]                                   # delete a line
    elif op < 0.6:
        lines.insert(i, lines[i])                      # duplicate a line
    elif op < 0.8 and len(lines) >= 2:
        j = rng.randrange(len(lines))
        lines[i], lines[j] = lines[j], lines[i]        # swap two lines
    else:
        # character-level chaos inside one line
        ln = lines[i]
        if ln:
            k = rng.randrange(len(ln))
            ch = rng.choice(["}", "{", "(", ")", '"', "+", "=", ""])
            lines[i] = ln[:k] + ch + ln[k:]
    return "\n".join(lines)


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    outdir = sys.argv[3] if len(sys.argv) > 3 else "fuzz_out"
    os.makedirs(outdir, exist_ok=True)
    rng = random.Random(seed)
    n_valid = count * 2 // 3
    for i in range(n_valid):
        g = Gen(random.Random(seed * 100003 + i))
        with open(os.path.join(outdir, "gen_%04d.wyn" % i), "w") as f:
            f.write(g.program())
    # mutants of the valid corpus - likely-invalid programs the CHECKER must
    # reject cleanly (no crash, no hang, no C-compiler leak-through).
    for i in range(count - n_valid):
        src_i = rng.randrange(n_valid)
        with open(os.path.join(outdir, "gen_%04d.wyn" % src_i)) as f:
            src = f.read()
        m = mutate(src, rng)
        if rng.random() < 0.4:
            m = mutate(m, rng)
        with open(os.path.join(outdir, "mut_%04d.wyn" % i), "w") as f:
            f.write(m)
    print("generated %d programs (%d valid-ish, %d mutants) in %s"
          % (count, n_valid, count - n_valid, outdir))


if __name__ == "__main__":
    main()
