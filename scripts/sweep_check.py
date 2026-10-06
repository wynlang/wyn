#!/usr/bin/env python3
"""Pass 1: `wyn check` exit-status differential between two compilers.

The question is NOT "how many files pass" - many are module fragments that cannot
check standalone and fail under both compilers. The question is whether the NEW
compiler rejects anything the OLD one accepted. So every file is checked by BOTH
compilers and only the disagreements matter:

    old ok   -> new FAIL   REGRESSION. This is the whole point of the sweep.
    old FAIL -> new ok     an improvement; listed but not a problem.
    same      both         no information, and that is the expected bulk.

A TIMEOUT IS NOT A REJECTION, AND CONFLATING THE TWO HID THIS SWEEP'S OWN DEFECT
--------------------------------------------------------------------------------
A timed-out run is recorded as exit -9, and -9 is "!= 0", so until 2026-10-05 a
baseline timeout made the classifier read `old != 0 and new != 0` and file the pair
under "both reject" - the expected bulk. The effect: OLD times out under load while
NEW genuinely rejects the file, and the sweep prints `REGRESSED: 0` and exits 0. A
real regression became a clean bill of health with no timeout count anywhere in the
summary to hint at it. That is precisely the failure this harness exists to prevent,
reproduced inside it, and it is the EXPECTED artifact of this box: several agents
build concurrently, so a 60s `wyn check` budget is missed occasionally rather than
never.

So -9 is now UNMEASURED, a third outcome: excluded from every bucket, re-run SERIALLY
at 3x the budget to give contention a chance to clear (the same confirm-before-
reporting discipline sweep_build.py already uses for nondeterminism), counted in the
summary, and a survivor makes the sweep exit NONZERO. "I could not measure N files"
is a different statement from "N files are fine", and only one of them is true.

Each worker gets its own TMPDIR: the C-compiler error file is per-process *within*
the temp dir, so sharing one across workers makes failures cross-talk.

Every path is a flag or an environment variable (see --help) and every precondition
is a hard failure - read scripts/sweep_common.py for why each one has to be.

  scripts/sweep_check.py --old /tmp/wyn-base/wyn --new ./wyn
"""
import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sweep_common as sc                                        # noqa: E402

CFG = None          # set in main(); re-established in each worker by _init()

# The sentinel `wyn check` itself can never return: a timed-out run has NO exit
# status, and giving it one that compares equal to "rejected" is what hid a real
# regression. Named so the three places that must agree cannot drift.
TIMEOUT_RC = -9

# macOS starts worker processes with `spawn`, not `fork`, so the resolved config
# cannot be inherited through a module global - it has to be handed to an
# initializer. Getting this wrong leaves CFG as None in every worker.
WORKER = {}


def _init(old, new, tmp, timeout):
    WORKER["old"] = old
    WORKER["new"] = new
    WORKER["tmp"] = tmp
    WORKER["timeout"] = timeout


def _check_one(path, old, new, tmp, timeout, widx=0):
    env = dict(os.environ)
    td = os.path.join(tmp, "check", "w%d" % widx)
    os.makedirs(td, exist_ok=True)
    env["TMPDIR"] = td
    out = {}
    for tag, exe in (("old", old), ("new", new)):
        try:
            r = subprocess.run([exe, "check", path], capture_output=True,
                               timeout=timeout, env=env,
                               cwd=os.path.dirname(path) or ".")
            out[tag] = r.returncode
            out[tag + "_err"] = (r.stdout + r.stderr).decode("utf8", "replace")[-300:]
        except subprocess.TimeoutExpired:
            out[tag] = TIMEOUT_RC
            out[tag + "_err"] = "TIMEOUT after %gs" % timeout
    return out


def check(args):
    path, widx = args
    return path, _check_one(path, WORKER["old"], WORKER["new"], WORKER["tmp"],
                            WORKER["timeout"], widx)


def main():
    global CFG
    p = argparse.ArgumentParser(
        description="`wyn check` exit-status differential across two compilers.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    sc.add_common_args(p, fallback_min_files=500)
    p.add_argument("--timeout", type=float,
                   default=float(os.environ.get("WYN_SWEEP_TIMEOUT", "60")),
                   help="per-file `wyn check` budget in seconds; a file that exceeds "
                        "it is UNMEASURED, never 'rejected' "
                        "(env WYN_SWEEP_TIMEOUT)")
    args = p.parse_args()
    CFG = sc.resolve(args, "sweep_check")
    CFG.banner()
    print("per-file timeout: %gs (confirm pass: %gs)"
          % (args.timeout, args.timeout * 3), flush=True)

    files = CFG.discover(".wyn")
    print("sweeping %d .wyn files with both compilers" % CFG.denominator, flush=True)

    results = {}
    tasks = [(f, i % CFG.jobs) for i, f in enumerate(files)]
    done = 0
    with ProcessPoolExecutor(max_workers=CFG.jobs, initializer=_init,
                             initargs=(CFG.old, CFG.new, CFG.tmp,
                                       args.timeout)) as ex:
        for path, out in ex.map(check, tasks, chunksize=20):
            results[path] = out
            done += 1
            if done % 200 == 0:
                print("  %d/%d" % (done, CFG.denominator), flush=True)

    # CONFIRM PASS. A timeout under `--jobs 4` on a box where other agents are
    # building is usually contention, not a compiler that hangs, so re-run the
    # timed-out files one at a time at 3x the budget before calling them
    # unmeasurable. Serial and 3x is cheap because the set is tiny or empty; when
    # it is NOT tiny that fact is itself the finding.
    timed_out = sorted(p_ for p_, o in results.items()
                       if TIMEOUT_RC in (o["old"], o["new"]))
    if timed_out:
        print("\n%d file(s) hit the %gs budget - re-running SERIALLY at %gs before "
              "classifying them:" % (len(timed_out), args.timeout, args.timeout * 3),
              flush=True)
        for p_ in timed_out:
            results[p_] = _check_one(p_, CFG.old, CFG.new, CFG.tmp,
                                     args.timeout * 3, widx=0)
            still = TIMEOUT_RC in (results[p_]["old"], results[p_]["new"])
            print("   %-60s %s" % (p_.replace(CFG.corpus + "/", ""),
                                   "STILL TIMES OUT" if still else "cleared"),
                  flush=True)

    # UNMEASURED is a THIRD outcome, not a rejection. Excluding it from both buckets
    # is the whole point: a baseline timeout counted as "OLD rejected" turns a real
    # regression into "both reject" and reports a clean rc=0.
    unmeasured = sorted(p_ for p_, o in results.items()
                        if TIMEOUT_RC in (o["old"], o["new"]))
    measured = {p_: o for p_, o in results.items() if p_ not in set(unmeasured)}

    reg = [p_ for p_, o in measured.items() if o["old"] == 0 and o["new"] != 0]
    imp = [p_ for p_, o in measured.items() if o["old"] != 0 and o["new"] == 0]
    both_ok = sum(1 for o in measured.values() if o["old"] == 0 and o["new"] == 0)
    both_bad = sum(1 for o in measured.values() if o["old"] != 0 and o["new"] != 0)

    print("\n" + "=" * 70)
    print(CFG.denominator_line())
    print("files checked         : %d" % len(results))
    print("OLD version           : %s" % CFG.old_version)
    print("NEW version           : %s" % CFG.new_version)
    print("MEASURED (classified) : %d" % len(measured))
    print("UNMEASURED (timed out twice; in NO bucket below): %d" % len(unmeasured))
    print("both accept           : %d" % both_ok)
    print("both reject           : %d" % both_bad)
    print("IMPROVED (old no  -> new yes): %d" % len(imp))
    print("REGRESSED (old yes -> new no): %d" % len(reg))
    if unmeasured:
        print("\n*** UNMEASURED - these files were NOT compared, so this sweep does")
        print("    NOT clear them. Raise --timeout, or run with --jobs 1 on an idle")
        print("    box, then re-read the verdict.")
        for p_ in unmeasured[:60]:
            o = results[p_]
            print("  %s  (old rc=%s, new rc=%s)"
                  % (p_.replace(CFG.corpus + "/", ""), o["old"], o["new"]))
    if reg:
        print("\n*** REGRESSIONS ***")
        for p_ in reg[:60]:
            print("  %s" % p_.replace(CFG.corpus + "/", ""))
            print("      %s" % (results[p_]["new_err"].strip().splitlines()[:2],))
    with open(CFG.out, "w") as f:
        json.dump({"old": CFG.old, "new": CFG.new,
                   "old_version": CFG.old_version, "new_version": CFG.new_version,
                   "corpus": CFG.corpus, "denominator": CFG.denominator,
                   "self_proof": CFG.identical, "timeout": args.timeout,
                   "regressions": reg, "improvements": imp,
                   "unmeasured": unmeasured,
                   "both_ok": both_ok, "both_bad": both_bad,
                   "measured": len(measured),
                   "total": len(results),
                   "detail": {p_: dict(o) for p_, o in results.items()
                              if o["old"] != o["new"]}}, f, indent=1)
    print("\nwrote %s" % CFG.out)
    if reg:
        print("VERDICT: REGRESSED - %d file(s) the OLD compiler accepted" % len(reg))
        return 1
    if unmeasured:
        print("VERDICT: INCONCLUSIVE - 0 regressions among the %d files that were "
              "measured, but %d were NOT measured. This is not a pass."
              % (len(measured), len(unmeasured)))
        return 1
    print("VERDICT: clean - %d files measured, 0 regressions, 0 unmeasured"
          % len(measured))
    return 0


if __name__ == "__main__":
    sys.exit(main())
