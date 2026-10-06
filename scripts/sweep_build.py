#!/usr/bin/env python3
"""Pass 1b / Pass 2b: BUILD and RUN every example under both compilers and compare
exit status *and stdout*.

`wyn check` cannot see a codegen regression - the defects this sweep was written for
were either invalid generated C or a silently wrong value, and check passes both. So
each file is compiled by both compilers and, if both produce a binary, both binaries
are run and their output compared byte for byte.

Each compiler gets its OWN COPY of every source file. `wyn build` writes the
generated C next to the source and the binary next to it too, so two compilers
pointed at one tree overwrite each other's intermediates.

A program that times out under BOTH compilers is "same" - plenty of examples are
servers or interactive. A program that times out under only one is a finding ONCE THE
TIMING-OUT SIDE REPRODUCES IT (that sentence was in this docstring while the code
excluded every TIMEOUT from the output comparison, so it was false until 2026-10-05;
and the first version of the fix reported it without re-running the side that timed
out, which manufactured a regression in a self-proof run).

A TIMEOUT IS NEVER AN ANSWER, IN EITHER PASS. It is UNMEASURED: excluded from every
bucket, counted, named with its reason under its own banner, and it makes the sweep exit
nonzero, because "I could not measure it" is not "it is fine". Two places got this
wrong, and the second survived the first fix:

  1. A BUILD that times out is not a build failure. -9 compares as "did not build", so
     OLD timing out while NEW genuinely failed to build read as "neither built" and
     landed in no bucket at all.
  2. A timeout inside the CONFIRM pass is not agreement either - and this one is worse,
     because the first pass has ALREADY seen a difference by the time it runs. The
     confirm re-runs OLD to separate a regression from a nondeterministic program, and
     compared the re-run with `!=`; a timed-out re-run therefore "disagreed with itself"
     and the finding was filed as `nondeterministic (ignored)`. A real regression - up
     to and including the one-sided run-TIMEOUT case this file was fixed to catch - came
     out as `OUTPUT/EXIT REGRESSIONS: 0` and exit 0. See confirm_blocked().

TWO SOURCES OF FALSE "OUTPUT REGRESSION", both found by running this sweep with the
SAME binary on both sides (--self-proof), where the only honest answer is zero:

  1. The harness's own scratch directory. Each compiler must get its own copy of the
     source, so the two sides necessarily run in DIFFERENT directories - and an
     example that prints its working directory therefore "differs" every single run.
     The sweep's choice of directory must never reach the verdict, so both sides'
     paths are canonicalised to a token before comparison.
  2. Genuinely nondeterministic programs - a wall-clock timestamp, a random seed, a
     coroutine race. No canonicalisation can fix those, so a surviving difference is
     CONFIRMED by re-running THE SIDE THAT PRODUCED IT - OLD first, and then NEW
     whenever the difference survives OLD's confirm. If either side disagrees with
     itself the program is nondeterministic; if a re-run could not observe the side it
     needed to, the file is UNMEASURED rather than cleared.

     The NEW-side re-run is not optional and it is not a refinement: without it, a
     coincidental agreement between OLD's two runs is enough to file a racy program as
     a regression. examples/54_spawn_basics.wyn does exactly that - its whole output
     difference is `await_any: 20` vs `await_any: 60`, which coroutine won, and 8
     consecutive runs of ONE binary gave 3 distinct outputs.

Every number below is measured, from `--self-proof --corpus <repo> --trees examples`
(190 files discovered). A self-proof compares a compiler with ITSELF, so its only honest
answer is zero - which makes it the one run that exposes harness bugs rather than
compiler bugs. Three separate ones were found that way:

  * a literal canon() made an example printing its own scratch path uppercased differ
    from itself                                      -> 1 false regression
  * no NEW-side confirm on a run timeout             -> 4 false `rc N->TIMEOUT`
  * no NEW-side confirm on an output difference      -> 1 false regression
    (examples/54_spawn_basics.wyn, a coroutine race)

With all three fixed, measured on 2026-10-05: **MEASURED 190, UNMEASURED 0, BUILD
REGRESSIONS 0, OUTPUT/EXIT REGRESSIONS 0, nondeterministic (ignored) 9, exit 0.** Eight
of those nine are OLD-side self-disagreement (timestamps, system info, HTTP); the ninth
is 54_spawn_basics.wyn, caught by the NEW-side branch. The timeout findings are
budget-sensitive (`--run-timeout`, default 10); the other two are not.

Every path is a flag or an environment variable (see --help) and every precondition
is a hard failure - read scripts/sweep_common.py for why each one has to be. The
corpus default here is narrower than sweep_check.py's: only whole-file programs meant
to compile standalone, so the floor default is lower too.

  scripts/sweep_build.py --old /tmp/wyn-base/wyn --new ./wyn
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sweep_common as sc                                        # noqa: E402

# Example trees only, relative to the corpus root: whole-file programs meant to
# compile standalone. Trees that are absent are reported, not silently skipped.
#
# The default has to follow the corpus ACTUALLY RESOLVED. In a workspace the corpus is
# the workspace root, so the trees are spelled from there; when the corpus default falls
# back to the compiler repo, neither of those paths exists - so every default run in a
# bare clone used to discover 0 files and die on the floor, while examples/ (190 files,
# counted 2026-10-05) sat in the repo unused. A tool whose defaults only work on one
# machine's directory layout is the thing this harness was moved into the repo to stop
# being - which is why this is chosen from cfg.corpus_is_repo_fallback AFTER parsing and
# not from the import-time layout flag.
WORKSPACE_TREES = ("repos/wyn/examples", "repos/sample-apps")
REPO_TREES = ("examples",)


# A build that timed out has NO exit status; this is the sentinel that says so. It must
# never compare equal to a real `wyn build` failure - conflating the two is the whole
# bug class this file documents. Named so the places that must agree cannot drift.
BUILD_TIMEOUT_RC = -9

# Sentinel for a run that exceeded its budget. Same rule: not an exit status.
RUN_TIMEOUT = "TIMEOUT"


def canon(text, cfg):
    """Erase the harness's own scratch paths from a program's output.

    `cfg.tmp` differs per side only in the "old"/"new" component, and macOS resolves
    /tmp through /private/tmp, so both spellings have to go.

    CASE-INSENSITIVELY, because a literal str.replace left a live false positive that a
    self-proof run over examples/ surfaced: examples/45_system_args_typed.wyn prints
    `System::args()[0]` UPPERCASED, so each side printed its own scratch path in a
    spelling the replacement could not see and the file was reported as an
    OUTPUT/EXIT REGRESSION by the same binary compared with itself."""
    for base in (cfg.tmp, os.path.realpath(cfg.tmp)):
        for tag in ("old", "new"):
            for path, token in (
                    (os.path.join(base, "build", tag), "<SWEEPDIR>"),
                    (os.path.join(base, "build", "tmp_" + tag), "<SWEEPTMP>")):
                text = re.sub(re.escape(path), token, text, flags=re.IGNORECASE)
    return text


def one(cfg, exe, src, tag, build_timeout=180, run_timeout=10):
    """Build src with exe in an isolated copy; run it if it built. Returns dict.

    The two budgets are parameters, not literals, for one reason: with 180s and 10s
    baked in, the UNMEASURED and one-sided-run-TIMEOUT arms below take minutes per
    case to exercise and so were never exercised at all. This harness's own argument
    is that a gate nobody can afford to run is a gate nobody verifies."""
    d = os.path.join(cfg.tmp, "build", tag, os.path.basename(os.path.dirname(src)))
    os.makedirs(d, exist_ok=True)
    dst = os.path.join(d, os.path.basename(src))
    shutil.copy2(src, dst)
    env = dict(os.environ)
    env["TMPDIR"] = os.path.join(cfg.tmp, "build", "tmp_" + tag)
    os.makedirs(env["TMPDIR"], exist_ok=True)
    binp = dst[:-4] if dst.endswith(".wyn") else dst + ".out"
    for p in (binp, dst + ".c"):
        if os.path.exists(p):
            os.remove(p)
    try:
        b = subprocess.run([exe, "build", dst], capture_output=True,
                           timeout=build_timeout, env=env)
        built = (b.returncode == 0 and os.path.exists(binp))
    except subprocess.TimeoutExpired:
        return {"build_rc": BUILD_TIMEOUT_RC, "built": False, "run_rc": None,
                "out": "BUILD TIMEOUT"}
    if not built:
        err = (b.stdout + b.stderr).decode("utf8", "replace")
        return {"build_rc": b.returncode, "built": False, "run_rc": None,
                "out": err[-250:]}
    try:
        r = subprocess.run([binp], capture_output=True, timeout=run_timeout,
                           env=env, cwd=d, stdin=subprocess.DEVNULL)
        return {"build_rc": 0, "built": True, "run_rc": r.returncode,
                "out": r.stdout.decode("utf8", "replace")[:4000]}
    except subprocess.TimeoutExpired:
        return {"build_rc": 0, "built": True, "run_rc": RUN_TIMEOUT,
                "out": RUN_TIMEOUT}


def confirm_blocked(first, again):
    """Why the confirm re-run of OLD cannot settle a difference - or "" if it can.

    The confirm pass exists to tell a REGRESSION (OLD agrees with itself, NEW differs)
    from NONDETERMINISM (OLD disagrees with itself). It can only do either if it
    actually OBSERVED OLD. A timeout observes nothing - and a plain `!=` against a
    timed-out re-run reads that absence as "OLD disagrees with itself", i.e. files the
    pair under `nondeterministic (ignored)`, the one bucket printed as a non-finding.

    So the first pass could find a genuine difference - including exactly the one-sided
    run-TIMEOUT case this file was fixed to catch - and the confirm pass would drop it on
    the floor, leaving `OUTPUT/EXIT REGRESSIONS: 0` and a clean exit. That is the same
    masking shape as the build-timeout hole, one level deeper, and it is why a timeout
    here becomes UNMEASURED rather than an answer."""
    if again["build_rc"] == BUILD_TIMEOUT_RC:
        return "confirm re-run of OLD: BUILD timed out, so OLD was not observed"
    if not again["built"]:
        return ("confirm re-run of OLD: build FAILED this time though it succeeded "
                "last time (rc=%s) - the environment is unstable, not the compiler"
                % again["build_rc"])
    if (again["run_rc"] == RUN_TIMEOUT) != (first["run_rc"] == RUN_TIMEOUT):
        # Both runs timing out still agrees (and is how "servers time out under both"
        # stays a non-finding); exactly one timing out is an absence of evidence in
        # either direction, never a disagreement.
        return ("confirm re-run of OLD: run timed out on exactly one of the two OLD "
                "runs (first=%s, confirm=%s)" % (first["run_rc"], again["run_rc"]))
    return ""


def discover_trees(cfg, trees):
    """Walk only the named subtrees. Missing subtrees are named in the output."""
    files, missing = [], []
    for t in trees:
        base = t if os.path.isabs(t) else os.path.join(cfg.corpus, t)
        if not os.path.isdir(base):
            missing.append(base)
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d not in cfg.prune]
            for f in sorted(filenames):
                if f.endswith(".wyn"):
                    files.append(os.path.join(dirpath, f))
    files.sort()
    cfg.denominator = len(files)
    if missing:
        print("subtrees NOT FOUND (contributing 0 files):", flush=True)
        for m in missing:
            print("   %s" % m, flush=True)
    if cfg.denominator < cfg.min_files:
        sc.die("corpus floor not met: found %d buildable .wyn files under %s "
               "(trees: %s), floor is %d.\n"
               "       A sweep over an almost-empty corpus reports '0 regressions'\n"
               "       and proves nothing. Fix --corpus/--trees, or lower the floor\n"
               "       deliberately with --min-files."
               % (cfg.denominator, cfg.corpus, ",".join(trees), cfg.min_files))
    return files


def main():
    p = argparse.ArgumentParser(
        description="build+run differential across two compilers.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    sc.add_common_args(p, default_min_files=50)
    p.add_argument("--trees", default=os.environ.get("WYN_SWEEP_TREES", ""),
        help="comma-separated subtrees of --corpus to build; default %s, or %s when "
             "the --corpus default falls back to the compiler repo alone "
             "(env WYN_SWEEP_TREES; absolute paths allowed)"
             % (",".join(WORKSPACE_TREES), ",".join(REPO_TREES)))
    p.add_argument("--build-timeout", type=float,
                   default=float(os.environ.get("WYN_SWEEP_BUILD_TIMEOUT", "180")),
                   help="per-file build budget in seconds; exceeding it makes the "
                        "file UNMEASURED, never 'failed to build' "
                        "(env WYN_SWEEP_BUILD_TIMEOUT)")
    p.add_argument("--run-timeout", type=float,
                   default=float(os.environ.get("WYN_SWEEP_RUN_TIMEOUT", "10")),
                   help="per-binary run budget in seconds; a timeout on ONE side "
                        "only is a finding (env WYN_SWEEP_RUN_TIMEOUT)")
    args = p.parse_args()
    cfg = sc.resolve(args, "sweep_build")
    cfg.banner()

    default_trees = REPO_TREES if cfg.corpus_is_repo_fallback else WORKSPACE_TREES
    trees = [t for t in (args.trees or ",".join(default_trees)).split(",") if t]
    files = discover_trees(cfg, trees)
    print("building+running %d files with BOTH compilers\n" % cfg.denominator,
          flush=True)

    budgets = (args.build_timeout, args.run_timeout)
    print("budgets: build %gs, run %gs per file\n"
          % (args.build_timeout, args.run_timeout), flush=True)
    rows, build_reg, out_reg, nondet, unmeasured = [], [], [], [], []
    for i, src in enumerate(files, 1):
        rel = src.replace(cfg.corpus + "/", "")
        o = one(cfg, cfg.old, src, "old", *budgets)
        n = one(cfg, cfg.new, src, "new", *budgets)
        # UNMEASURED FIRST, because a BUILD TIMEOUT is not a build failure. It is
        # recorded as build_rc=-9/built=False, so OLD timing out while NEW genuinely
        # fails to build reads as "neither built" and lands in no bucket at all -
        # the same masking shape sweep_check.py carried, and the same fix.
        if BUILD_TIMEOUT_RC in (o["build_rc"], n["build_rc"]):
            why = ("build timed out (rc old=%s new=%s)"
                   % (o["build_rc"], n["build_rc"]))
            unmeasured.append((rel, why))
            print("?? UNMEASURED %s  <- %s" % (rel, why), flush=True)
        # Regression: old built, new did not.
        elif o["built"] and not n["built"]:
            build_reg.append(rel)
            print("** BUILD REGRESSION %s\n     %s"
                  % (rel, n["out"].strip()[:200]), flush=True)
        # Regression: both built and ran, but output or exit status differs. A RUN
        # timeout on ONE side only is such a difference, and the docstring has always
        # said so - but the condition used to require both sides be non-TIMEOUT,
        # which dropped exactly that case. Both sides timing out still compares equal
        # ("TIMEOUT" == "TIMEOUT"), which is the documented "servers and interactive
        # examples are the same under both" behaviour.
        elif o["built"] and n["built"]:
            if canon(o["out"], cfg) != canon(n["out"], cfg) \
                    or o["run_rc"] != n["run_rc"]:
                # CONFIRM before reporting: re-run the OLD side. A program that
                # disagrees with ITSELF is nondeterministic, not regressed - but only if
                # the re-run actually observed OLD (see confirm_blocked).
                o2 = one(cfg, cfg.old, src, "old", *budgets)
                blocked = confirm_blocked(o, o2)
                # The OLD-side confirm cannot test a timeout that happened on NEW, and a
                # one-sided run timeout is the one difference whose entire content IS a
                # timeout. Measured, not theorised: a SELF-PROOF run over examples/ - the
                # one run whose only honest answer is zero - reported this shape as an
                # OUTPUT/EXIT REGRESSION. So the timing-out side has to reproduce it.
                if (not blocked and n["run_rc"] == RUN_TIMEOUT
                        and o["run_rc"] != RUN_TIMEOUT):
                    n2 = one(cfg, cfg.new, src, "new", *budgets)
                    if n2["run_rc"] != RUN_TIMEOUT:
                        blocked = ("NEW's run timeout did NOT reproduce (confirm re-run "
                                   "of NEW: build_rc=%s run_rc=%s) - one slow run is "
                                   "contention, not a finding"
                                   % (n2["build_rc"], n2["run_rc"]))
                if blocked:
                    unmeasured.append((rel, blocked))
                    print("?? UNMEASURED %s  <- %s\n     (the first pass DID see a "
                          "difference here: rc %s->%s)"
                          % (rel, blocked, o["run_rc"], n["run_rc"]), flush=True)
                elif canon(o2["out"], cfg) != canon(o["out"], cfg) \
                        or o2["run_rc"] != o["run_rc"]:
                    nondet.append(rel)
                    print("   nondeterministic (OLD disagrees with itself), "
                          "not counted: %s" % rel, flush=True)
                else:
                    # OLD agreed with itself, so the difference survived the OLD-side
                    # confirm. That is NOT yet a regression: the side that PRODUCED the
                    # difference has not been asked to reproduce it. Re-run NEW.
                    #
                    # Measured, not theorised. The documented self-proof command over
                    # examples/ - the one run whose only honest answer is zero - reported
                    # examples/54_spawn_basics.wyn as an OUTPUT/EXIT REGRESSION against a
                    # compiler compared with ITSELF. The whole difference was
                    # `await_any: 20` vs `await_any: 60`, i.e. which coroutine won a race;
                    # 8 consecutive runs of one binary produced 3 distinct outputs. OLD's
                    # two runs had simply agreed by coincidence.
                    #
                    # The rule this restores is the one the NEW-timeout branch above
                    # already states: the timing-out side has to reproduce it. There was
                    # no reason for that rule to be special to timeouts. Cost is one
                    # extra NEW run per FINDING, and findings are rare by construction -
                    # a clean sweep pays nothing.
                    n2 = one(cfg, cfg.new, src, "new", *budgets)
                    if canon(n2["out"], cfg) != canon(n["out"], cfg) \
                            or n2["run_rc"] != n["run_rc"]:
                        nondet.append(rel)
                        print("   nondeterministic (NEW disagrees with itself), "
                              "not counted: %s" % rel, flush=True)
                    else:
                        out_reg.append(rel)
                        print("** OUTPUT DIFFERS %s  rc %s->%s"
                              % (rel, o["run_rc"], n["run_rc"]), flush=True)
        rows.append({"file": rel, "old": o, "new": n})
        if i % 50 == 0:
            print("  %d/%d" % (i, cfg.denominator), flush=True)

    ob = sum(1 for r in rows if r["old"]["built"])
    nb = sum(1 for r in rows if r["new"]["built"])
    newly = [r["file"] for r in rows if not r["old"]["built"] and r["new"]["built"]]
    print("\n" + "=" * 74)
    print(cfg.denominator_line())
    print("trees                     : %s" % ",".join(trees))
    print("OLD version               : %s" % cfg.old_version)
    print("NEW version               : %s" % cfg.new_version)
    print("built by OLD              : %d" % ob)
    print("built by NEW              : %d" % nb)
    print("NEWLY building on NEW     : %d" % len(newly))
    print("MEASURED (classified)     : %d" % (len(rows) - len(unmeasured)))
    print("UNMEASURED (in NO bucket below): %d" % len(unmeasured))
    print("BUILD REGRESSIONS         : %d" % len(build_reg))
    print("OUTPUT/EXIT REGRESSIONS   : %d" % len(out_reg))
    print("nondeterministic (ignored): %d" % len(nondet))
    # A nonzero UNMEASURED count gets its own banner. One line among twelve is how "I
    # could not measure 40 files" gets read as a clean run - the count above sits
    # directly above two reassuring zeros, which is the most reassuring place to hide.
    if unmeasured:
        print("\n*** %d FILE(S) WERE NOT MEASURED - THIS SWEEP DOES NOT CLEAR THEM ***"
              % len(unmeasured))
        print("    Not a regression and not a pass: no comparison happened. Raise")
        print("    --build-timeout/--run-timeout, or re-run on an idle box, then")
        print("    re-read the verdict.")
        for p_, why in unmeasured:
            print("  %s\n      %s" % (p_, why))
    for p_ in build_reg:
        print("   build: %s" % p_)
    for p_ in out_reg:
        print("   output: %s" % p_)
    with open(cfg.out, "w") as f:
        json.dump({"old": cfg.old, "new": cfg.new,
                   "old_version": cfg.old_version, "new_version": cfg.new_version,
                   "corpus": cfg.corpus, "trees": trees,
                   "denominator": cfg.denominator, "self_proof": cfg.identical,
                   "build_regressions": build_reg, "output_regressions": out_reg,
                   "nondeterministic": nondet,
                   "unmeasured": [{"file": f_, "why": w_} for f_, w_ in unmeasured],
                   "measured": len(rows) - len(unmeasured),
                   "newly_building": newly, "old_built": ob, "new_built": nb,
                   "total": len(rows), "rows": rows}, f, indent=1)
    print("\nwrote %s" % cfg.out)
    if build_reg or out_reg:
        print("VERDICT: REGRESSED")
        return 1
    if unmeasured:
        print("VERDICT: INCONCLUSIVE - 0 regressions among the %d files that were "
              "measured, but %d were NOT measured. This is not a pass."
              % (len(rows) - len(unmeasured), len(unmeasured)))
        return 1
    print("VERDICT: clean - %d files measured, 0 regressions" % len(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
