<!--
Base your branch on `dev`, not `main` — see CONTRIBUTING.md. `main` only moves at
release time.

Keep a PR to ONE CONCERN, roughly <=5 files. The reason is isolation, not review
latency: a large PR cannot tell you WHICH change broke a platform, and `make test` is
fail-fast at the suite level, so a red batch tells you nothing about the rest of it. A
fix plus its regression test is one concern; a fix plus a refactor is two — land the
refactor first, behaviour-neutral.

Delete any section that does not apply. Prose is fine; this is a checklist, not a form.
-->

## What this changes

<!-- One or two sentences. What was wrong, and what is now true instead. -->

## Why, with evidence

<!--
The mechanism, not the symptom. If you are fixing a defect, the reproduction goes
here — a program, a command, a request. A grep count or a source comment is not
evidence that a defect exists; the repro is, and it usually costs under a minute.

If you are correcting a claim (a doc, a benchmark, a comment), say what the old claim
was and which command disproves it.
-->

## How it is gated

<!--
Which test, and where. A new rejection rule also needs the differential corpus sweep
and the book snippet gate.
-->

- [ ] **Mutation-verified.** I stubbed the fix out and confirmed the new test FAILS,
      then restored it — per arm, if the test has several.

<!--
Two traps this project has actually fallen into, so they are named here:

  * PREFER DELETING A RULE OVER REWORDING IT. Prefixing a diagnostic leaves the
    asserted text as a substring, so the gate stays green and the mutation taught
    nothing.
  * A POSITIVE CONTROL MUST EXERCISE THE THING IT CLAIMS TO. A control that deletes a
    symbol the test never reaches passes for the wrong reason, and reads as "the symbol
    is dead" when it only means "the probe is wrong". If a mutation applies, builds, and
    changes no test result, suspect the probe before concluding the gate is sound.
-->

## Verification run

<!--
What you actually ran, and what it said. Builds and tests go in the container
(`./wyn-container.sh`); the host's EDR costs ~14 s per novel binary, so native timings
are not comparable and native suite runs are slow rather than wrong.

  - [ ] `make` from scratch (`rm -f wyn && make` — make returns 0 WITHOUT relinking on
        an mtime-second tie, so verify the artefact, not the exit code)
  - [ ] `make test` — paste the tally
  - [ ] ASan, if this touches the runtime or anything memory-related
-->

## Anything you chose not to do

<!--
Known-but-unfixed things you found on the way, and why they are not here. Filing them
is better than fixing them in this PR. "Not a regression" is not a reason to ship a
defect you found, but "it needs a design decision that is not mine to make" is a good
reason to file it instead.
-->

Closes #
