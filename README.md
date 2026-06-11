# cpputest-turbo-lite

[![ci](https://github.com/Bud-ro/cpputest-turbo/actions/workflows/ci.yml/badge.svg?branch=lite)](https://github.com/Bud-ro/cpputest-turbo/actions/workflows/ci.yml)

The **lite branch** of [cpputest-turbo](https://github.com/Bud-ro/cpputest-turbo):
a from-scratch rewrite of [CppUTest](https://cpputest.github.io/) with a C11
core and a thin C++ header shim, **stripped to the surface most embedded
suites actually use**. Test sources that stay on that surface recompile
unchanged with byte-identical output — and compile dramatically faster,
because `TestHarness.h` no longer drags in any C++ standard header.

For the full upstream-compatible feature set (leak detection, the C API,
JUnit/TeamCity output, conformance suite, ...), use the
[`master` branch](https://github.com/Bud-ro/cpputest-turbo/tree/master).

*Entirely AI-generated — see [Provenance](#provenance).*

```
include/CppUTest/      C++ shim headers (upstream-identical names & macros)
include/CppUTestExt/   CppUMock shim + MockSupportPlugin
include/cpputest_core/ internal C core interface (not a supported API)
src/core/              C11 core: runner, assertions, output, filters, -jN
src/mock/              C11 mock core
src/shim/              the single C++ TU in the library (SimpleString)
```

## What lite keeps, what it drops

Kept (byte-identical to upstream, verified by differential fuzzing):

- every assertion macro, `TEST_GROUP`/`TEST_GROUP_BASE`/`IMPORT_TEST_GROUP`
- the full `mock()` matching engine: all `withParameter` overloads,
  `withParameterOfType` + comparators/copiers (structs!), `onObject`
  (C-vtable verification), output parameters, return values, scopes,
  `strictOrder`, `ignoreOtherCalls`, mid-test `checkExpectations`
- `MockSupportPlugin` and the plugin base (pre/post/parseArguments)
- group + name filters (`-g/-sg/-xg/-xsg -n/-sn/-xn/-xsn -t/... "TEST(G, n)"`),
  `-v`, `-ri`
- `-jN` parallel workers (extension): groups across N forked processes,
  deterministic replayed output

Removed (compile error if you use it — nothing silently changes meaning):

- memory leak detection (the `new` macro; `new`/`delete` now run at libc
  speed) and the allocator/OOM-simulation surface
- the C API (`TestHarness_c.h`, `MockSupport_c.h`) — test code is C++; the
  *code under test* is still typically C
- JUnit/TeamCity output, `-k`, `-o*`
- `-p` fork-per-test, shuffle/`-s`, repeat/`-r`, reverse/`-b`, listings
  (`-lg/-ln/-ll`), `-f`, `-e/-ci`, `-c`, `-vv`
- mock data store (`setData`/`getData`), mock tracing mode
- `OrderedTest`, `UT_PTR_SET`/`SetPointerPlugin`, `TestTestingFixture`

## Building

```sh
make                # gcc + GNU make: build/libCppUTest.a, build/libCppUTestExt.a
./scripts/check.sh  # full gate: format, build, unit suites, static analyzer,
                    # sanitizers, differential fuzz vs vendored upstream
```

No cmake, no autotools. `make install PREFIX=/usr/local` installs the
libraries, headers and a pkg-config file.

**Requirements:** a C11 compiler and GNU make. The C++ shim needs C++11 and
is built `-fno-exceptions -fno-rtti`; the resulting archives have **zero
libstdc++ dependencies** — a pure-C consumer links them with plain `gcc`.
Linux and macOS are the supported platforms; CI exercises gcc and clang on
both.

## Drop-in migration

Point your include path at `include/` and link `build/libCppUTest.a`
(plus `libCppUTestExt.a` for mocks). When linking with `-l`, list
`-lCppUTestExt` **before** `-lCppUTest` (the mock library depends on the
core); linking the `.a` files by path in that order avoids the trap.
Vendored as a submodule, the whole consumer recipe is:

```make
third_party/cpputest-turbo/build/libCppUTest.a:
	$(MAKE) -C third_party/cpputest-turbo
CPPFLAGS  += -Ithird_party/cpputest-turbo/include
TEST_LIBS  = third_party/cpputest-turbo/build/libCppUTestExt.a \
             third_party/cpputest-turbo/build/libCppUTest.a
```

CMake consumers: `make install` produces a `cpputest.pc`, so
`find_package(PkgConfig)` + `pkg_check_modules(CPPUTEST REQUIRED cpputest)`
works; the pkg-config module name is `cpputest` (matching upstream).

As with upstream, install `MockSupportPlugin` in your runner for mock
expectations to be verified at test end. Test sources are unchanged:

```cpp
#include "CppUTest/TestHarness.h"

TEST_GROUP(Stack)
{
    TEST_SETUP() { /* ... */ }
    TEST_TEARDOWN() { /* ... */ }
};

TEST(Stack, pushPop)
{
    LONGS_EQUAL(42, 42);
    STRCMP_EQUAL("a", "a");
    mock().expectOneCall("driverInit");  // CppUTestExt/MockSupport.h
}
```

## Runner flags

`-h` for the full list: `-v -ri -j<N> -g/-sg/-xg/-xsg -n/-sn/-xn/-xsn
-t/-st/-xt/-xst "TEST(Group, name)"`. Removed flags are rejected with the
usage message, never silently ignored.

`-jN` (extension): test groups are distributed over N forked workers with
full memory isolation. Output is replayed in worker order, so parallel runs
are deterministic.

## Performance

Numbers and the reproduction harness live in
[`bench/RESULTS.md`](bench/RESULTS.md) (`sh bench/run_bench.sh`). Headlines
on the measured machine: a `TestHarness.h` test TU preprocesses to ~1,000
lines instead of ~31,000 (no std headers), the 21-TU benchmark suite
compiles ~2.4× faster than against upstream, and the runtime stress load
(5M assertions + 500k `new`/`delete`) runs ~8× faster sequentially —
passing assertions are an inlined compare plus counter bump, and untracked
`new`/`delete` is plain libc.

## How parity is verified

`third_party/cpputest/` vendors upstream as a read-only oracle. The gate
compiles the same public-API torture suites and seeded random-op fuzz
drivers (mock sequences, API compositions, CLI flag combinations) against
both libraries and byte-compares outputs and exit codes after normalizing
timings/addresses. Sanitizers (ASan/UBSan) and `gcc -fanalyzer` run over
everything; golden-output suites pin the failure-message formats.

## Known divergences from upstream

- Everything in the "removed" list above is a compile- or usage-error, by
  design.
- `-jN` is an extension; upstream rejects the flag.
- Not implemented (as on master): the `MockFailureReporter` injection
  surface; mock failures always report through the normal test-failure path
  (which `MockSupportPlugin` relies on, and which behaves identically in
  practice).
- Failing assertions longjmp out of the test (upstream's no-exceptions mode);
  C++ temporaries alive at that moment are not destructed. Bounded to failure
  paths; the sanitizer sweep (`scripts/check-sanitizers.sh`) accounts for it.

## Provenance

This project is **entirely AI-generated** (Claude, via Claude Code) — core,
headers, tests, fuzzers, build system, and docs, including this README. It
was built in a plan-driven loop: do the next unchecked item in
[`PLAN.md`](PLAN.md), verify, log, commit — the full history of iterations
(bugs, dead ends, lessons) is in PLAN.md's iteration log, under the standing
rules in [`CLAUDE.md`](CLAUDE.md). Correctness never rested on the model's
self-assessment: the gates are golden-output tests, differential fuzzing
against the real upstream build, sanitizers, and static analysis.

The lite branch came from one more human prompt, roughly: *make a `lite`
branch that utterly minimizes compile times, line count and complexity for
downstream consumers whose code under test is C — keep comparators,
onObject, filters, `-jN` and MockSupportPlugin; drop the rest; prove
near-identical behavior by differential fuzzing.*

## License

BSD-3-Clause — see `LICENSE`. The interface (header names, macro shapes,
message formats) and a small number of macro bodies follow CppUTest
(BSD-3-Clause, see `third_party/cpputest/COPYING`); the implementation is
new code.

cpputest-turbo is an independent reimplementation and is not affiliated
with or endorsed by the CppUTest project. "CppUTest" refers to the upstream
project at cpputest.github.io, whose public interface this project
reproduces for source compatibility.
