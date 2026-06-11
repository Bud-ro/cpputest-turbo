# Changelog

All notable changes to cpputest-turbo. Format follows
[Keep a Changelog](https://keepachangelog.com).

Versioning: the repository carries a single moving tag,
`latest-passing-build+1.0.0` — it tracks the most recent commit where the
full gate (build, unit, conformance, vendor smoke, sanitizers, and the
differential fuzzers against the vendored upstream) is green, and the
`+1.0.0` marks the project as functionally complete. The historical
`v0.1.0`–`v1.0.0` tags referenced by the entries below were retired in
favor of this scheme; the entries are kept for history.

## [Unreleased]

### Fixed
- `-p` and `-jN` no longer duplicate previously-buffered output when stdout
  is piped/redirected: the console output now flushes after every print
  (upstream `ConsoleTestOutput` parity), and both fork sites flush first.
- The 50-dots-per-line counter resets at each run summary, so `-r` repeats
  break lines at the same positions as upstream.
- Failure messages are no longer truncated at 8 KB (long `STRCMP_EQUAL`
  operands now render in full, like upstream's unbounded messages).
- C mock interface normalizes bool values at every boundary
  (`withBoolParameters("b", 2)` now matches `1`, as upstream does).
- `SIGNED_BYTES_EQUAL_LOCATION` now has upstream's 4-parameter signature and
  `SIGNED_BYTES_EQUAL_TEXT_LOCATION` exists (source-compat fix).
- `-s <negative>` is rejected like upstream instead of being wrapped to a
  huge unsigned seed (`SimpleString::AtoU` digit parsing).
- `cpputest_realloc` keeps the original block valid and tracked when the new
  allocation fails (per the realloc contract); previously a later free of it
  could corrupt the heap.
- `-jN` no longer reports success when a worker `fork()` fails or a worker
  cannot write its stats file.
- A stale `MockActualCall_c`/`MockExpectedCall_c` handle can no longer
  dereference freed memory after `checkExpectations()`/`clear()`.
- Incremental builds now rebuild objects when headers change (`-MMD` deps);
  archives are recreated instead of updated in place.
- The sanitizer sweep now catches standalone UBSan reports (`runtime
  error:` without the `ERROR:` marker) and sanitizer aborts.
- The differential fuzzer links the mock core from the instrumented archive
  (it was silently resolving from the uninstrumented library).
- Sanitizer/fuzz scripts honor `CC`/`CXX`, so the clang CI legs actually
  test clang.
- `bench/run_bench.sh` works on macOS (`date +%s%N` fallback) and builds an
  upstream cache that also satisfies the fuzz gate (cache poisoning fix).
- Framework-internal allocations are checked (`cu_x*` helpers): OOM aborts
  loudly instead of dereferencing NULL. Tracked test allocations still
  return NULL for the OOM-simulation API.
- JUnit `<system-out>` accumulation is linear instead of O(n²) in the
  number of prints.
- clang builds the library warning-clean: printf-format attributes on the
  variadic helpers (clang's `-Wformat-nonliteral` does not exempt va_list
  functions like gcc's) and a `void *` hop for its stricter `-Wcast-align`.
- Sanitizer gates run on macOS (LeakSanitizer is Linux-only; `detect_leaks`
  is now forced off on Darwin) and under clang's ASan (`poison_array_cookie=0`
  — the leak tracker legitimately 0xCD-fills the new[] cookie on delete[]).

### Added
- `make lint` / `scripts/check-analyzer.sh`: `gcc -fanalyzer` static
  analysis over the C core and mock core, wired into `check.sh` and CI
  (skips gracefully on compilers without the analyzer).
- More warnings: `-Wpointer-arith -Wvla -Wformat=2 -Wcast-align
  -Wnull-dereference -Wdouble-promotion` (+ `-Wnon-virtual-dtor` for C++),
  plus compiler-probed gcc-only diagnostics (`-Wlogical-op
  -Wduplicated-cond -Wduplicated-branches -Wjump-misses-init
  -Wuseless-cast -Wzero-as-null-pointer-constant`).

## [v1.0.0] — 2026-06-10

### Changed
- Project renamed to **cpputest-turbo** (library and header names keep the
  CppUTest-compatible spelling — that is the interface).
- `include/cpputest_core/` headers are now explicitly internal (no
  stability guarantee); the supported C API is `TestHarness_c.h` +
  `MockSupport_c.h`.
- Required build flags raised to maximum strictness: `-Wpedantic -Wshadow
  -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wwrite-strings
  -Wconversion -Wsign-conversion` under `-Werror`.

### Added
- clang-format style (`.clang-format`, applied repo-wide), `.editorconfig`,
  and `scripts/check-format.sh` (style + hygiene) wired into `check.sh`
  and CI.
- Differential fuzzing across optimization levels (`FUZZ_OPT`); CI deep-fuzz
  matrix runs -O0/-O1/-O2/-O3.
- "Lean mode" documentation: cuts preprocessed test-TU size by more than
  an order of magnitude via `-DCPPUTEST_MEM_LEAK_DETECTION_DISABLED
  -DCPPUTEST_USE_STD_CPP_LIB=0` (see bench/RESULTS.md).
- `onObject()` on expected and actual mock calls (object-instance matching,
  with upstream's `MockUnexpectedObjectFailure` /
  `MockExpectedObjectDidntHappenFailure` messages).
- `mock().tracing(bool)` and `getTraceOutput()` — actual calls are recorded
  in upstream's exact trace format instead of matched.
- `MockSupportPlugin` comparator/copier pipeline: registrations install
  before every test, `postTestAction` skips `checkExpectations()` on
  already-failed tests, comparators removed after each test.
- `withName()` on expected and actual calls; `withCallOrder()` on actual
  calls (no-op when matching, recorded when tracing).
- Integer widening coercion in typed return getters (C++ and C API),
  matching upstream `MockNamedValue`.
- `make install` / `make uninstall` (PREFIX/DESTDIR) and a pkg-config file.
- LICENSE (BSD-3-Clause, dual copyright with upstream attribution).
- `-DCPPUTEST_MEM_LEAK_DETECTION_DISABLED` honored like upstream.
- `scripts/check.sh` now gates the ASan/UBSan sweep and a bounded
  differential fuzz pass by default (`CHECK_FAST=1` to skip).

### Changed
- `SimpleString` `operator==`/`!=` are friend free functions, so
  `"literal" == str` compiles (upstream signature).
- `expectNoCall()` returns `void` (upstream signature).
- User-supplied `CFLAGS`/`CXXFLAGS` no longer override the flags the build
  requires.
- `crashOnFailure` reports the mock failure before trapping.

### Fixed
- Mock failure messages larger than 8 KiB were truncated.
- Freeing/reallocating a tracked pointer after leak tracking was toggled
  off corrupted the heap (interior pointer passed to raw `free`).
- Size-arithmetic overflow in the leak tracker for huge allocations.
- Parallel workers use a private `mkdtemp` directory instead of predictable
  shared `$TMPDIR` filenames.

## [v0.3.0] — 2026-06-10

Pre-release snapshot rolled into v1.0.0 the same day; its changes
(licensing/packaging groundwork, `onObject()`/tracing/`MockSupportPlugin`
parity, `crashOnFailure` ordering, the 8 KiB message-truncation fix, and
the hardened gates) are documented under the v1.0.0 entry above.

## [v0.2.0] — 2026-06-09

### Changed
- Assertion fast path inlined into user translation units: passing
  assertions cost ~0.3 ns (10× faster); the library is entered only on
  failure.
- Leak tracker uses a single allocation per tracked block plus size-class
  freelists: 6.4 ns per tracked new/delete pair (2.2×).
- Net: ~5× faster than upstream CppUTest on an assertion/allocation stress
  load sequentially, ~14× with `-j8`.

### Added
- Differential mock fuzzer (`fuzz/fuzz_mock_diff.cpp`): the same public-API
  driver compiles against upstream and us; outputs diffed byte-for-byte.
  Plus seeded ASan/UBSan fuzzers for the CLI parser, failure formatters and
  leak tracker, and an adversarial mock torture suite.

### Fixed
- 14 mock behavior/parity bugs found by differential fuzzing (duplicate
  parameter names, scope state propagation/inheritance, check counting,
  output parameter typing, `expectedCallsLeft` finalization, and more).
- Failure messages over 1 KiB were truncated by the output path.

## [v0.1.0] — 2026-06-09

Initial complete rewrite: C11 core + thin C++ shim, source-compatible with
CppUTest. 210 unmodified upstream tests pass; byte-identical output;
fork isolation (`-p`) and parallel workers (`-jN`); pure-C consumer path;
Linux + macOS (cross-compile verified).
