# Third-party notices

The Wyn compiler and runtime are licensed under the MIT License (see [`LICENSE`](LICENSE)).
The distributed `wyn` binary and the release archives additionally contain, or are
statically linked against, third-party components listed below. Those components
remain under their own licenses; this file collects the required notices and points
at the license text that ships with them.

## Summary

| Component | Version | License | License text | How it is used |
|---|---|---|---|---|
| TinyCC (TCC) | 0.9.28rc, `mob` branch commit `4597a96` (built 2026-02-07) | LGPL-2.1 | [`vendor/tcc/COPYING`](vendor/tcc/COPYING) | **Redistributed, not linked.** The `tcc` executable, `libtcc1.a` and TCC's own headers ship under `vendor/tcc/`; Wyn runs the driver as a child process. No TCC code is in the `wyn` binary — see [below](#tinycc-is-invoked-not-linked) |
| minicoro | v0.2.0 (15 Nov 2023) | Public Domain (Unlicense) **or** MIT-0, at your option | [`vendor/minicoro/LICENSE`](vendor/minicoro/LICENSE) | header-only; `#include`d by `src/coroutine.c`, `src/spawn_fast.c`, `src/future.c`, so compiled into both the `wyn` binary and the runtime library |
| LuaCoco (portions) | derived, via minicoro | MIT | [`vendor/minicoro/LICENSE`](vendor/minicoro/LICENSE) | some of minicoro's assembly context-switch code is derived from LuaCoco by Mike Pall |
| Mbed TLS | 3.6.7 (3.6 LTS) | Apache-2.0 **or** GPL-2.0-or-later, at your option | [`vendor/mbedtls/LICENSE`](vendor/mbedtls/LICENSE) | `libmbedtls_wyn.a` is built from `vendor/mbedtls/library/*.c` and **statically linked** into every compiled Wyn program that uses `Http.*` over `https://`, and its objects are reachable from the redistributed `runtime/libwyn_rt.a` |
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

- `bin/tcc`, `lib/libtcc1.a`, `lib/tcc/libtcc1.a` — TCC driver and support library,
  redistributed unmodified so the default "no external C compiler needed" path works
- `include/libtcc.h`, `tcc_include/*` — TCC's public header and its bundled system
  headers
- `lib/libtcc.a` — present in the vendor tree but **not used**: no longer a link
  input, and nothing includes `libtcc.h`. It need not be vendored at all; removing it
  is a separate cleanup

### TinyCC is invoked, not linked

This entry previously said `libtcc.a` was "statically linked into `wyn`" and that
this "is what makes the shipped compiler binary a work that incorporates LGPL-2.1
code". **That was wrong**, and it is worth recording how, because an entire §6
analysis rested on it.

```
$ nm wyn | grep -i tcc
0000000100101874 T _wyn_tcc_available
0000000100101330 T _wyn_tcc_compile_to_exe
```

Two symbols, both Wyn's own. No libtcc symbols are present in the binary.

```
$ grep -rcoE '\btcc_[a-z_]+\(' src/     # libtcc API calls
0
$ grep -rn 'libtcc.h' src/              # the header
(no matches)
```

Nothing calls the libtcc API and nothing includes its header. `src/tcc_backend.c`
builds a command line and runs the driver as a separate process:

```c
snprintf(tcc_bin, sizeof(tcc_bin), "%s/vendor/tcc/bin/tcc", wyn_root);
...
int result = system(cmd);
```

`vendor/tcc/lib/libtcc.a` *was* named on the link line, which is where the claim came
from — but since no member was referenced, the linker discarded all of them. **An
archive on a link line is not the same as its code being in the output, and `nm` is
the check that tells them apart.** The dead input has now been removed from the
`Makefile`, so the artefact is unambiguous rather than merely documented.

The practical consequence of the error was larger than a wrong sentence: appending
this third-party prose to `LICENSE` made the file unclassifiable, so GitHub reported
the repository's licence as `NOASSERTION` and a visitor evaluating Wyn saw no licence
at all. `LICENSE` is now the unmodified MIT text and these notices live here.

`vendor/tcc/lib/libwyn_rt_tcc.a` is *not* a TCC artifact: it is Wyn's own runtime
(`src/*.c`) compiled *with* TCC and stored under that directory for convenience. It
contains no TCC code and is MIT-licensed like the rest of Wyn.

### LGPL-2.1 §6 does not apply — but §4 does

**§6 governs distributing a work that is *linked with* the Library, and `wyn` is not
one.** The evidence is above: no libtcc symbols in the binary, no libtcc API calls, no
`libtcc.h` include, and the driver invoked with `system()`. So there is no relinking
obligation on the `wyn` binary, and the "recommendation: dynamic linking" that used to
appear here was advice about a problem the project does not have. A user who wants a
different TinyCC replaces `vendor/tcc/bin/tcc`, which is where the backend looks for
it — substitutability is already a property of the design.

What *does* apply is the obligation on redistributing the Library and the driver
themselves. Wyn ships `bin/tcc` and `lib/libtcc1.a` as **object code** without TCC's
corresponding source, which is the situation LGPL-2.1 §4 addresses: distribution in
object-code form must be accompanied by the complete corresponding machine-readable
source, or by a written offer (valid three years) to supply it. §1/§2 cover the
verbatim source and headers that do ship.

So correcting the claim does **not** close the compliance item; it changes which one
is open, and the practical remedy is the same one the old text arrived at by a
different route: **publish the exact TCC source corresponding to the bundled binaries
(0.9.28rc, `mob@4597a96`) from the same place as the release archives.** That is a
single tarball per release rather than an ongoing per-platform relink recipe, and it
needs no change to how Wyn builds or links.

The unconditional notice duties are met: this file states that TCC is used and is
LGPL-2.1, and [`vendor/tcc/COPYING`](vendor/tcc/COPYING) ships in every archive, with
`release.yml` asserting its presence in the unpacked result.

The licence conclusions above are the project owner's to confirm; what this file can
and does establish is the factual question — what the artefacts contain, and by which
commands that was determined.

<details>
<summary>The superseded §6 option analysis, kept for the record</summary>

The text below was written on the assumption that `libtcc.a` was statically linked.
It is retained because the reasoning about option costs is still useful if the project
ever *does* link libtcc, but it describes no present obligation.



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

</details>

## Mbed TLS

- Upstream: <https://github.com/Mbed-TLS/mbedtls> (3.6 LTS branch)
- Version: 3.6.7, from `MBEDTLS_VERSION_STRING` in
  `vendor/mbedtls/include/mbedtls/build_info.h`
- License: dual Apache-2.0 **or** GPL-2.0-or-later, at the user's option
- License text: [`vendor/mbedtls/LICENSE`](vendor/mbedtls/LICENSE)
- Provenance and upgrade recipe: [`vendor/mbedtls/README.wyn.md`](vendor/mbedtls/README.wyn.md)

Until 2026-09 this entry was absent, and that was defensible for exactly one reason:
`src/wyn_tls.c` was compiled by **nothing**, so no mbedTLS code reached a user. Making
HTTPS native changed that - `src/wyn_tls.c` and `src/wyn_https.c` are now in the
Makefile's `RT_SRCS`, and `vendor/mbedtls/lib/libmbedtls_wyn.a` is on the link line of
every program that calls `Http.*` over `https://`. Static linking of a third-party
library is what makes the notice mandatory, so the entry lands with the change that
creates the obligation.

Apache-2.0 §4 requires retaining the copyright, patent, trademark and attribution
notices from the source, and including a copy of the License with any distribution -
both satisfied by shipping `vendor/mbedtls/LICENSE` and this entry. It imposes no
relinking obligation, so there is nothing here like the open LGPL-2.1 §6 question
above; taking the GPL-2.0-or-later arm of the dual license would, which is a reason to
stay on the Apache-2.0 arm.

Files redistributed under `vendor/mbedtls/`:

- `lib/libmbedtls_wyn.a` - the built static library, statically linked into compiled
  Wyn programs (built by the `$(MBEDTLS_LIB)` rule in the `Makefile`)
- `include/mbedtls/*.h`, `include/psa/*.h` - the public headers, needed to compile
  `src/wyn_tls.c` (which `wyn build-runtime` does)
- `library/*.c`, `library/*.h` - the corresponding source, so the shipped archive's
  provenance is verifiable and it can be rebuilt
- `LICENSE`, `README.wyn.md`

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
`LICENSE`, `THIRD-PARTY-NOTICES.md`, `src/`, `runtime/`, `vendor/minicoro/`,
`vendor/tcc/` and `vendor/mbedtls/` (its `LICENSE`, built library and headers) into the
distribution. Because those directories are copied recursively,
`vendor/tcc/COPYING`, `vendor/minicoro/LICENSE` and `vendor/mbedtls/LICENSE` ship with
every release.

The notices are **enforced, not merely intended**: the "Verify artifact layout"
steps (Unix and Windows) assert that `THIRD-PARTY-NOTICES.md`, `vendor/tcc/COPYING`
and `vendor/mbedtls/LICENSE` are present in the packaged archive, and the release fails
if any is missing. A future refactor of the packaging steps therefore cannot
silently drop the notices and put the distribution back out of compliance.

`site/public/install.ps1` also installs `THIRD-PARTY-NOTICES.md` and `vendor/`
alongside the binary. (The Unix installer extracts the whole tarball, so it gets
them implicitly.)

## Reporting a problem with this file

If a component is missing, misattributed, or its license text is out of date, please
open an issue at <https://github.com/wynlang/wyn/issues>.
