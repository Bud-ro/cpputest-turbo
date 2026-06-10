# cpputest-revibed — Execution Plan

Rewrite of CppUTest (https://cpputest.github.io/) with a C11 core and a thin C++
header shim. This file is the durable state for the work loop: each iteration
picks the next unchecked item in order, implements it, verifies it, checks it
off, and commits. Do not skip ahead past unchecked items unless blocked (note
blockers in the **Blockers** section).

## Locked decisions (do not re-litigate)
- **Core language:** C11, built with plain `gcc` + POSIX `make`. No cmake
  required for consumers. Core exposes a stable C ABI.
- **Interface:** Byte-compatible *source* interface with upstream CppUTest.
  Same header paths (`CppUTest/TestHarness.h`, `CppUTestExt/MockSupport.h`, …),
  same macros, same observable runner behavior/output. Headers are thin C++
  shims that lower to C ABI calls — minimal templates, no deep include chains.
  Pure-C consumers use `CppUTest/TestHarness_c.h` against the C core directly.
- **Conformance:** Upstream repo vendored at `third_party/cpputest` (reference
  only — never edit it). Goal is to compile and pass the public-API subset of
  `third_party/cpputest/tests/`, with skipped internals-only tests recorded in
  `conformance/SKIPPED.md` (file + reason each).
- **Process model:** Legacy sequential in-process execution is the default.
  `fork`-based isolation and parallel workers are opt-in flags. POSIX
  (Linux + macOS) are the supported platforms.
- **Scope:** CppUTest core + CppUMock (MockSupport). GTest convertor and
  rarely-used CppUTestExt extras are out of scope.
- **Performance goals:** measurably faster user-test *compile* times than
  upstream (thin headers) and faster suite runtime (no SimpleString allocation
  churn, no virtual dispatch in hot paths in the core).

## Layout
```
include/CppUTest/      C++ shim headers, upstream-identical names
include/CppUTestExt/   Mock shim headers
src/core/              C11 core (registry, runner, assert, output, leak detect)
src/mock/              C11 mock core
tests/                 Our own unit tests for the core (plain C where possible)
conformance/           Harness that builds upstream tests against our lib
third_party/cpputest/  Vendored upstream (READ ONLY oracle)
bench/                 Compile-time + runtime benchmarks vs upstream
Makefile               Builds lib, tests, conformance, bench
```

## Phases

### Phase 0 — Scaffolding
- [x] 0.1 Makefile skeleton: builds `libCppUTest.a` from `src/core`, `make check`
      runs `tests/`, `make conformance` placeholder. gcc, `-std=c11 -Wall -Wextra -Werror`.
- [x] 0.2 `scripts/check.sh`: one command that builds everything + runs all suites;
      loop iterations must end green on this.
- [x] 0.3 Read upstream public headers + key sources (Utest, UtestMacros,
      TestRegistry, TestOutput, CommandLineTestRunner, TestHarness_c) and write
      `docs/INTERFACE.md`: the exact macro/class/function surface we must
      reproduce, and upstream's default output format (normal/verbose/color),
      exit codes, and CLI flags. This is the contract document.

### Phase 1 — Core runner walking skeleton
- [x] 1.1 C core: test registry (static registration), test descriptor structs,
      run loop, result counting, plain-text output matching upstream's default
      format exactly (incl. `OK (N tests, N ran, ...)` summary and failure format).
- [x] 1.2 C++ shim: `TestHarness.h`, `Utest.h`, `UtestMacros.h` subset —
      `TEST_GROUP`, `TEST`, `IGNORE_TEST`, `TEST_GROUP_BASE`, setup/teardown
      hooks, static auto-registration. A sample test file compiles with
      `g++ -I include` and runs with correct output.
- [x] 1.3 `CommandLineTestRunner.h` shim + C core arg parsing: full upstream
      parser (all filters incl. exclude/strict variants, repeat, shuffle,
      reverse, listings, -ri, -f, -e/-ci, -o types, -k, TEST(...) form, usage +
      help text verbatim), exit code semantics. JUnit/TeamCity output selection
      parsed but falls back to console until Phase 3; -p parsed, runs in Phase 8.
- [x] 1.4 Our own smoke tests in `tests/` + wire into `scripts/check.sh`.

### Phase 2 — Assertions & SimpleString shim
- [x] 2.1 C core assertion engine: failure capture (longjmp out of test body,
      mirroring upstream's no-exceptions mode), file/line, message formatting
      into fixed/arena buffers (no per-assert heap churn).
- [x] 2.2 All check macros with upstream-identical failure text: `CHECK`,
      `CHECK_TRUE/FALSE`, `CHECK_EQUAL`, `CHECK_COMPARE`, `STRCMP_*` (incl.
      nocase/contains), `LONGS_EQUAL`, `UNSIGNED_LONGS_EQUAL`,
      `LONGLONGS_EQUAL`, `BYTES_EQUAL`, `SIGNED_BYTES_EQUAL`, `POINTERS_EQUAL`,
      `FUNCTIONPOINTERS_EQUAL`, `DOUBLES_EQUAL`, `MEMCMP_EQUAL`,
      `BITS_EQUAL`, `ENUMS_EQUAL_INT`, `FAIL`/`FAIL_TEST`, `CHECK_TEXT` variants,
      `CHECK_THROWS` (C++ shim only).
- [x] 2.3 `SimpleString.h` shim: minimal C++ `SimpleString` class (wrapping core
      C string-builder) sufficient for the public API: `StringFrom*` overloads,
      `StringFromFormat`, concatenation, comparison — what user code and
      upstream public-API tests actually touch.
- [x] 2.4 `CHECK_EQUAL` template plumbing: works for any type with `operator==`
      and `StringFrom` overload, like upstream.

### Phase 3 — Output formats & runner polish
- [x] 3.1 Verbose (`-v`) and color (`-c`) output, exact upstream formats.
- [x] 3.2 JUnit XML output (`-ojunit`), file naming + escaping parity.
- [x] 3.3 TeamCity output (`-oteamcity`).
- [x] 3.4 (landed with 1.3) Test filters: name/group, strict (`-sg/-sn`), exclude (`-xg/-xn`),
      repeat, shuffle w/ seed parity, `-ll`/list options if present upstream.
- [x] 3.5 `TestPlugin.h` shim + C core plugin hooks (preTestAction/postTestAction);
      `SetPointerPlugin` parity.

### Phase 4 — Memory leak detection
- [x] 4.1 C core allocator interposition: counting allocator wrapping
      malloc/free/realloc with per-test balance, allocation-site recording
      (file/line via macros), leak report format parity.
- [x] 4.2 `MemoryLeakDetectorNewMacros.h` / `MallocMacros.h` shims: operator
      new/delete overrides routing into the C core tracker; `NewMacros` on by
      default via `TestHarness.h` like upstream.
- [x] 4.3 `MemoryLeakWarningPlugin.h` shim: enable/disable, expected-leaks API
      (`EXPECT_N_LEAKS`), ignore-when-debugging behaviors.
- [ ] 4.4 (deferred to Phase 7, conformance-driven) `TestMemoryAllocator.h` shim subset used by public API tests
      (setCurrentNewAllocator etc. to the extent upstream tests require).

### Phase 5 — C interface & fixtures
- [x] 5.1 `TestHarness_c.h`: `TEST_C`, `TEST_GROUP_C_*`, `CHECK_*_C` macros,
      `cpputest_malloc/free` — pure C consumer path, tested with `gcc -std=c11`.
- [x] 5.2 `TestTestingFixture.h` shim (upstream's tests use it heavily to test
      the framework itself — needed for conformance).
- [x] 5.3 `OrderedTest` (CppUTestExt) support.

### Phase 6 — CppUMock
- [x] 6.1 C mock core: expectation store, call matching, parameter records
      (int/unsigned/long/longlong/double/string/pointer/funcptr/mem buffer/
      object), call order, unexpected/unfulfilled failure messages w/ upstream
      text parity.
- [x] 6.2 (onObject/tracing deferred to conformance) `MockSupport.h` C++ shim: `mock()`, `expectOneCall`, `expectNCalls`,
      `actualCall`, `withParameter` overload set, `andReturnValue` set,
      `returnValue()` accessors, scopes (`mock("scope")`), `checkExpectations`,
      `clear`, `ignoreOtherCalls`, `disable/enable`, `crashOnFailure`.
- [x] 6.3 Comparators & copiers: `installComparator/Copier`,
      `withParameterOfType`, `withOutputParameter[OfType]`.
- [x] 6.4 `MockSupport_c.h` C interface for mocks (pure C impl over the mock core).
- [x] 6.5 MockSupportPlugin parity (auto checkExpectations/clear per test; comparator repository install deferred).

### Phase 7 — Conformance against upstream suite
- [x] 7.1 `conformance/` harness: bundles manifest files (UNMODIFIED upstream
      sources) into one binary against our headers+libs; full classification
      in `conformance/SKIPPED.md` (PASSING/PLANNED/INTERNALS/OUT-OF-SCOPE).
- [ ] 7.2 Get all PASS-required CppUTest-core files compiling and green.
- [ ] 7.3 Same for `tests/CppUTestExt/` mock tests.
- [x] 7.4 Wire conformance into `scripts/check.sh`.

### Phase 8 — Fork isolation & parallelism (opt-in)
- [ ] 8.1 `-pfork`-style per-test fork isolation: crash containment (signal →
      test failure w/ signal name), leak-detector interplay documented.
- [ ] 8.2 Parallel workers (`-jN`): group-granularity scheduling across forked
      workers, deterministic merged output, exit-code aggregation.
- [ ] 8.3 Tests for both, incl. a deliberately-crashing test contained.

### Phase 9 — Portability, benchmarks, docs
- [ ] 9.1 macOS portability pass: no Linux-isms (use `zig cc` cross-compile
      smoke check or guard with feature macros); CI script for both OSes.
- [ ] 9.2 `bench/`: compile-time benchmark (N-file test suite, ours vs upstream
      headers) and runtime benchmark; record results in `bench/RESULTS.md`.
- [ ] 9.3 README: drop-in migration guide, flag reference, divergences list.
- [ ] 9.4 Final sweep: `scripts/check.sh` green, valgrind clean on our suite,
      `-Wall -Wextra -Werror` clean, tag v0.1.0.

## Blockers
(none)

## Iteration log
(append one line per loop iteration: date, items done, anything learned)
- 2026-06-09 #18: conformance burn-down 2 — SetPluginTest (3) + PluginTest (10) PASS UNMODIFIED. Added: TestRegistry instances w/ setCurrentRegistry (core-list swap), countPlugins, runAllTests(TestResult&) w/ output capture; TestResult(TestOutput&) ctor + per-run stats; UtestShell getName/getGroup/getFile return SimpleString (+ getFormattedName/getMacroName/willRun/hasFailed); PlatformSpecificSrand/Rand as swappable core symbols (shuffle uses them); PlatformSpecificFunctions.h. CRITICAL FIX: TestTestingFixture now owns a real TestRegistry instance (upstream semantics) — before, fixture getRegistry() returned the global registry and PluginTest installed/deleted heap plugins into the outer chain → use-after-free in post-test hooks. Bundle now 210 tests green.
- 2026-06-09 #17: conformance burn-down — TestUTestMacro.cpp (148 tests) + TestUTestStringMacro.cpp (46 tests) PASS UNMODIFIED: the entire upstream assertion-macro suite now verifies our engine. Fixes: TestOutput.h shim (TestOutput/Console/StringBufferTestOutput), UtestShell::print SimpleString template overload, checkTestFailsWithProperTestLocation rewritten upstream-exact (incl. lineExecutedAfterCheck jump verification), ContainsFailure renders NULL as empty string not (null). Conformance bundle: 197 tests / 146 ran / 51 ignored, green.
- 2026-06-09 #16: 7.1+7.4 done — conformance harness runs upstream sources UNMODIFIED (CheatSheetTest, MockCheatSheetTest, PreprocessorTest, CompatabilityTests green). Key fix: NewMacros must pre-include <new>/<memory>/<string> before defining the new macro (upstream dance) or user includes after TestHarness.h explode. Config gains CPPUTEST_USE_STD_CPP_LIB/USE_MEM_LEAK_DETECTION defaults. SKIPPED.md classifies all ~60 upstream files; PLANNED blockers: TestOutput.h shim (StringBufferTestOutput), TestFilter.h, TestFailure surface, MockFailure.h + failure-reporter injection in mock core, SimpleString statics. 7.2/7.3 = burn down PLANNED list.
- 2026-06-09 #15: 6.4 done, Phase 6 COMPLETE — MockSupport_c.h upstream-identical struct surface implemented in pure C over the mock core (function-pointer tables on current-scope/expectation/actual globals like upstream; fn-ptr types need typedefs to pass through macros); 6 pure-C mock tests green. Deferred to conformance: mock tracing, onObject, MockSupportPlugin comparator repository, mock failure-reporter injection. NEXT: Phase 7 conformance harness.
- 2026-06-09 #14: mock slice 5 / 6.3 done — comparator/copier registry in C core (C++ virtuals adapted via extern-C trampolines), withParameterOfType (object equality via comparator, custom valueToString in messages), withOutputParameterOfType[Returning] (copier-based), output-param TYPE matching incl. the "Unexpected parameter type" failure case, MockNoWayToCompare/Copy failures (note upstream "MockFailure:" without space), expectation-side type-name copies. REMAINING Phase 6: MockSupport_c (6.4), then mark 6.1/6.2/6.5 done (plugin exists; tracing/onObject deferred to conformance-driven work).
- 2026-06-09 #13: mock slice 4 — output parameters (withOutputParameterReturning/withUnmodifiedOutputParameter/withOutputParameter, copy-on-match incl. the ignore-others first-matching copy path, MockUnexpectedOutputParameterFailure name-case golden-pinned, <output> rendering in callToString + missing-params) + MockSupportPlugin (post-test checkExpectations+clear). LESSON: bash heredoc mangles bang chars in python scripts — three replaces silently no-opd; use the Edit tool for code with exclamation marks. REMAINING in Phase 6: comparators/copiers (installComparator/withParameterOfType + OfType output params), MockSupport_c, tracing; 6.1/6.2 mostly done otherwise.
- 2026-06-09 #12: mock slice 3 — andReturnValue overloads (12 types), returnXValue/OrDefault on actual call + MockSupport-level XReturnValue accessors (reading finalizes the call like upstream checkExpectations-in-returnValue), MockNamedValue facade w/ STRCMP-checked getters, per-scope data store (setData overloads, setDataObject w/ copied type name, getData/hasData). NEXT SLICE: output parameters (withOutputParameterReturning + copy-on-match), MockSupportPlugin, MockSupport_c C interface; then comparators/copiers; tracing if conformance needs it.
- 2026-06-09 #11: mock slice 2 — input parameters: cum_value tagged union (13 types), upstream's integer cross-type matrix collapsed to sign-aware equality (provably equivalent), expected-side double tolerance (default 0.005), full withParameter overload set on both facades, candidate-filtering state machine (discard-on-reopen, finalized-matching removal, parameterWasPassed on all candidates), 3 param failure messages golden-pinned (name/value/missing-params w/ MISSING parameters line). Slice 3 (#12) added return values + data store. Slice 4 (#13) added output params + MockSupportPlugin.
- 2026-06-09 #10: 6.1 basic slice — mock core (src/mock/mock_core.c) + MockSupport.h facade: expectOneCall/expectNCalls/expectNoCall, parameterless actualCall matching w/ deferred finalization (callWasMade on next actualCall/checkExpectations like upstream), strictOrder + out-of-order detection, ignoreOtherCalls, enable/disable, scopes (storage level), crashOnFailure. All 5 failure messages golden-pinned (incl. ordinal "additional (2nd) call" and history trailers). mock() singletons use save/disable tracking to stay out of leak accounting. Slice 2 (#11) added input parameters.
- 2026-06-09 #9: 5.3 done, Phase 5 complete — OrderedTest ported verbatim (ordered chain threaded through registry; needed UtestShell::addTest/getNext + TestRegistry::getFirstTest/getTestWithNext shims). Phase 6 (CppUMock) next: read INTERFACE.md lines 842+ for mock failure formats first.
- 2026-06-09 #8: 5.1+5.2 done — TestHarness_c.h full macro surface w/ harness_c.c in the C core (upstream dispatch parity: BOOL/CHAR/UBYTE/SBYTE via CheckEqualFailure, INT via LongsEqual...); pure-C consumers WORK: c_core_tests.c registers cu_test directly and links the CPPUTEST_C_ONLY lib with plain gcc (verified in check.sh). Output now routes through a swappable sink (cu_set_output_sink) enabling TestTestingFixture (isolated registry swap, output capture, nested-run state save/restore, plugin hooks suppressed in nested runs like upstream's fresh-registry behavior). 5.3 OrderedTest next.
- 2026-06-09 #7: 4.1-4.3 done — memleak.c (guard bytes BAS, 0xCD poison, 4096-byte report buffer w/ footer reservation, hash-table nodes, checking periods), newdelete.cpp = the ONE C++ TU in the lib (replacement operators can't be inline; make CPPUTEST_C_ONLY=1 skips it), New/Malloc macro headers (malloc macros opt-in like upstream), MemoryLeakWarningPlugin shim, runner installs leak plugin + prints FinalReport on green. Dealloc failures use test NAME as failure file (upstream quirk, golden-pinned). 4.4 deferred to conformance.
- 2026-06-09 #6: 3.5 done — Phase 3 complete. TestPlugin chain lives in the C++ shim (verbatim upstream semantics incl. NullTestPlugin terminator, reverse post-action order, disable); C core exposes pre/post/parse hooks; TestRegistry/TestResult/TestFailure shims added; SetPointerPlugin + UT_PTR_SET with the 32-entry table; CommandLineTestRunner installs SetPointerPlugin like upstream. -pXXX args now reach plugin parseArguments.
- 2026-06-09 #5: 3.1-3.4 done — JUnit XML (junit.c: per-group files, check-count deltas, first-failure-only, system-out accumulation never reset, filtered-group cpputest_.xml quirk), TeamCity stream (escaping incl. unescaped-test-file quirk in testFailed), -vv traces upstream-exact, output dispatch + composite junit+console under -v. JUnit drops print(number) like upstream no-ops. 3.5 plugins next.
- 2026-06-09 #1: Phase 0 complete (Makefile, check.sh, docs/INTERFACE.md). Key parity traps from upstream: tests run in REVERSE registration order; exit code = failure count (ran-nothing counts as an error); `"expected <%s>\n\tbut was  <%s>"` has two spaces; IGNORE_TEST installer name lacks an underscore (upstream bug, must reproduce); CHECK_EQUAL double-evaluates with WARNING on mismatch.
- 2026-06-09 #4: Phase 2 done — C core assertion engine (assert.c) with byte-exact formats for all 25+ macro failure modes (verified by 325 lines of golden output); SimpleString shim + StringFrom/HexStringFrom overload sets; UtestShell gets upstream's full assert method surface. Found: upstream FAIL counts a check (goldens updated); CHECK_COMPARE counts NO check on success; the double-eval WARNING can't be unit-tested under -Werror=sequence-point (conformance will cover). sb_raise uses 8KB static buffer (messages truncate beyond — fine vs upstream's 4KB leak buffer norms).
- 2026-06-09 #3: 1.3 done — full CommandLineArguments port (dispatch order preserved verbatim; quirks: malformed TEST(...) args never error, -s0 glued is a parse error, -pXXX hits the plugin branch and errors with no plugins, -lg wins over -ln). Shuffle = srand/rand Fisher-Yates like upstream. 36 CLI behavior tests green. Phase 1 complete; Phase 2 (assertions+SimpleString) next.
- 2026-06-09 #2: 1.1+1.2+1.4 done — C core (registry/runner/output/platform, setjmp-stack protected sections) + header shim (Utest.h/UtestMacros.h/TestHarness.h/CommandLineTestRunner.h) + golden-output smoke tests (normal/verbose/pass), all byte-exact. Gotcha: -std=c11 needs _POSIX_C_SOURCE for clock_gettime. 1.3 (CLI parser) next.
