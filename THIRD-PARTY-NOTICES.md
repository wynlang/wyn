# Third-party notices

The Wyn compiler and runtime are licensed under the MIT License (see [`LICENSE`](LICENSE)).
The distributed `wyn` binary and the release archives additionally contain, or are
statically linked against, third-party components listed below. Those components
remain under their own licenses; this file collects the required notices and points
at the license text that ships with them.

## Summary

| Component | Version | License | License text | How it is used |
|---|---|---|---|---|
| TinyCC (TCC) / `libtcc` | 0.9.28rc, `mob` branch commit `4597a96` (built 2026-02-07) | LGPL-2.1 | [`vendor/tcc/COPYING`](vendor/tcc/COPYING) | `libtcc.a` is **statically linked** into the `wyn` binary; the `tcc` executable, `libtcc1.a` and TCC's own headers are also redistributed |
| minicoro | v0.2.0 (15 Nov 2023) | Public Domain (Unlicense) **or** MIT-0, at your option | [`vendor/minicoro/LICENSE`](vendor/minicoro/LICENSE) | header-only; `#include`d by `src/coroutine.c`, `src/spawn_fast.c`, `src/future.c`, so compiled into both the `wyn` binary and the runtime library |
| LuaCoco (portions) | derived, via minicoro | MIT | [`vendor/minicoro/LICENSE`](vendor/minicoro/LICENSE) | some of minicoro's assembly context-switch code is derived from LuaCoco by Mike Pall |
| clang `stdatomic.h` (derived) | via TCC | Apache-2.0 WITH LLVM-exception | notice in the file header of [`vendor/tcc/tcc_include/stdatomic.h`](vendor/tcc/tcc_include/stdatomic.h) | redistributed as one of TCC's bundled headers |
| mingw-w64 `varargs.h` | via TCC | Public Domain (no copyright asserted) | notice in the file header of [`vendor/tcc/tcc_include/varargs.h`](vendor/tcc/tcc_include/varargs.h) | redistributed as one of TCC's bundled headers |

`vendor/opencl/CL/cl.h` is **not** third-party code: it is a minimal, hand-written
OpenCL 1.2 type/constant subset authored for Wyn so that `src/gpu_opencl.c` can
compile without the Khronos SDK (the real `libOpenCL` is loaded with `dlopen` at run
time and never linked). It carries the same MIT license as the rest of Wyn.

## TinyCC (TCC)

- Upstream: <https://repo.or.cz/tinycc.git> (project page: <https://bellard.org/tcc/>)
- Version: `tcc version 0.9.28rc 2026-02-07 mob@4597a96`, as reported by
  `vendor/tcc/bin/tcc -v`
- License: GNU Lesser General Public License, version 2.1
- License text: [`vendor/tcc/COPYING`](vendor/tcc/COPYING)
- Copyright: `Tiny C Compiler 0.9.28rc - Copyright (C) 2001-2006 Fabrice Bellard`
  (the notice carried by the binary itself), plus the TinyCC contributors since

Files redistributed under `vendor/tcc/`:

- `lib/libtcc.a` — statically linked into `wyn` (see the `wyn$(EXE_EXT)` rule in the
  `Makefile`); this is what makes the shipped compiler binary a work that
  incorporates LGPL-2.1 code
- `bin/tcc`, `lib/libtcc1.a`, `lib/tcc/libtcc1.a` — TCC driver and support library,
  redistributed unmodified so the default "no external C compiler needed" path works
- `include/libtcc.h`, `tcc_include/*` — TCC's public header and its bundled system
  headers

`vendor/tcc/lib/libwyn_rt_tcc.a` is *not* a TCC artifact: it is Wyn's own runtime
(`src/*.c`) compiled *with* TCC and stored under that directory for convenience. It
contains no TCC code and is MIT-licensed like the rest of Wyn.

### LGPL-2.1 §6 — relinking obligation

LGPL-2.1 §6 governs distributing a work that is linked with the Library. It requires
three things unconditionally — a prominent notice that the Library is used and that
the Library and its use are covered by the LGPL, a copy of the License, and (if the
work displays copyright notices at run time) the Library's copyright notice among
them — plus **one** of options 6(a) through 6(e). This file and
[`vendor/tcc/COPYING`](vendor/tcc/COPYING) satisfy the notice and license-copy parts.
Because `libtcc.a` is *statically* linked into the shipped `wyn` binary, the
remaining choice is the open item. The relevant options:

- **(a) Keep static linking and ship the relink materials** — accompany the binary
  with the complete corresponding source for `libtcc` (including any local changes),
  *and* the "work that uses the Library" — Wyn's own code as object code and/or
  source — so a user can build a modified `libtcc` and relink it into a working
  `wyn`. In practice: the TCC source tarball or object files used to produce
  `libtcc.a`, Wyn's sources (already shipped under `src/` in the release archives),
  and the documented link command. 6(c) and 6(d) are lighter variants of the same
  duty: a three-year written offer for those materials, or offering them from the
  same download location as the binary.
- **(b) Link `libtcc` dynamically** — use a shared-library mechanism so the
  executable does not copy library functions into itself and will work with a
  user-installed, interface-compatible replacement. Note that 6(b) is written around
  a library "already present on the user's computer system"; shipping Wyn's own
  `libtcc.so` / `libtcc.dylib` / `libtcc.dll` next to the binary is the common
  interpretation and meets the substitutability requirement, but it does mean the
  bundled copy must remain replaceable (a stable soname / rpath the user can override,
  no static fallback silently winning).

**Recommendation: (b), dynamic linking**, with 6(d) as the interim measure.

Reasoning: dynamic linking is the option with the smallest ongoing compliance
surface — nothing has to be regenerated or kept in sync per release, and there is no
obligation to publish Wyn object files or maintain a relink recipe. Option (a) is
workable — the release archives already contain `src/`, and the link line is a single
`cc` invocation — but it converts a release-checklist item into a compliance
dependency: the shipped TCC source/object code and the relink instructions must stay
accurate for five platforms, or the distribution quietly falls out of compliance
again. The cost of (b) is packaging and loader work: build a shared `libtcc` per
target, wire up rpath / `@loader_path` / `LoadLibrary`, and give up the
single-file-binary property for the TCC path.

Choosing between these is a project decision and is deliberately **not** made here —
the `Makefile`'s linking strategy is unchanged by this file. Until it is decided, the
distribution should be treated as relying on 6(d): the exact TCC source corresponding
to the bundled `libtcc.a` (0.9.28rc, `mob@4597a96`) should be published from the same
place as the release archives, together with the command used to link `wyn`, so that
the relink path is genuinely available to users.

## minicoro

- Upstream: <https://github.com/edubart/minicoro>
- Version: v0.2.0, dated 15/Nov/2023 (from the header comment in
  `vendor/minicoro/minicoro.h`)
- License: your choice of Public Domain (Unlicense) or MIT No Attribution (MIT-0)
- License text: [`vendor/minicoro/LICENSE`](vendor/minicoro/LICENSE) (reproduced from
  the license block at the end of `vendor/minicoro/minicoro.h`)
- Copyright: Copyright (c) 2021-2023 Eduardo Bart

Neither license requires attribution, so this entry is informational. minicoro's
assembly context-switch code is partly derived from LuaCoco by Mike Pall
(<https://coco.luajit.org/>), MIT-licensed; that notice is reproduced in
`vendor/minicoro/LICENSE` and does require preservation.

## Packaging note

The release archives are assembled by `.github/workflows/release.yml`, which copies
`LICENSE`, `THIRD-PARTY-NOTICES.md`, `src/`, `runtime/`, `vendor/minicoro/` and
`vendor/tcc/` into the distribution. Because `vendor/` is copied recursively,
`vendor/tcc/COPYING` and `vendor/minicoro/LICENSE` ship with every release.

The notices are **enforced, not merely intended**: the "Verify artifact layout"
steps (Unix and Windows) assert that both `THIRD-PARTY-NOTICES.md` and
`vendor/tcc/COPYING` are present in the packaged archive, and the release fails if
either is missing. A future refactor of the packaging steps therefore cannot
silently drop the notices and put the distribution back out of compliance.

`site/public/install.ps1` also installs `THIRD-PARTY-NOTICES.md` and `vendor/`
alongside the binary. (The Unix installer extracts the whole tarball, so it gets
them implicitly.)

## Reporting a problem with this file

If a component is missing, misattributed, or its license text is out of date, please
open an issue at <https://github.com/wynlang/wyn/issues>.
