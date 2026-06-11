# cpputest-turbo

[![ci](https://github.com/Bud-ro/cpputest-turbo/actions/workflows/ci.yml/badge.svg)](https://github.com/Bud-ro/cpputest-turbo/actions/workflows/ci.yml)

A from-scratch rewrite of [CppUTest](https://cpputest.github.io/) with a **C11
core** and a thin C++ header shim. Source-compatible with upstream — existing
CppUTest suites recompile unchanged — with byte-identical output, faster
runtime, process-level test isolation, and first-class support for projects
that only have a C compiler.

*Entirely AI-generated — see [Provenance](#provenance).*

```
include/CppUTest/      C++ shim headers (upstream-identical names & macros)
include/CppUTestExt/   CppUMock shim + OrderedTest
include/cpputest_core/ internal C core interface (not a supported API)
src/core/              C11 core: runner, assertions, output, leak detection
src/mock/              C11 mock core + MockSupport_c
src/shim/              the only C++ TUs in the library (operator new, SimpleString)
```

## Building

```sh
make            # gcc + GNU make: build/libCppUTest.a, build/libCppUTestExt.a
make CPPUTEST_C_ONLY=1   # C compiler only — no C++ anywhere
./scripts/check.sh       # full gate: format, build, unit + conformance,
                         # static analyzer, sanitizers, differential fuzz
```

No cmake, no autotools. `make install PREFIX=/usr/local` installs the
libraries, headers and a pkg-config file.

**Requirements:** a C11 compiler and GNU make — the stock `make` on Linux
and macOS both qualify. The C++ shim needs C++11. Linux and macOS are the
supported platforms; CI exercises gcc and clang on both, and
`scripts/check-macos.sh` cross-compiles for both macOS architectures with
`zig cc`. The pre-PR gate is `./scripts/check.sh` (`CHECK_FAST=1` skips the
heavy stages).

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
works; the pkg-config module name is `cpputest` (matching upstream), so
existing lookups pick up the replacement transparently.

Two caveats: `make CPPUTEST_C_ONLY=1` builds without the C++ shim, so C++
test sources will not link against it (it serves pure-C consumers of
`TestHarness_c.h`); and as with upstream, install `MockSupportPlugin` in
your runner for mock expectations to be verified at test end. Test sources
are unchanged:

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

All assertion macros, the mock() framework, memory leak detection (the `new`
macro and the opt-in malloc macros), `TEST_GROUP_BASE`, `IMPORT_TEST_GROUP`,
plugins, `UT_PTR_SET`, `TEST_ORDERED` and the `TestHarness_c.h` C interface
behave like upstream — failure messages, summary lines, exit codes and JUnit/
TeamCity files are byte-for-byte compatible (including several upstream quirks
we reproduce deliberately). Upstream's own test suites for the assertion
macros, plugins and cheat sheets pass **unmodified** (`make conformance`,
manifest in `conformance/`, per-file skip reasons in `conformance/SKIPPED.md`).

## Pure C usage

C projects can use the framework with no C++ toolchain at all:

```c
#include "CppUTest/TestHarness_c.h"   /* CHECK_EQUAL_C_INT, TEST_C, ... */
#include "CppUTestExt/MockSupport_c.h" /* mock_c()->expectOneCall(...) */
```

Pair C test files with the upstream-compatible C++ wrapper pattern, or use
the C macros end to end. These two headers are the **supported C API**; the
`cpputest_core/` headers they sit on are internal and carry no stability
guarantee.

## Runner flags

All upstream flags work (`-h` for the full list): `-v -vv -c -g/-sg/-xg/-xsg
-n/-sn/-xn/-xsn -t/-st/-xt/-xst -b -s [seed] -r[N] -ri -f -e/-ci -lg -ln -ll
-ojunit -oteamcity -k <package> "TEST(Group, name)"`.

Process isolation:

- `-p` (upstream-compatible): every test runs in a forked child. A crashing
  test reports `Failed in separate process - killed by signal N` and the run
  continues.
- `-jN` (extension): test groups are distributed over N forked workers with
  full memory isolation. Output is replayed in worker order, so parallel runs
  are deterministic. Composes with `-p` for per-test crash containment inside
  each worker.

## Lean mode (fast compiles)

`TestHarness.h` must pre-include `<new>`/`<memory>`/`<string>` before
defining the leak-detection `new` macro — those standard headers dominate the
compile cost of every test file (same as upstream). If you don't use the
leak detector or SimpleString's `std::string` interop, compile your test
files with:

```sh
-DCPPUTEST_MEM_LEAK_DETECTION_DISABLED -DCPPUTEST_USE_STD_CPP_LIB=0
```

which cuts the preprocessed size of a test TU by more than an order of
magnitude and speeds compilation accordingly (current measurements:
`bench/RESULTS.md`). Building the library with `CPPUTEST_C_ONLY=1`
additionally drops the global `operator new` replacement, so `new`/`delete`
in tests run at plain libc speed.

## Performance

The design goals: a passing assertion is an inlined compare plus a counter
bump (no function call, no allocation); leak-tracked `new`/`delete` recycles
blocks through size-class freelists instead of hitting malloc; failure paths
may allocate, passing paths never do; and `-jN` spreads groups across forked
workers. Current numbers, and the harness to reproduce them on your machine,
live in [`bench/RESULTS.md`](bench/RESULTS.md) (`sh bench/run_bench.sh`) —
both sequential and parallel runs are substantially faster than upstream on
the synthetic stress load measured there. C++ compile time is at parity
(both pay for the std headers the `new` macro must pre-include); pure-C test
files compile dramatically faster since they never touch the C++ standard
library.

## Known divergences from upstream

- Upstream's *self*-tests that construct internal classes
  (`MockCheckedExpectedCall`, `MemoryLeakDetector`, `TestOutput` subclass
  internals, ...) are not supported; the behaviors they pin are covered by
  this repo's golden-output suites. Full classification:
  `conformance/SKIPPED.md`.
- `-jN` is an extension; upstream rejects the flag.
- Out-of-scope: GTest convertor, MemoryReport plugins, IEEE754 plugin.
- Not yet implemented: the `MockFailureReporter` injection surface
  (`setMockFailureStandardReporter`/`setActiveReporter`) and the standalone
  comparator-repository API; mock failures always report through the normal
  test-failure path (which `MockSupportPlugin` relies on, and which behaves
  identically in practice).
- Not yet implemented: the `TestMemoryAllocator` injection surface
  (`setCurrentNewAllocator` and custom allocator subclasses). The leak
  detector always uses the built-in tracker; projects that swap allocators
  to simulate OOM will not compile.
- Shuffle order for a given seed matches upstream only on the same libc
  (both use `srand`/`rand`).
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
self-assessment: the gates are golden-output tests, upstream's own suites
compiled unmodified, differential fuzzing against the real upstream build,
sanitizers, and static analysis.

The human contribution was a handful of prompts. The initial one, roughly:

> *Rewrite CppUTest from scratch: a C11 core with a thin C++ header shim,
> source-compatible with upstream — existing test suites must recompile
> unchanged with byte-identical output. Plain gcc + make, no cmake. Vendor
> upstream as a read-only oracle and prove conformance by running its own
> tests against the new library. Keep PLAN.md as durable state and work in
> small iterations that each end with the check script green and a commit.*

Steering after that amounted to incantations like: *fuzz test* · *test
against the original implementation* · *rewrite in C, keep C++ headers* ·
*rewrite for performance, minimal virtualization, minimal allocations* ·
*allow multi-threading via fork or another mechanism with memory isolation*.
Everything else — design, debugging, verification, and the decision of what
to do next — was Claude-driven.

## License

BSD-3-Clause — see `LICENSE`. The interface (header names, macro shapes,
message formats) and a small number of macro bodies follow CppUTest
(BSD-3-Clause, see `third_party/cpputest/COPYING`); the implementation is
new code.

cpputest-turbo is an independent reimplementation and is not affiliated
with or endorsed by the CppUTest project. "CppUTest" refers to the upstream
project at cpputest.github.io, whose public interface this project
reproduces for source compatibility.
