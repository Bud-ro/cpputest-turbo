# cpputest-revibed

A from-scratch rewrite of [CppUTest](https://cpputest.github.io/) with a **C11
core** and a thin C++ header shim. Source-compatible with upstream — existing
CppUTest suites recompile unchanged — with byte-identical output, faster
runtime, process-level test isolation, and first-class support for projects
that only have a C compiler.

```
include/CppUTest/      C++ shim headers (upstream-identical names & macros)
include/CppUTestExt/   CppUMock shim + OrderedTest
include/cpputest_core/ the C ABI (pure-C consumers use this directly)
src/core/              C11 core: runner, assertions, output, leak detection
src/mock/              C11 mock core + MockSupport_c
src/shim/              the only C++ TUs in the library (operator new, SimpleString)
```

## Building

```sh
make            # gcc + POSIX make: build/libCppUTest.a, build/libCppUTestExt.a
make CPPUTEST_C_ONLY=1   # C compiler only — no C++ anywhere
./scripts/check.sh       # full test + conformance gate
```

No cmake, no autotools. Linux and macOS (`scripts/check-macos.sh`
cross-compiles for both macOS architectures with `zig cc`).

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

Either pair C test files with the upstream-compatible C++ wrapper pattern, or
go 100% C: register `cu_test` records with `cpputest_core/core.h` and call
`cu_run_all(argc, argv)` (see `tests/c_interface/c_core_tests.c`).

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

## Performance

See `bench/RESULTS.md` (reproduce with `sh bench/run_bench.sh`): runtime is
~25–30% faster than upstream on an assertion/allocation stress load; C++
compile time is at parity (both pay for the std headers the `new` macro must
pre-include); pure-C test files compile dramatically faster since they never
touch the C++ standard library.

## Known divergences from upstream

- Upstream's *self*-tests that construct internal classes
  (`MockCheckedExpectedCall`, `MemoryLeakDetector`, `TestOutput` subclass
  internals, ...) are not supported; the behaviors they pin are covered by
  this repo's golden-output suites. Full classification:
  `conformance/SKIPPED.md`.
- `-jN` is an extension; upstream rejects the flag.
- Out-of-scope: GTest convertor, MemoryReport plugins, IEEE754 plugin.
- Shuffle order for a given seed matches upstream only on the same libc
  (both use `srand`/`rand`).
- Failing assertions longjmp out of the test (upstream's no-exceptions mode);
  C++ temporaries alive at that moment are not destructed. Bounded to failure
  paths; the sanitizer sweep (`scripts/check-sanitizers.sh`) accounts for it.

## License

The interface (header names, macro shapes, message formats) follows CppUTest,
BSD-3-Clause — see `third_party/cpputest/COPYING`. The implementation here is
new code.
