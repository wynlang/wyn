#!/usr/bin/env python3
"""Re-measure the published benchmark rows from a source checkout.

This is the harness behind the numbers on the benchmarks page: the
`fork`/`exec`/`wait4` process timer (bench_exec.c), the in-process microsecond
fixtures in fx/, and the generated compile-time scaling fixtures. It exists so a
release re-measurement is a command instead of a reconstruction from prose.

It reports numbers. It does NOT store them, and nothing in this directory
asserts a result: the published figures live on the benchmarks page, and
duplicating them here is how the two copies drifted by 5x in the first place.

NOT A GATE. It measures the machine as much as the compiler, so a red number
here is not a test failure. Run it on an IDLE machine - a parallel build or test
run roughly halves every result - and prefer the DIFFERENTIAL between two
compilers measured back to back over any absolute value.

Usage:
    benchmarks/harness/run_bench.py                       # ./wyn in this checkout
    benchmarks/harness/run_bench.py rc=./wyn rel=/opt/wyn-1.21.0/bin/wyn
    benchmarks/harness/run_bench.py --quick               # fewer reps, smoke only
    benchmarks/harness/run_bench.py --only sort,fib35
    benchmarks/harness/run_bench.py --skip-scale          # no compile-time table

Each labelled compiler gets its OWN copy of the fixtures: `wyn build` writes the
generated C next to the source, so a shared directory has two compilers
overwriting each other's intermediates.
"""
import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import time

HARNESS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HARNESS))
FX = os.path.join(HARNESS, "fx")
GEN = os.path.join(HARNESS, "gen_scale_fixture.py")
OUTDIR = os.path.join(HARNESS, "results")

# In-process fixtures: the program prints its own `label us=N` timings, so the
# median is taken over the PRINTED values, not over process wall clock. reps is
# low for the slow ones (naive concat is ~11s a run).
INPROC = [
    ("sort",          7),
    ("str_sb",        7),
    ("str_len",       7),
    ("str_chain",     7),
    ("conc_seq",      5),
    ("conc_fire1m",   5),
    ("conc_sleep",    5),
    ("conc_cpu",      5),
    ("conc_parallel", 5),
    ("str_concat",    3),
]

# Process-level fixtures: whole-process wall clock via bench_exec, which is what
# the page's "Compute" table reports (startup floor included, not subtracted).
PROC = [("hello", 15), ("fib35", 15)]

# Compile-time scaling. Approximate line counts; gen_scale_fixture.py reports
# the exact size it wrote, and that size is recorded alongside the timing. The
# hello-world row uses fx/hello.wyn, not a generated unit: at that size the
# number is process startup, and the page says so rather than calling it a
# check-speed figure.
SCALE_LINES = [1063, 5065]
SCALE_REPS = 11


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def ensure_bench_exec():
    """Build the fork/exec/wait4 driver if it is missing or older than its source."""
    exe = os.path.join(OUTDIR, "bench_exec")
    src = os.path.join(HARNESS, "bench_exec.c")
    if os.path.exists(exe) and os.path.getmtime(exe) >= os.path.getmtime(src):
        return exe
    cc = os.environ.get("CC", "cc")
    r = sh([cc, "-O2", "-std=c11", "-o", exe, src])
    if r.returncode != 0:
        sys.exit("bench_exec failed to build:\n" + r.stdout + r.stderr)
    return exe


def build_fixtures(wyn, label, only):
    """Copy fx/ for this compiler and build each fixture with --release."""
    outdir = os.path.join(OUTDIR, label)
    shutil.rmtree(outdir, ignore_errors=True)
    shutil.copytree(FX, outdir)
    env = dict(os.environ, TMPDIR=os.path.join(OUTDIR, "tmp", label))
    os.makedirs(env["TMPDIR"], exist_ok=True)
    built, failed = {}, {}
    for name, _ in INPROC + PROC:
        if only and name not in only:
            continue
        src = os.path.join(outdir, name + ".wyn")
        r = sh([wyn, "build", src, "--release"], env=env)
        exe = os.path.join(outdir, name)
        if r.returncode != 0 or not os.path.exists(exe):
            failed[name] = (r.stdout + r.stderr)[-400:]
        else:
            built[name] = exe
    return built, failed, env, outdir


def median_printed(exe, reps, env):
    """Run exe reps+1 times and collect every `label us=N` pair it prints,
    returning the median per label. The FIRST run is always discarded: macOS
    scans a freshly built binary on first exec (7.2s vs 0.16s observed on the
    same 75KB binary), which would otherwise dominate the sample."""
    rows = {}
    for i in range(reps + 1):
        r = sh([exe], env=env)
        if i == 0:
            continue
        if r.returncode != 0:
            rows.setdefault("__nonzero_exit__", []).append(r.returncode)
            continue
        for line in r.stdout.splitlines():
            m = re.match(r"^(.*?)\s*us=(\d+)(.*)$", line.strip())
            if m:
                rows.setdefault(m.group(1).strip(), []).append(int(m.group(2)))
    meds = {k: statistics.median(v) / 1000.0
            for k, v in rows.items() if v and k != "__nonzero_exit__"}
    return meds, rows


def proc_timing(bench_exec, argv, reps, env, discard=2):
    r = sh([bench_exec, str(reps), str(discard), "--"] + argv, env=env)
    try:
        return json.loads(r.stdout.strip().splitlines()[-1])
    except Exception:
        return {"error": (r.stdout + r.stderr)[-300:]}


def scale_fixtures(wyn, label, env, bench_exec, reps):
    """Generate a realistic program at each size and time check / build /
    build --release on it. `wyn check` scales with DECLARATION count, not lines,
    so the unit and declaration counts are recorded with every row."""
    rows = {}
    base = os.path.join(OUTDIR, label, "scale")
    os.makedirs(base, exist_ok=True)
    for lines in ["hello"] + SCALE_LINES:
        d = os.path.join(base, f"n{lines}")
        os.makedirs(d, exist_ok=True)
        src = os.path.join(d, "scale.wyn")
        if lines == "hello":
            shutil.copyfile(os.path.join(FX, "hello.wyn"), src)
            meta = {"out": src, "units": 0, "declarations": 1,
                    "lines": len(open(src).read().splitlines()),
                    "note": "hello world - this row is process startup, not check speed"}
        else:
            g = sh([sys.executable, GEN, "--lines", str(lines), "-o", src])
            if g.returncode != 0:
                rows[lines] = {"error": g.stderr[-300:]}
                continue
            meta = json.loads(g.stdout.strip().splitlines()[-1])
        row = {"fixture": meta}
        for mode, argv in (
            ("check",   [wyn, "check", src]),
            ("build",   [wyn, "build", src, "-o", os.path.join(d, "b")]),
            ("release", [wyn, "build", src, "--release", "-o", os.path.join(d, "r")]),
        ):
            row[mode] = proc_timing(bench_exec, argv, reps, env)
        rows[lines] = row
        print(f"  scale {meta['lines']:>5} lines / {meta['declarations']:>4} decls  "
              f"check {row['check'].get('median_ms')}ms  "
              f"build {row['build'].get('median_ms')}ms  "
              f"release {row['release'].get('median_ms')}ms", flush=True)
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("compilers", nargs="*", default=[],
                    help="label=path pairs; default local=<repo>/wyn")
    ap.add_argument("--only", default="", help="comma-separated fixture names")
    ap.add_argument("--quick", action="store_true", help="fewer reps; smoke only")
    ap.add_argument("--skip-scale", action="store_true",
                    help="skip the compile-time scaling table")
    ap.add_argument("--scale-only", action="store_true",
                    help="only the compile-time scaling table")
    ap.add_argument("--out", default=os.path.join(OUTDIR, "results.json"))
    a = ap.parse_args()

    only = set(f for f in a.only.split(",") if f)
    targets = []
    for spec in a.compilers or [f"local={os.path.join(REPO, 'wyn')}"]:
        if "=" not in spec:
            sys.exit(f"expected label=path, got {spec!r}")
        label, path = spec.split("=", 1)
        path = os.path.abspath(path)
        if not os.access(path, os.X_OK):
            sys.exit(f"{label}: {path} is not executable (run `make` first?)")
        targets.append((label, path))

    os.makedirs(OUTDIR, exist_ok=True)
    bench_exec = ensure_bench_exec()
    scale_reps = 3 if a.quick else SCALE_REPS

    out = {"harness": "benchmarks/harness", "repo": REPO, "targets": {}}
    for label, wyn in targets:
        ver = sh([wyn, "version"]).stdout.strip().splitlines()
        print(f"\n{'='*70}\n== {label}: {wyn}\n== {ver[-1] if ver else '?'}\n{'='*70}",
              flush=True)
        t0 = time.time()
        built, failed, env, _ = build_fixtures(
            wyn, label, {"__none__"} if a.scale_only else only)
        print(f"built {len(built)} fixtures in {time.time()-t0:.0f}s", flush=True)
        for n, why in failed.items():
            print(f"  BUILD FAILED {n}: {why}", flush=True)
        res = {"wyn": wyn, "version": ver[-1] if ver else None,
               "build_failed": list(failed), "inproc": {}, "proc": {},
               "binary_size_bytes": {}, "scale": {}}
        for name, reps in INPROC:
            if name not in built:
                continue
            if a.quick:
                reps = max(1, reps // 3)
            meds, raw = median_printed(built[name], reps, env)
            res["inproc"][name] = meds
            for k, v in sorted(meds.items()):
                spread = max(raw[k]) / max(min(raw[k]), 1)
                print(f"  {name:15s} {k:42s} {v:10.3f} ms  (n={len(raw[k])}, "
                      f"max/min={spread:.2f})", flush=True)
        for name, reps in PROC:
            if name not in built:
                continue
            if a.quick:
                reps = max(3, reps // 3)
            res["proc"][name] = proc_timing(bench_exec, [built[name]], reps, env)
            print(f"  {name:15s} whole-process {res['proc'][name]}", flush=True)
        for name, exe in built.items():
            res["binary_size_bytes"][name] = os.path.getsize(exe)
        if not a.skip_scale and (a.scale_only or not only):
            res["scale"] = scale_fixtures(wyn, label, env, bench_exec, scale_reps)
        out["targets"][label] = res

    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)
    print(f"\nwrote {a.out}", flush=True)
    print("Reminder: max/min well above 1.0 on any row means the machine was "
          "not idle. Re-run before quoting anything.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
