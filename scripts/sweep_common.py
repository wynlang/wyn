#!/usr/bin/env python3
"""Shared argument resolution and self-checks for the differential corpus sweeps.

The sweeps (sweep_check.py, sweep_build.py, sweep_apps.py) are the release gate for
every new rejection rule: they run an OLD compiler and a NEW compiler over the same
corpus and report only the disagreements.

WHY THIS FILE EXISTS, AND WHY EVERY CHECK BELOW IS A HARD FAILURE
----------------------------------------------------------------
The sweeps' failure mode is SILENT and it looks exactly like success. A sweep whose
OLD compiler is missing, unreadable, or is secretly the SAME binary as NEW prints a
clean "0 regressions" report - the most reassuring output it has - while having
measured nothing. These scripts previously hardcoded an absolute path to a v1.21.0
binary in an untracked scratch directory. A cleanup deleted that binary, so the gate
could not run at all; and because every path in them was absolute and
machine-specific, they could not live in the repo, be reviewed in a PR, or run in CI.

So: every precondition below EXITS NONZERO with a message. None of them warn.

  1. OLD and NEW both exist, are regular files, and are executable.
  2. `<exe> --version` runs, exits 0, and prints something; the version string of
     BOTH compilers is echoed into the report. (A previously-deleted artifact
     directory's binary reported the wrong version - printing it is the only way a
     reader can tell which two compilers were actually compared.)
  3. OLD and NEW are not the same binary, unless --self-proof is passed. Comparing a
     compiler against itself is a legitimate smoke test of the harness, but it must
     be requested, never stumbled into.
  4. The discovered corpus is at or above --min-files. A walk that silently finds 3
     files reports 0 regressions just as happily as one that finds 1,433.
  5. The denominator actually measured is printed in the summary. Never inherit a
     denominator from a document: measured on 2026-10-05, this workspace's `.wyn` count
     is 1,433 with `worktrees` pruned and 7,573 without, because each `worktrees/*`
     checkout carries a copy of the corpus. That is why `worktrees` is pruned by
     default and why the prune list is printed next to the count.

Paths are derived from this file's own location (scripts/ -> repo -> workspace) and
every one of them can be overridden by a flag or an environment variable, so nothing
here is specific to one machine or one checkout.
"""

import hashlib
import os
import re
import subprocess
import sys

# --------------------------------------------------------------------------- paths

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)            # the wyn compiler repo/worktree
WORKSPACE_ROOT = os.path.dirname(os.path.dirname(REPO_ROOT))

# The corpus is deliberately the WORKSPACE corpus (every Wyn package and app beside
# the compiler), not just the compiler repo: a rejection rule has to be measured
# against real user code, and repos/wyn holds only 1,022 of the 1,433 files. If this
# checkout is not inside a workspace layout, fall back to the repo itself.
IN_WORKSPACE = os.path.isdir(os.path.join(WORKSPACE_ROOT, "repos"))
DEFAULT_CORPUS = WORKSPACE_ROOT if IN_WORKSPACE else REPO_ROOT

# True for a bare clone (CI, a reviewer's checkout): the corpus DEFAULT is the compiler
# repo alone. The FLOOR has to move with it. The workspace corpus is 1,433 .wyn files
# against a floor of 1,000; the repo alone is 1,022 against the same 1,000, i.e. 22
# files of headroom, so a legitimate test-consolidation PR that deletes 23 fixtures
# hard-fails the tool with "corpus floor not met". A floor that reds on a correct
# change is a floor somebody lowers in a hurry, and after that it protects nothing -
# so each caller declares a fallback floor sized to the corpus it will actually walk.
#
# THIS FLAG IS A PROPERTY OF THE CHECKOUT LAYOUT, NOT OF WHAT WAS SWEPT. On its own it
# answers "could the corpus default have fallen back", and using it directly handed the
# reduced floor to a bare clone that was pointed at a full workspace with an explicit
# --corpus - a loosened floor nobody asked for. resolve() therefore narrows it to
# cfg.corpus_is_repo_fallback: this flag AND the corpus actually resolving to the
# default. Read that, not this, when deciding anything floor- or tree-shaped.
CORPUS_FALLBACK = not IN_WORKSPACE

# `worktrees` is pruned because each sibling worktree is a near-copy of the compiler
# repo: measured on this box on 2026-10-05, the same walk finds 1,433 .wyn files with
# `worktrees` pruned and 7,573 with the six worktrees that were live - i.e. an unpruned
# walk would report a denominator more than five times the corpus.
DEFAULT_PRUNE = (".git", "node_modules", "worktrees")

# Where a baseline compiler is expected to live. There is no honest way to guess a
# previous release's binary, so this is a convention, and its absence is a loud
# failure carrying the recipe for populating it (see die_no_old below).
DEFAULT_OLD = os.path.join(REPO_ROOT, ".sweep", "old", "wyn")
DEFAULT_NEW = os.path.join(REPO_ROOT, "wyn")

ANSI = re.compile(r"\x1b\[[0-9;]*m")


def die(msg):
    """Every self-check failure comes through here: stderr, prefixed, exit 2."""
    sys.stderr.write("\nsweep: FATAL: %s\n" % msg)
    sys.exit(2)


def _exe_check(label, path):
    if not path:
        die("%s compiler path is empty (pass --%s or set WYN_SWEEP_%s)"
            % (label, label.lower(), label.upper()))
    if not os.path.exists(path):
        die("%s compiler does not exist: %s\n"
            "       A sweep with a missing compiler would otherwise report a clean\n"
            "       '0 regressions'. Build or extract a baseline first, e.g.\n"
            "         git worktree add /tmp/wyn-base <base-sha> && (cd /tmp/wyn-base && make)\n"
            "         %s --old /tmp/wyn-base/wyn ..."
            % (label, path, os.path.basename(sys.argv[0])))
    if not os.path.isfile(path):
        die("%s compiler is not a regular file: %s" % (label, path))
    if not os.access(path, os.X_OK):
        die("%s compiler is not executable: %s" % (label, path))


def _version_of(label, path):
    """Run `<exe> --version`; require success and non-empty output; return it."""
    try:
        r = subprocess.run([path, "--version"], capture_output=True, timeout=60)
    except OSError as e:
        die("%s compiler could not be executed: %s (%s)" % (label, path, e))
    except subprocess.TimeoutExpired:
        die("%s compiler hung on --version: %s" % (label, path))
    if r.returncode != 0:
        die("%s compiler `--version` exited %d: %s\n       stderr: %s"
            % (label, r.returncode, path,
               r.stderr.decode("utf8", "replace").strip()[:200]))
    text = ANSI.sub("", (r.stdout + r.stderr).decode("utf8", "replace")).strip()
    if not text:
        die("%s compiler `--version` printed nothing: %s" % (label, path))
    return text.splitlines()[0].strip()


def _md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class SweepConfig(object):
    """Resolved, self-checked inputs for one sweep run."""

    def __init__(self, old, new, old_version, new_version, identical,
                 corpus, prune, tmp, out, jobs, min_files,
                 corpus_is_repo_fallback=False, min_files_source="default"):
        self.old = old
        self.new = new
        self.old_version = old_version
        self.new_version = new_version
        self.identical = identical
        self.corpus = corpus
        self.prune = prune
        self.tmp = tmp
        self.out = out
        self.jobs = jobs
        self.min_files = min_files
        # True only when the corpus default was ACTUALLY taken and it fell back to the
        # compiler repo. Callers key their reduced floors and their default subtrees off
        # this, never off CORPUS_FALLBACK.
        self.corpus_is_repo_fallback = corpus_is_repo_fallback
        # WHERE the floor came from, printed beside it. The banner used to annotate the
        # floor line with "repo-only fallback floor" whenever the LAYOUT could have
        # triggered a fallback - including runs whose floor came straight from
        # --min-files, which mislabels the one number a reader checks.
        self.min_files_source = min_files_source
        self.denominator = None        # set by discover()

    # ---------------------------------------------------------------- discovery
    def discover(self, suffix=".wyn"):
        """Walk the corpus, enforce the floor, and record the denominator."""
        if not os.path.isdir(self.corpus):
            die("corpus root is not a directory: %s" % self.corpus)
        files = []
        for dirpath, dirnames, filenames in os.walk(self.corpus):
            dirnames[:] = [d for d in dirnames if d not in self.prune]
            for f in filenames:
                if f.endswith(suffix):
                    files.append(os.path.join(dirpath, f))
        files.sort()
        self.denominator = len(files)
        if self.denominator < self.min_files:
            die("corpus floor not met: found %d %s files under %s (pruned: %s), "
                "floor is %d.\n"
                "       A sweep over an almost-empty corpus reports '0 regressions'\n"
                "       and proves nothing. Fix the corpus root, or lower the floor\n"
                "       deliberately with --min-files."
                % (self.denominator, suffix, self.corpus,
                   ",".join(sorted(self.prune)), self.min_files))
        return files

    # ----------------------------------------------------------------- reporting
    def banner(self):
        print("=" * 74)
        print("OLD  : %s" % self.old)
        print("       version: %s" % self.old_version)
        print("NEW  : %s" % self.new)
        print("       version: %s" % self.new_version)
        if self.identical:
            print("NOTE : SELF-PROOF MODE - OLD and NEW are the SAME binary (md5 equal).")
            print("       Zero differences here proves the HARNESS, not the compiler.")
        print("corpus root   : %s" % self.corpus)
        print("pruned dirs   : %s" % ",".join(sorted(self.prune)))
        print("floor         : %d   (%s)" % (self.min_files, self.min_files_source))
        print("=" * 74, flush=True)

    def denominator_line(self, label="corpus denominator", root=None):
        """The one line every summary must carry. Never inherit this number.

        `root` is explicit because a sweep may walk something narrower than
        --corpus (sweep_apps walks --apps); printing the corpus root next to a
        count that did not come from it is how a wrong denominator survives."""
        return ("%-22s: %s  (measured now; root=%s pruned=%s)"
                % (label, self.denominator, root or self.corpus,
                   ",".join(sorted(self.prune))))


def add_common_args(parser, default_min_files=1000, fallback_min_files=None):
    """Register the flags every sweep shares. Env vars supply the defaults.

    `fallback_min_files`, when given, replaces the floor WHEN THE CORPUS DEFAULT WAS
    ACTUALLY TAKEN and that default fell back to the compiler repo alone. An explicit
    per-caller number beats scaling the floor by a ratio: sweep_build's and sweep_apps'
    floors count buildable trees and wyn.toml projects, not the whole-corpus walk, so
    the same multiplier would be wrong for them. Omit it and the floor is unchanged -
    no silent loosening.

    The choice CANNOT be made here. This function runs before --corpus is parsed, so
    the only fact available is CORPUS_FALLBACK - the checkout layout - and keying on it
    gave the reduced floor to `--corpus <a full workspace>` run from a bare clone. Both
    numbers are therefore recorded on the parser and resolve() picks one once it knows
    what will actually be walked.
    """
    env = os.environ.get
    parser.set_defaults(_default_min_files=default_min_files,
                        _fallback_min_files=fallback_min_files)
    parser.add_argument("--old", default=env("WYN_SWEEP_OLD", DEFAULT_OLD),
                        help="baseline compiler (env WYN_SWEEP_OLD; default %(default)s)")
    parser.add_argument("--new", default=env("WYN_SWEEP_NEW", DEFAULT_NEW),
                        help="candidate compiler (env WYN_SWEEP_NEW; default %(default)s)")
    parser.add_argument("--corpus", default=env("WYN_SWEEP_CORPUS", DEFAULT_CORPUS),
                        help="root to walk for inputs (env WYN_SWEEP_CORPUS; "
                             "default %(default)s)")
    parser.add_argument("--prune", default=env("WYN_SWEEP_PRUNE",
                                               ",".join(DEFAULT_PRUNE)),
                        help="comma-separated directory names to skip "
                             "(env WYN_SWEEP_PRUNE; default %(default)s)")
    parser.add_argument("--min-files", type=int,
                        default=(int(env("WYN_SWEEP_MIN_FILES"))
                                 if env("WYN_SWEEP_MIN_FILES") else None),
                        help="HARD floor on the discovered corpus size "
                             "(env WYN_SWEEP_MIN_FILES; default %d, or %s when the "
                             "--corpus default falls back to the compiler repo alone)"
                             % (default_min_files,
                                fallback_min_files
                                if fallback_min_files is not None else "unchanged"))
    parser.add_argument("--jobs", type=int,
                        default=int(env("WYN_SWEEP_JOBS", "4")),
                        help="parallel workers (env WYN_SWEEP_JOBS; default %(default)s)")
    parser.add_argument("--tmp", default=env("WYN_SWEEP_TMP", ""),
                        help="scratch dir (env WYN_SWEEP_TMP; "
                             "default $TMPDIR/wyn-sweep)")
    parser.add_argument("--out", default=env("WYN_SWEEP_OUT", ""),
                        help="JSON report path (env WYN_SWEEP_OUT; "
                             "default <tmp>/<script>.json)")
    parser.add_argument("--self-proof", action="store_true",
                        default=env("WYN_SWEEP_SELF_PROOF", "") not in ("", "0"),
                        help="permit OLD and NEW to be the same binary "
                             "(harness smoke test; env WYN_SWEEP_SELF_PROOF)")
    return parser


def resolve(args, tag):
    """Run every self-check and return a SweepConfig. Exits nonzero on any failure."""
    old = os.path.abspath(os.path.expanduser(args.old))
    new = os.path.abspath(os.path.expanduser(args.new))

    _exe_check("OLD", old)
    _exe_check("NEW", new)
    old_version = _version_of("OLD", old)
    new_version = _version_of("NEW", new)

    identical = (old == new) or (_md5(old) == _md5(new))
    if identical and not args.self_proof:
        die("OLD and NEW are the SAME binary (md5 equal):\n"
            "         OLD %s\n         NEW %s\n"
            "       Such a sweep cannot find anything and would report a clean\n"
            "       '0 regressions'. Point --old at a different compiler, or pass\n"
            "       --self-proof if you meant to smoke-test the harness."
            % (old, new))

    tmp = os.path.abspath(args.tmp) if args.tmp else os.path.join(
        os.environ.get("TMPDIR", "/tmp"), "wyn-sweep")
    os.makedirs(tmp, exist_ok=True)
    out = os.path.abspath(args.out) if args.out else os.path.join(tmp, tag + ".json")

    corpus = os.path.abspath(os.path.expanduser(args.corpus))
    # The reduced floor is for the corpus the fallback actually produces, so it needs
    # BOTH facts: the layout has no workspace around it AND the resolved corpus is still
    # the default. Keying on the layout alone (CORPUS_FALLBACK) silently lowered the
    # floor for an explicit `--corpus <workspace>`, i.e. for the larger corpus.
    corpus_is_repo_fallback = (CORPUS_FALLBACK
                               and corpus == os.path.abspath(DEFAULT_CORPUS))
    min_files = args.min_files
    source = "from --min-files/WYN_SWEEP_MIN_FILES"
    if min_files is None:          # neither --min-files nor WYN_SWEEP_MIN_FILES given
        fb = getattr(args, "_fallback_min_files", None)
        if corpus_is_repo_fallback and fb is not None:
            min_files = fb
            source = ("repo-only fallback floor: the --corpus default fell back to the "
                      "compiler repo")
        else:
            min_files = args._default_min_files
            source = "this script's default floor"

    return SweepConfig(
        old=old, new=new, old_version=old_version, new_version=new_version,
        identical=identical,
        corpus=corpus,
        prune=set(p for p in args.prune.split(",") if p),
        tmp=tmp, out=out, jobs=max(1, args.jobs), min_files=min_files,
        corpus_is_repo_fallback=corpus_is_repo_fallback, min_files_source=source)
