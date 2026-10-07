# Security policy

## Reporting a vulnerability

**Use GitHub's private vulnerability reporting:**
[**Report a vulnerability**](https://github.com/wynlang/wyn/security/advisories/new).

That channel is private between you and the maintainer until an advisory is published,
and it does not require an email address from either side. Please use it rather than a
public issue for anything that could be exploited against someone running Wyn code.

If the form is unavailable to you for any reason, open a public issue saying only that
you have a security report and asking for a private channel — no details — and one will
be arranged.

### What to include

A report is most useful with:

- the Wyn version (`wyn --version`) and the platform,
- a minimal `.wyn` program or HTTP request that demonstrates it,
- what an attacker gains, and what they need in order to try.

A reproduction matters more than a severity rating. Several defects in this project
have been filed with a confident severity and a mechanism that turned out to be wrong,
so the repro is the part that carries the argument.

### What to expect

This is a **single-maintainer, pre-1.0 project**, and the honest commitment is modest:
an acknowledgement when the report is read, and a fix or a stated decision not to fix.
There is no 24-hour SLA and promising one would be false. Fixes land on `dev` and ship
in the next release; if something warrants an out-of-band release, that is a judgement
call made per report.

Credit is given in the advisory and the changelog unless you ask otherwise.

## Supported versions

Only the **latest release** is supported, and `dev` is where fixes land first. There
are no maintained release branches and no backports. See
[CHANGELOG.md](CHANGELOG.md) for what is current.

## Where the attack surface actually is

This section exists so a reporter knows where to look, and so the project is not
pretending the surface is smaller than it is. Wyn is a compiler *and* a runtime that
programs link against, so there are two distinct trust boundaries.

**The runtime, reached by a remote attacker.** This is the sharp end, because a Wyn
HTTP server processes untrusted bytes:

- `Http.serve` / `Http.accept` / `Http.read_request` — request parsing, the request
  record handed to user code, response routing.
- The vendored [Mbed TLS](THIRD-PARTY-NOTICES.md) 3.6 LTS for `https://`. Report
  issues in Mbed TLS itself upstream; report *our wiring* of it here.
- The hand-written JSON parser, the regex engine, `Csv`, `Toml`, `Base64`,
  `Encoding` — all of these are fed attacker-controlled input by real programs, and
  none of them is a hardened third-party implementation.
- `Db` / SQLite query construction, and `Template` rendering.

**The compiler, reached by hostile source.** Compiling untrusted Wyn source is
**not** a supported security boundary — `wyn` runs a C compiler on generated code and
honours `#[ffi]` and `system()`-shaped stdlib calls, so a hostile `.wyn` file should
be treated like a hostile `Makefile`. Crashes in the compiler on malformed input are
real bugs worth reporting, but they are reliability bugs, not sandbox escapes, and the
playground is the only place where this boundary is load-bearing.

**Memory safety.** The runtime is C with reference-counted strings and manually
managed containers. Use-after-free, double-free and buffer overruns in the runtime are
in scope and are the most valuable class to report. `make test` runs ASan and TSan
jobs in CI; a report that comes with an ASan trace is close to a fix.

## Known, already public

The project tracks security-relevant defects in public issues, because until now there
was no private channel to use instead. That is a deliberate record, not an oversight to
be quietly cleaned up. Reporting a *new* issue privately is the preferred path from
here on.

## Scope

In scope: the compiler, the runtime, the standard library, the installers, and the
release artefacts in this repository.

Out of scope: third-party upstreams (report those upstream and tell us so we can
update the vendored copy), the content of `wynlang.com`, and anything requiring a
pre-existing local foothold on the developer's machine.
