# Conformance status of upstream test files

Files in `conformance/manifest.txt` compile UNMODIFIED against our headers
and pass. Everything else is listed here with the current blocker.
Categories:
- **PLANNED** — public-API test we intend to pass; blocker named.
- **INTERNALS** — tests upstream's internal classes that this rewrite
  deliberately does not reproduce (per the locked decisions in PLAN.md).
- **OUT-OF-SCOPE** — feature excluded from the project scope.

## tests/CppUTest

| File | Status | Reason |
|---|---|---|
| AllTests.cpp/.h | n/a | runner main; we supply our own |
| AllocLetTestFree*, AllocationIn* | n/a | support fixtures, pulled in only as needed |
| CheatSheetTest.cpp | **PASSING** | |
| CommandLineArgumentsTest.cpp | INTERNALS | constructs CommandLineArguments objects directly; arg behavior covered by tests/cli |
| CommandLineTestRunnerTest.cpp | INTERNALS | subclasses CommandLineTestRunner with fake platforms |
| CompatabilityTests.cpp | **PASSING** | |
| DummyMemoryLeakDetector* | n/a | support |
| JUnitOutputTest.cpp | INTERNALS | drives JUnitTestOutput class directly with mocked FILE ops; format covered by tests/outputs goldens |
| MemoryLeakDetectorTest.cpp | INTERNALS | MemoryLeakDetector class API; report formats covered by tests/leaks goldens |
| MemoryLeakWarningTest.cpp | PLANNED | needs DummyMemoryLeakDetector fixture + detector injection |
| MemoryOperatorOverloadTest.cpp | INTERNALS | operator-new overload mechanics + allocator internals |
| PluginTest.cpp | **PASSING** | |
| PreprocessorTest.cpp | **PASSING** | (compile-only constructs) |
| SetPluginTest.cpp | **PASSING** | |
| SimpleMutexTest.cpp | INTERNALS | SimpleMutex platform class |
| SimpleStringCacheTest.cpp | INTERNALS | string-cache allocator internals |
| SimpleStringTest.cpp | PLANNED | needs fuller SimpleString shim (split/replace/find/AtoI statics); formats already golden-tested |
| TeamCityOutputTest.cpp | INTERNALS | drives TeamCityTestOutput class directly; format covered by tests/outputs goldens |
| TestFailureNaNTest.cpp | PLANNED | needs CppUTest/TestOutput.h + TestFailure class surface |
| TestFailureTest.cpp | PLANNED | needs TestFailure class surface (constructs failure objects directly) |
| TestFilterTest.cpp | PLANNED | needs CppUTest/TestFilter.h shim (TestFilter class); semantics covered by tests/cli |
| TestHarness_cTest.cpp | PLANNED | needs TestOutput.h + fixture plumbing; C surface covered by tests/c_interface |
| TestInstallerTest.cpp | INTERNALS | installer/unDo internals |
| TestMemoryAllocatorTest.cpp | INTERNALS | TestMemoryAllocator class family |
| TestOutputTest.cpp | INTERNALS | TestOutput class virtuals with mocked time; formats covered by goldens |
| TestRegistryTest.cpp | INTERNALS | registry plumbing via mock registry |
| TestResultTest.cpp | INTERNALS | TestResult class with mocked output |
| TestUTestMacro.cpp | **PASSING** | |
| TestUTestStringMacro.cpp | **PASSING** | |
| UtestPlatformTest.cpp | INTERNALS | fork-mode platform behavior; covered by tests/process goldens |
| UtestTest.cpp | PLANNED | needs TestOutput.h + UtestShell surface additions |

## tests/CppUTestExt

| File | Status | Reason |
|---|---|---|
| AllTests.cpp | n/a | runner main |
| *MemoryReport* | OUT-OF-SCOPE | memory report plugin excluded by project scope |
| ExpectedFunctionsListTest.cpp | INTERNALS | MockExpectedCallsList internals |
| GMockTest.cpp, GTest*.cpp | OUT-OF-SCOPE | GTest convertor excluded |
| IEEE754PluginTest* | OUT-OF-SCOPE (for now) | IEEE754ExceptionsPlugin not shipped |
| MockActualCallTest.cpp | INTERNALS | MockCheckedActualCall direct construction |
| MockCallTest.cpp | PLANNED | needs CppUTestExt/MockFailure.h + failure-reporter injection (MockFailureReporterForTest) |
| MockCheatSheetTest.cpp | **PASSING** | |
| MockComparatorCopierTest.cpp | PLANNED | needs MockFailure.h reporter injection |
| MockExpectedCallTest.cpp | INTERNALS | MockCheckedExpectedCall direct construction |
| MockFailureReporterForTest.* | n/a | support, needed for the PLANNED mock tests |
| MockFailureTest.cpp | PLANNED | golden mock-failure strings; needs MockFailure class surface |
| MockFakeLongLong.cpp | INTERNALS | no-long-long fallback build |
| MockHierarchyTest.cpp | PLANNED | needs MockFailure.h reporter injection |
| MockNamedValueTest.cpp | INTERNALS | MockNamedValue internals |
| MockParameterTest.cpp | PLANNED | needs MockFailure.h reporter injection |
| MockPluginTest.cpp | PLANNED | needs TestOutput.h + MockFailure.h |
| MockReturnValueTest.cpp | PLANNED | needs MockFailure.h reporter injection |
| MockStrictOrderTest.cpp | PLANNED | needs MockFailure.h reporter injection |
| MockSupportTest.cpp | PLANNED | needs MockExpectedCall.h split header + reporter injection |
| MockSupport_cTest.cpp | PLANNED | needs reporter injection via mock_c |
| OrderedTestTest.cpp | PLANNED | needs TestOutput.h shim |
