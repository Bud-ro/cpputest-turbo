# cpputest-turbo

[![ci](https://github.com/Bud-ro/cpputest-turbo/actions/workflows/ci.yml/badge.svg)](https://github.com/Bud-ro/cpputest-turbo/actions/workflows/ci.yml)

A from-scratch rewrite of [CppUTest](https://cpputest.github.io/) with a **C11
core** and a thin C++ header shim. Source-compatible with upstream — existing
CppUTest suites recompile unchanged — with byte-identical output, faster
runtime, process-level test isolation, and first-class support for projects
that only have a C compiler.

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
and macOS both qualify (developed with gcc 13; clang works). The C++ shim
needs C++11. Linux and macOS (`scripts/check-macos.sh` cross-compiles for
both macOS architectures with `zig cc` 0.16). Pre-PR gate:
`./scripts/check.sh` (build, unit + conformance suites, `gcc -fanalyzer`
static analysis, ASan/UBSan sweep, bounded differential fuzz; `CHECK_FAST=1`
skips the heavy stages).

## Drop-in migration

Point your include path at `include/` and link `build/libCppUTest.a`
(plus `libCppUTestExt.a` for mocks). Test sources are unchanged:

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
defining the leak-detection `new` macro — that is ~30k preprocessed lines
per test file, the dominant compile cost (same as upstream). If you don't
use the leak detector or SimpleString's `std::string` interop, compile your
test files with:

```sh
-DCPPUTEST_MEM_LEAK_DETECTION_DISABLED -DCPPUTEST_USE_STD_CPP_LIB=0
```

which shrinks a test TU from ~30,700 to ~1,100 preprocessed lines — about
**4× faster compilation** (106 ms → 27 ms per TU at -O0 on the benchmark
machine). Building the library with `CPPUTEST_C_ONLY=1` additionally drops
the global `operator new` replacement, so `new`/`delete` in tests run at
plain libc speed.

## Performance

See `bench/RESULTS.md` (reproduce with `sh bench/run_bench.sh`; numbers
from one Linux/gcc-13 machine on a synthetic stress load): sequential runs
are **~5× faster** than
upstream (passing assertions are an inlined compare + counter bump — ~0.3 ns;
leak-tracked new/delete recycles through size-class freelists — 6.4 ns/pair),
and `-j8` reaches **~14×**. C++ compile time is at parity (both pay for the
std headers the `new` macro must pre-include); pure-C test files compile
dramatically faster since they never touch the C++ standard library.

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
- Shuffle order for a given seed matches upstream only on the same libc
  (both use `srand`/`rand`).
- Failing assertions longjmp out of the test (upstream's no-exceptions mode);
  C++ temporaries alive at that moment are not destructed. Bounded to failure
  paths; the sanitizer sweep (`scripts/check-sanitizers.sh`) accounts for it.

## License

BSD-3-Clause — see `LICENSE`. The interface (header names, macro shapes,
message formats) and a small number of macro bodies follow CppUTest
(BSD-3-Clause, see `third_party/cpputest/COPYING`); the implementation is
new code.

cpputest-turbo is an independent reimplementation and is not affiliated
with or endorsed by the CppUTest project. "CppUTest" refers to the upstream
project at cpputest.github.io, whose public interface this project
reproduces for source compatibility.
