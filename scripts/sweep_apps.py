#!/usr/bin/env python3
"""Pass 2: run every sample-app project's OWN `wyn test` under both compilers and
compare the verdicts, not just the exit codes.

`wyn check` on a sample-app file standalone is NOT the gate - cross-directory imports
only resolve through the project runner, which is why an earlier sweep reported "45
broken apps" that were fine. So this walks wyn.toml projects and runs the project
runner in each.

Both the tally (N pass / M fail) and the exit code are compared. A build that compiles
and runs but silently returns different answers is the failure mode worth catching,
and only the tally shows that.

Every path is a flag or an environment variable (see --help) and every precondition is
a hard failure - read scripts/sweep_common.py for why each one has to be. Here the
floor is on PROJECTS (wyn.toml dirs), not on .wyn files.

  scripts/sweep_apps.py --old /tmp/wyn-base/wyn --new ./wyn
"""
import argparse
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sweep_common as sc                                        # noqa: E402

DEFAULT_APPS_REL = "repos/sample-apps"

# The runner prints "Results: 15 tests passed, 0 failed  (1 test file: 1 passed, 0
# failed, 0.0s)". An earlier version of this regex used \\s+ between the number and
# "passed", so it matched only the parenthetical FILE tally and silently reported 58
# tests across all 39 projects instead of ~965 - the comparison was still valid but
# every number in it was the wrong unit. Anchor on the test-level line.
# TWO formats, because the runner's summary changed between the versions and a regex
# that knows only one silently reports zero for the other:
#   v1.21.0 : "3 tests passed"                 (assertion tally, no ", N failed")
#             "Results: 1 passed, 0 failed"    (FILE tally)
#   v1.22   : "Results: 3 tests passed, 0 failed  (1 test file: 1 passed, 0 failed)"
# ASSERTS counts assertion-level tests in either dialect; FILES counts test files.
ASSERTS_NEW = re.compile(r"(\d+)\s+tests?\s+passed,\s*(\d+)\s+failed", re.I)
ASSERTS_OLD = re.compile(r"^\s*(?:\x1b\[[0-9;]*m)*(\d+)\s+tests?\s+passed", re.I | re.M)
ASSERTS_OLD_FAIL = re.compile(r"(\d+)\s+tests?\s+failed", re.I)
FILES = re.compile(r"Results:.*?(\d+)\s+passed,\s*(\d+)\s+failed", re.I)


def tally(text):
    """(asserts_pass, asserts_fail, files_pass, files_fail) in either dialect."""
    ap = af = 0
    hits = list(ASSERTS_NEW.finditer(text))
    if hits:                                    # newer dialect
        for m in hits:
            ap += int(m.group(1)); af += int(m.group(2))
    else:                                       # older dialect
        for m in ASSERTS_OLD.finditer(text):
            ap += int(m.group(1))
        for m in ASSERTS_OLD_FAIL.finditer(text):
            af += int(m.group(1))
    fp = ff = 0
    for m in FILES.finditer(text):
        # The newer Results line also carries the file tally in parentheses; taking
        # the FIRST two numbers of that line would double-count, so match the file
        # pair only when the line is the older shape (no "tests passed" before the
        # comma).
        seg = m.group(0)
        if re.search(r"test file", seg, re.I) or not ASSERTS_NEW.search(seg):
            fp += int(m.group(1)); ff += int(m.group(2))
    return ap, af, fp, ff


def run(cfg, exe, proj, tag):
    env = dict(os.environ)
    td = os.path.join(cfg.tmp, "apps", tag)
    os.makedirs(td, exist_ok=True)
    env["TMPDIR"] = td
    try:
        r = subprocess.run([exe, "test"], cwd=proj, capture_output=True,
                           timeout=600, env=env)
        return r.returncode, (r.stdout + r.stderr).decode("utf8", "replace")
    except subprocess.TimeoutExpired:
        return -9, "TIMEOUT"


def discover_projects(cfg, apps):
    projects = []
    if not os.path.isdir(apps):
        sc.die("apps root is not a directory: %s" % apps)
    for dirpath, dirnames, filenames in os.walk(apps):
        dirnames[:] = [d for d in dirnames if d not in cfg.prune]
        if "wyn.toml" in filenames:
            projects.append(dirpath)
    projects.sort()
    cfg.denominator = len(projects)
    if cfg.denominator < cfg.min_files:
        sc.die("project floor not met: found %d wyn.toml projects under %s, "
               "floor is %d.\n"
               "       A sweep over zero projects reports '0 differing' and proves\n"
               "       nothing. Fix --apps, or lower the floor deliberately with\n"
               "       --min-files."
               % (cfg.denominator, apps, cfg.min_files))
    return projects


def main():
    p = argparse.ArgumentParser(
        description="`wyn test` per-project verdict differential across two compilers.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    sc.add_common_args(p, default_min_files=20)
    p.add_argument("--apps", default=os.environ.get("WYN_SWEEP_APPS", ""),
                   help="root holding wyn.toml projects "
                        "(env WYN_SWEEP_APPS; default <corpus>/%s)" % DEFAULT_APPS_REL)
    args = p.parse_args()
    cfg = sc.resolve(args, "sweep_apps")
    apps = os.path.abspath(args.apps) if args.apps else os.path.join(
        cfg.corpus, DEFAULT_APPS_REL)
    cfg.banner()
    print("apps root     : %s" % apps, flush=True)

    projects = discover_projects(cfg, apps)
    print("%d sample-app projects\n" % cfg.denominator, flush=True)

    rows = []
    for i, proj in enumerate(projects, 1):
        name = os.path.relpath(proj, apps)
        rc_o, out_o = run(cfg, cfg.old, proj, "old")
        rc_n, out_n = run(cfg, cfg.new, proj, "new")
        po, fo, fpo, ffo = tally(out_o)
        pn, fn, fpn, ffn = tally(out_n)
        # Compare BOTH units. Asserts is the real test count; files guards against a
        # project whose files all ran but whose assertions silently stopped counting.
        same = (rc_o == rc_n) and (po, fo) == (pn, fn) and (fpo, ffo) == (fpn, ffn)
        rows.append({"name": name, "rc_old": rc_o, "rc_new": rc_n,
                     "old": [po, fo], "new": [pn, fn],
                     "old_files": [fpo, ffo], "new_files": [fpn, ffn], "same": same,
                     "new_tail": out_n[-700:] if not same else "",
                     "old_full": out_o, "new_full": out_n})
        flag = "  " if same else "**"
        print("%s %2d/%d %-38s OLD: rc=%s %st/%sf %sfile  ->  "
              "NEW: rc=%s %st/%sf %sfile"
              % (flag, i, cfg.denominator, name, rc_o, po, fo, fpo,
                 rc_n, pn, fn, fpn), flush=True)

    diff = [r for r in rows if not r["same"]]
    tot_o = (sum(r["old"][0] for r in rows), sum(r["old"][1] for r in rows))
    tot_n = (sum(r["new"][0] for r in rows), sum(r["new"][1] for r in rows))
    fo_t = (sum(r["old_files"][0] for r in rows), sum(r["old_files"][1] for r in rows))
    fn_t = (sum(r["new_files"][0] for r in rows), sum(r["new_files"][1] for r in rows))
    print("\n" + "=" * 74)
    print(cfg.denominator_line("projects denominator", root=apps))
    # A project whose assertion tally is 0 under BOTH compilers carries no
    # information: `wyn test` exits 0 with zero test blocks, so "same" there is
    # vacuous rather than reassuring. Count them so the number is visible.
    vacuous = [r["name"] for r in rows if r["old"][:2] == [0, 0] == r["new"][:2]]
    print("vacuous (0 asserts both sides, no information): %d of %d"
          % (len(vacuous), len(rows)))
    print("OLD %-22s: %s tests pass, %s fail | %s files pass, %s fail"
          % (cfg.old_version[:22], tot_o[0], tot_o[1], fo_t[0], fo_t[1]))
    print("NEW %-22s: %s tests pass, %s fail | %s files pass, %s fail"
          % (cfg.new_version[:22], tot_n[0], tot_n[1], fn_t[0], fn_t[1]))
    print("projects DIFFERING : %d" % len(diff))
    for r in diff:
        print("\n  *** %s: rc %s->%s, %sp/%sf -> %sp/%sf"
              % (r["name"], r["rc_old"], r["rc_new"],
                 r["old"][0], r["old"][1], r["new"][0], r["new"][1]))
        for line in r["new_tail"].strip().splitlines()[-12:]:
            print("        %s" % line)
    with open(cfg.out, "w") as f:
        json.dump({"old": cfg.old, "new": cfg.new,
                   "old_version": cfg.old_version, "new_version": cfg.new_version,
                   "apps": apps, "denominator": cfg.denominator,
                   "vacuous": vacuous,
                   "self_proof": cfg.identical, "rows": rows}, f, indent=1)
    print("\nwrote %s" % cfg.out)
    return 1 if diff else 0


if __name__ == "__main__":
    sys.exit(main())
