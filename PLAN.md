# cpputest-turbo — Execution Plan

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
- [x] 7.2 PASS-required core files green (CheatSheet, Preprocessor, Compatability, TestUTestMacro, TestUTestStringMacro, SetPlugin, Plugin — 210 upstream tests). Remaining PLANNED files need upstream-internal class surfaces (documented per file in SKIPPED.md).
- [x] 7.3 MockCheatSheetTest green; remaining mock self-tests require constructing upstream-internal MockChecked*/MockFailure classes — behavior is covered byte-exact by tests/mock goldens (line documented in SKIPPED.md).
- [x] 7.4 Wire conformance into `scripts/check.sh`.

### Phase 8 — Fork isolation & parallelism (opt-in)
- [x] 8.1 `-p` per-test fork isolation, upstream-exact semantics and messages ("Failed in separate process[- killed by signal N]", EINTR retry, SIGCONT).
- [x] 8.2 `-jN` parallel workers: round-robin group scheduling over forked workers, per-worker output/stats temp files replayed in worker order (deterministic), merged summary, dead-worker reporting; composes with -p.
- [x] 8.3 tests/process: SIGSEGV containment under -p, -j determinism, -j -p composition, dead-worker reporting.

### Phase 9 — Portability, benchmarks, docs
- [x] 9.1 macOS portability: scripts/check-macos.sh cross-compiles core+mock+shim+tests for aarch64-macos and x86_64-macos via zig cc (compile-only, green).
- [x] 9.2 bench/: run_bench.sh (builds upstream lib for comparison) + RESULTS.md — runtime ~25-30% faster (11-12ms vs 15ms on 2.5M asserts + 250k new/delete); C++ compile parity (libstdc++-dominated both sides; SimpleString bodies moved out-of-line to get there); pure-C TUs skip C++ headers entirely.
- [x] 9.3 README: drop-in migration guide, flag reference, divergences list.
- [x] 9.4 Final sweep: check.sh green; ASan/UBSan sweep green (scripts/check-sanitizers.sh; valgrind unavailable — sanitizers used instead; longjmp-over-temporaries on failure paths documented); -Wall -Wextra -Werror clean throughout; tagged v0.1.0.

## Blockers
(none)

## Publish blockers (from the 5-agent review, 2026-06-09)
- [x] P1 LICENSE file (BSD-3, dual copyright) + upstream attribution header in UtestMacros.h (copied macro bodies)
- [x] P1 Makefile: user CFLAGS must not drop -std=c11/-Iinclude (split REQ_CFLAGS)
- [ ] P1 SimpleString operator==/!= as free functions (reversed-literal comparisons don't compile)
- [x] P1 mock onObject() (expected+actual, core + facade + failure messages)
- [x] P1 MockSupportPlugin: installComparator/Copier, !hasFailed gating on checkExpectations, removeAll after clear
- [x] P2 mock tracing()/getTraceOutput(); actual withCallOrder(); withName() (reporter/repository hooks deferred: need the MockFailure class hierarchy — documented divergence in README, post-publish)
- [x] P2 honor -DCPPUTEST_MEM_LEAK_DETECTION_DISABLED
- [x] P2 check.sh gates sanitizers + bounded fuzz (CHECK_FAST=1 skips); sweep covers leaks/process/c_interface/torture
- [x] P3 install target + pkg-config; CHANGELOG; README disclaimer + requirements; expectNoCall returns void
- [x] P3 crashOnFailure: report before crash; SimpleString utility surface (find/replace/split/VStringFromFormat/std::string); CI workflow added

## Iteration log
(append one line per loop iteration: date, items done, anything learned)
- 2026-06-10 #32 (composition fuzzer): fuzz/fuzz_compose_diff.cpp — third differential pair: every assertion-macro family (passing AND deliberately failing), tracked new/delete churn, cpputest_malloc/realloc/free, DELIBERATE leaks (leak-report parity: content dump memset-deterministic; norm() gains Alloc-num + heap-address rules since the ordinal counts internal allocations which legitimately differ), SimpleString ops (printed), UT_PTR_SET, CHECK_THROWS, and mock expectations interleaved with user heap ops (the implicit-allocation composition ask). First run: byte-identical to upstream modulo the two new normalizations — no bugs found, which itself validates the leak-report/assert parity under composition. 15-round triple diff + full gate green.
- 2026-06-10 #31 (mock_c differential fuzzer; 3 scope bugs): new fuzz/fuzz_mock_c_seq.c (plain C11 TU driving the whole MockSupport_c surface, values printed) + fuzz_mock_c_diff.cpp C++ runner — the C-code-under-test/C++-test-binding split — wired into check-fuzz.sh as a second diff pair. Caught 4 real bugs the C++ fuzzer missed: (1) mock_scope_c(s)->clear() cleared ALL scopes (upstream clear_c clears currentMockSupport only); (2) scoped checkExpectations/expectedCallsLeft must examine that scope ONLY (new cum_check_expectations_scope/cum_expected_calls_left_scope; failure listing filtered to the scope) — C++ facade had the same bug, masked because the old fuzzer only ever checked via the global mock; (3) stringReturnValue on an ignored call returns "" not NULL (MockIgnoredActualCall); (4) returnValue_c/getData_c COUNT one check per conversion (upstream getMockValueCFromNamedValue reads via type-asserted getters; object branch excepted). Getter freshness gated in the driver: upstream's static actualCall dangles after clear/checkExpectations — real upstream UAF hazard, not comparable. 15-round dual diff + full gate green.
- 2026-06-10 #30 (integration-bug sweep: includes, vendoring, return-value parity): all own-header includes converted from <angle> to "quoted" form (16 files; the reported vendoring breakage). New scripts/check-vendor.sh wired into check.sh: copies the tree into a fake host's third_party/, builds nested via make -C (default + CPPUTEST_C_ONLY + BUILD= override), compiles+runs host C++ and pure-C consumers. Differential fuzzer rewritten to cover the ENTIRE mock API (all 12 andReturnValue overloads, all 26 return*Value[OrDefault] getters, named with*Parameter variants, MockSupport-level getters, full data store, withCallOrder) and getters now PRINT their values so silent value-level divergence diffs. That immediately exposed a return-value model gap vs upstream: upstream's "empty" MockNamedValue is a NAMED int-0 (getName ""/"no return value"/"returnValue"), hasReturnValue is name-based (TRUE for unmatched calls!), int-family getters on empty return 0 silently via widening coercion, ignored calls skip type asserts entirely. Reworked: cum_actual_return_value tri-state+ignored (CUM_RET_*), MockNamedValue gains name_/getName + upstream empty semantics (hasValue/rawValue dropped, unused), actual/support facades + MockSupport_c rebuilt on the 4-state model (C support table now shares the actual-call getters like upstream's gMockSupport; hasReturnValue_c is scope-level like upstream). API parity: upstream spells tolerance as a withDoubleParameter OVERLOAD — added (our AndTolerance name kept as alias); MockNamedValue::getName added. 10-round diff fuzz + full gate green.
- 2026-06-10 #29 (multi-agent review + fixes + lint): 3-agent correctness/perf/build review; 2 HIGH reproduced bugs fixed — -p/-jN duplicated buffered stdout into forked children (emit_str now fflushes like upstream ConsoleTestOutput; both fork sites flush first) and the sanitizer/fuzz gates had silent-pass gaps (UBSan "runtime error:" not matched by ERROR-grep; differential fuzzer linked mock core from the UNinstrumented lib — link order). Also: 8KB sb_raise truncation -> growable static; dot_count reset at summary (-r parity); C mock bool normalization (!= 0 at all boundaries, upstream MockSupport_c.cpp); SIGNED_BYTES_EQUAL_LOCATION arity (upstream has 4-param + _TEXT_LOCATION); -s negative seed rejected via AtoU port; realloc OOM keeps old block valid+tracked (reinsert_node); -jN fork/stats failures now fail the run; stale cur_actual/cur_expectation nulled on check/clear; cu_x* checked allocators for framework internals (NOT tracked paths — OOM simulation must keep returning NULL); JUnit print O(n²) -> cached len; Makefile -MMD header deps (touch internal.h rebuilds 10 objs) + rm-before-ar; NEW make lint = gcc -fanalyzer gate (20 warnings -> 0 after cu_x sweep) in check.sh+CI; warnings raised (+probed gcc-only set via try-cflag so clang legs survive); scripts honor CC/CXX (clang CI legs were testing gcc binaries); bench date +%s%N macOS fallback + upstream-cache now includes Ext (poisoning); README badge \! fixed; "POSIX make" claim corrected to GNU make. README badge bug: literal backslash-bang broke image rendering. check.sh ALL GREEN incl. differential fuzz. CI fix round: clang-format 18 (cached static binary at /tmp/claude-1000/tools) on new code; clang -Wformat-nonliteral does NOT exempt va_list fns like gcc -> CU_FORMAT_PRINTF attributes on the variadic helpers; clang -Wcast-align fires on x86 unlike gcc -> void* hop in memleak block-header cast; LESSON: zig cc/c++ reproduces the clang CI legs locally under the full warning set. CI round 2: Darwin ASan aborts on detect_leaks=1 (LSan Linux-only; old script ignored rc so it never noticed) -> leaks forced 0 on Darwin; clang ASan poisons new[] array cookies, our 0xCD fill of the whole tracked block in operator delete[] trips it (gcc has no cookie poisoning; zig clang-20 didn't repro either) -> ASAN_OPTIONS poison_array_cookie=0 in both gate scripts.
- 2026-06-10 #28 (SHIPPED): pushed master + all tags to github.com/Bud-ro/cpputest-turbo; v1.0.0 GitHub release published; CI green across all 9 jobs after 3 rounds of fixes (CI branch filter main->master; .upstream-cache relative paths broke on cold build — stale local cache had masked it; deliberate-SIGSEGV test can't run under sanitizers — UBSan null-store on Linux passed my ERROR-grep by luck, ASan DEADLYSIGNAL failed on Darwin -> exclude via -xg Hazard; BSD grep has no -P). First REAL macOS runs (gcc+clang) green — cross-compile-only validation held up. Docs: CONTRIBUTING.md, CI badge.
- 2026-06-10 #27 (release prep): clang-format 18 (static binary via GitHub) applied repo-wide w/ .clang-format/.editorconfig + check-format.sh in check.sh and CI (tests/ exempt: goldens embed line numbers); REQ flags maxed (-Wconversion -Wsign-conversion etc., codebase already clean); FUZZ_OPT matrix -O0..-O3 green locally + in CI; cpputest_core demoted to internal (banner + README); RENAMED to cpputest-turbo (user pick); pc 1.0.0; tagged v1.0.0.
- 2026-06-10 #26 (publish blockers, "do it all"): LICENSE+attribution; Makefile REQ_* split + install/pkg-config; SimpleString free operators + full utility surface (differential suite); CPPUTEST_MEM_LEAK_DETECTION_DISABLED; expectNoCall void; onObject (core+facade+both failure messages, fuzz alphabet widened, identical first try); withName/withCallOrder; MockSupportPlugin comparator pipeline + hasFailed gate (differential plugin suite); tracing (upstream-exact format, differential); crashOnFailure report-then-trap; mock_fail 8KiB truncation fixed (parked heap slot); check.sh gates sanitizers+bounded fuzz; sweep covers leaks/fork/-j2/C-consumers; memleak fuzzer adds double-free; CHANGELOG; README requirements/disclaimer; CI workflow. Deferred (documented): MockFailureReporter injection surface.
- 2026-06-09 #25 (5x Opus review + widened fuzzing): widened differential alphabet (comparators/copiers/OfType/membuffer/typed-return-asks/scope-clear) caught 4 more parity bugs (OfType output rendering in dumps+MISSING list; child-scope clear must not wipe globally; known-name wrong-type output params not rescued by ignoreOtherParameters; integer return getters need upstream's widening coercion in BOTH C++ facade and C API). Review-driven safety fixes: heap corruption freeing tracked ptrs after tracking toggled off (lookup-based release), size-arithmetic overflow guard in block_alloc, mkdtemp per-run worker dirs. Review verdict logged in next section: NOT yet publish-ready — blockers: LICENSE/attribution, Makefile CFLAGS, SimpleString free operator==, mock onObject/tracing/MockSupportPlugin, CPPUTEST_MEM_LEAK_DETECTION_DISABLED.
- 2026-06-09 #24: fuzzer found a REAL output bug — emit() truncated failure messages >1KiB (fixed-size stack buffer; long mock histories hit it). Fixed with heap spill + fixture regression test. 400-round soak (20k random mock sequences) now byte-identical to upstream; torture suite identical; sanitizers green.
- 2026-06-09 #23 (fuzzing + mock torture): built seeded ASan/UBSan fuzzers (args parser, failure formatters, leak tracker) and a DIFFERENTIAL mock fuzzer (same public-API driver vs upstream, outputs diffed). Found and fixed 8 real divergences: duplicate-param-name first-by-name rendering/lookup quirks; actual output param type label void*; ignoreOtherCalls/disable/enable propagate global->children; lazily-created scopes clone global state; global clear DELETES children (zombie+revive model) and discards pending actual call UNCHECKED; mock(name) on existing scope counts a check (upstream internal STRCMP assert); output-param copy counts a check per param; C-API typed return getters count+fail on type mismatch like upstream getters. Also: tests/leaks/run.sh golden check had been vacuous (mangled bang) — fixed, goldens verified genuine. Our mock suite now runs byte-identical ON UPSTREAM. mock torture suite added (15 adversarial scenarios, differential).
- 2026-06-09 #22 (post-v0.1.0 perf rounds): napkin math said 25-30% was a miss -> profiled (asserts 2.7ns, churn 14ns/pair vs floors 0.2ns/2ns). Round 1: inline assertion fast path (passing path = counter+compare in user TU, failure-only lib entries; LIFETIME TRAP: capturing char*/void* args into locals kills temporaries (getType().asCharString()) — string/mem compares must run inside the same full expression via inline check fns; caught by mock golden). Round 2: leak tracker embeds node in block (1 malloc not 2) + capped size-class freelists. Results: asserts 10x (0.3ns), churn 2.2x (6.4ns/pair), sequential ~5x upstream, -j8 ~14x. -p8 is a parse error (-p takes no N; use -jN); old single-group bench couldn't parallelize — bench now 8 groups. All gates green (check, sanitizers, macOS).
- 2026-06-09 #21: 9.3+9.4 done — ALL PHASES COMPLETE. README written; ASan/UBSan sweep green (sole finding: longjmp-over-C++-temporaries on deliberate-failure paths, inherent to the no-exceptions design, documented); tagged v0.1.0.
- 2026-06-09 #20: 9.1+9.2 done — macOS cross-compile green both arches (zig cc); benchmarks: runtime 25-30% faster than upstream, C++ compile parity after moving SimpleString out-of-line (header cost is libstdc++ pre-includes both sides, required by the new-macro contract), C-only TUs are the big compile win. Remaining: 9.3 README, 9.4 final sweep (valgrind, tag).
- 2026-06-09 #19: Phase 8 done — fork.c: -p per-test fork (upstream-exact failure messages/waitpid loop) + -jN parallel extension (groups round-robin over forked workers, temp-file output replay in worker order = deterministic, merged summary, worker-death accounting; -j composes with -p for full containment). 7.2/7.3 closed at documented line. Phase 9 (portability/bench/docs) remains.
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
