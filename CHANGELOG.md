# Changelog

All notable changes to cpputest-revibed. Format follows
[Keep a Changelog](https://keepachangelog.com); versions are git tags.

## [Unreleased]

### Added
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
