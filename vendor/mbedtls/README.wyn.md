# Vendored mbedTLS — provenance and upgrade recipe

**Version: 3.6.7** (the 3.6 **LTS** line). Apache-2.0 — see `LICENSE`.

## Why this is here

Wyn's HTTPS client used to shell out:

```c
popen("printf 'GET %s HTTP/1.1\r\nHost: %s\r\n\r\n' | openssl s_client -quiet -connect %s:443")
```

That splices caller data — the URL and the POST body — into a shell command, i.e. an RCE
in `Http.get`, `https_post`, `http_put` and `http_delete`. It also imposed a hidden
runtime dependency (no `openssl` on `PATH` → every HTTPS call returned `""`), capped
responses at 128 KB, never decoded chunked bodies, and threw the status line away.

mbedTLS gives us in-process TLS with X.509 verification, from one code path on all five
targets (macOS, Linux, Windows, iOS, Android). Owner decision, 2026-09-22 — the reasoning
and the alternatives that lost (BearSSL, per-platform system TLS, libcurl) are recorded in
the release plan, "OWNER DECISIONS 2026-09-22".

## What is vendored, and what is not

| Path | |
|---|---|
| `include/mbedtls/`, `include/psa/` | upstream, unmodified |
| `library/*.c`, `library/*.h` | upstream, unmodified — **all 109** `.c` files |
| `LICENSE` | upstream Apache-2.0 |
| *(not vendored)* | `tests/`, `programs/`, `docs/`, `scripts/`, `3rdparty/`, CMake/build glue |

**Nothing upstream is patched.** Keep it that way: a local edit to a vendored tree is
invisible at review time and silently lost on the next upgrade. If mbedTLS needs to behave
differently, do it through the config or through our own seam (`src/wyn_tls.c`), not by
editing `library/`.

`3rdparty/` (Everest Curve25519, p256-m) is deliberately absent — both are referenced only
under config flags that the default config leaves off.

## Config

The upstream default `include/mbedtls/mbedtls_config.h`, **untouched**. Correctness first;
`libmbedtls_wyn.a` is a static archive, so a program links only the members it actually
uses and the unused algorithms cost compiled binaries nothing. Trimming the config to a
client-only profile is a separate, later concern.

## Build

```sh
make mbedtls     # -> vendor/mbedtls/lib/libmbedtls_wyn.a
```

`make all` builds it too, so CI compiles the vendored tree on Linux, macOS-arm64,
macOS-x64 and Windows/mingw. Objects land in `vendor/mbedtls/obj/`; both that and `lib/`
are git-ignored (the sources are committed, the products are not — unlike
`vendor/tcc/lib/libtcc.a`, which is a committed upstream binary).

## Upgrading

Stay on the **3.6 LTS** line. Do **not** move to 4.x without a decision: 4.x is
PSA-crypto-only, a different API, and a rewrite of our seam rather than a version bump.

```sh
V=3.6.8   # a tag on the 3.6 branch
cd /tmp && curl -sSLO https://codeload.github.com/Mbed-TLS/mbedtls/tar.gz/refs/tags/v$V
tar xzf v$V && cd <wyn>/vendor/mbedtls
rm -rf include library && cp -R /tmp/mbedtls-$V/include . && mkdir library
cp /tmp/mbedtls-$V/library/*.c /tmp/mbedtls-$V/library/*.h library/
cp /tmp/mbedtls-$V/LICENSE . && rm -f include/CMakeLists.txt
cd <wyn> && make clean && make && make test
```

The tag archive is what we use rather than a release tarball because the 3.6 branch
**commits** its generated PSA driver wrappers (`library/psa_crypto_driver_wrappers.h`,
`psa_crypto_driver_wrappers_no_static.c`) — so no Python codegen, no `framework/`
submodule, nothing to run at build time. Verify those two files exist after any upgrade;
if they don't, you are not on 3.6.
