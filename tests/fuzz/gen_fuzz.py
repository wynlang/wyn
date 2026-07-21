#!/usr/bin/env python3
"""Grammar-aware fuzz generator for Wyn (T1.4).

Emits syntactically plausible Wyn programs from a weighted grammar, plus
mutation-based variants (token deletion/duplication/swap) of the generated
corpus. The harness (run_fuzz.sh) feeds them to `wyn check` / `wyn build`:
a crash (signal), a hang, or an "internal codegen error" is ALWAYS a bug -
the checker must reject bad programs cleanly, and accepted programs must
compile.

Deterministic per seed: `gen_fuzz.py <seed> <count> <outdir>`.
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


class Gen:
    def __init__(self, rng):
        self.r = rng
        self.depth = 0
        self.vars_int = []
        self.vars_str = []
        self.vars_arr = []
        self.fns = []

    def name(self, pool):
        # fresh name not colliding across pools
        used = set(self.vars_int) | set(self.vars_str) | set(self.vars_arr) | set(self.fns)
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
            body = self.block(indent + 1)
            els = ""
            if self.r.random() < 0.5:
                els = "%s else {\n%s\n%s}" % ("", self.block(indent + 1), pad)
            return "%sif %s {\n%s\n%s}%s" % (pad, self.bool_expr(), body, pad, els)
        if roll < 0.86 and indent < 3:
            v = self.name(self.vars_int)
            self.vars_int.append(v)
            body = self.block(indent + 1)
            self.vars_int.remove(v)
            return "%sfor %s in 0..%d {\n%s\n%s}" % (pad, v, self.r.randrange(1, 10), body, pad)
        if roll < 0.93 and self.vars_arr:
            a = self.r.choice(self.vars_arr)
            lam = self.r.choice(["(v) => v * 2", "(v) => v + 1", "|v| v - 1"])
            return "%sprintln(%s.map(%s).sum().to_string())" % (pad, a, lam)
        return "%sprintln(%s)" % (pad, self.int_expr())

    def block(self, indent):
        return "\n".join(self.stmt(indent) for _ in range(self.r.randrange(1, 4)))

    def fn(self, idx):
        fname = "fn_%d" % idx
        self.fns.append(fname)
        # int -> int helper
        return ("fn %s(p: int) -> int {\n    return %s\n}\n"
                % (fname, self.int_expr().replace("(", "(", 1)))

    def program(self):
        saved = (self.vars_int, self.vars_str, self.vars_arr)
        parts = []
        for i in range(self.r.randrange(0, 3)):
            self.vars_int, self.vars_str, self.vars_arr = ["p"], [], []
            parts.append(self.fn(i))
        self.vars_int, self.vars_str, self.vars_arr = [], [], []
        body_lines = [self.stmt(1) for _ in range(self.r.randrange(3, 12))]
        for f in self.fns:
            if self.r.random() < 0.5:
                body_lines.append("    println(%s(%s).to_string())" % (f, self.r.randrange(0, 50)))
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
