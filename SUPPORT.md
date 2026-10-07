# Getting help with Wyn

Wyn is a single-maintainer, pre-1.0 project. There is no support contract and no
guaranteed response time — but questions are welcome, and a question that turns out to
be a defect is one of the more useful things you can send.

## Start here

- **[The book](https://wynlang.com/book)** — the long-form guide, worked examples first.
- **[Docs and the standard library reference](https://wynlang.com/docs)**.
- **[The playground](https://wynlang.com)** — run something without installing anything.
- **`wyn help`**, and `wyn help <command>` for a specific one.
- **`wyn doctor`** — reports what the installation can actually do: which C backend it
  found, whether the bundled TCC backend and runtime are present, whether HTTPS is
  wired up. Run this first when something behaves unexpectedly after install.

## Asking a question

Open a **[GitHub issue](https://github.com/wynlang/wyn/issues)**. There is no mailing
list, chat or forum; issues are deliberately the single channel, so that answers are
searchable by the next person.

What makes a question answerable:

- `wyn --version` and your platform.
- A **small program** that shows the problem, rather than a description of one. Five
  lines that misbehave beat two paragraphs about a hundred that do.
- What you expected and what happened — including the exact error text, if any.

If the compiler accepted your program and the *generated C* failed, please include that
C error too. `wyn build <file> --debug` keeps the generated `<file>.wyn.c` next to the
source so you can look at it. That class of failure is invisible to `wyn check`, and
knowing which side rejected the program is most of the diagnosis.

## Reporting a bug

Same place, with one addition: say whether it reproduces on a **fresh build of `dev`**
if you can. A surprising share of reported defects in this project's history turned out
to be already fixed, or to be a stale build artefact rather than a regression — a
uniform failure across many programs is usually evidence of the latter.

If the bug has security impact, do **not** open a public issue. See
[SECURITY.md](SECURITY.md).

## Requesting a feature

Open an issue describing the problem you hit, not just the syntax you want. The design
record for this language is mostly a record of features that were cut because the
problem had a smaller answer, so the problem statement is the part that persuades.

## Contributing a fix

See [CONTRIBUTING.md](CONTRIBUTING.md). Short version: branch from `dev`, one concern
per PR, bring a regression test, and mutation-verify it.

## What is not supported

- Release branches other than the latest, and backports to them.
- Compiling untrusted Wyn source as a security boundary — see
  [SECURITY.md](SECURITY.md).
- Wyn packages and applications that live outside this repository and are versioned
  separately.
