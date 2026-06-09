# CppUTest Public Interface Contract

Spec for a source-compatible, byte-for-byte output-compatible reimplementation of CppUTest
(C11 core + thin C++ header shim). Mined from the vendored sources at
`third_party/cpputest` (CMake project version 4.0, post-4.0 master). All file:line
references below are relative to `third_party/cpputest/`.

Conventions: quoted strings are **exact** (including spaces, tabs `\t`, newlines `\n`).
Format specifiers are printf-style as they appear in the source.

---

## 1. Test declaration macros (include/CppUTest/UtestMacros.h, Utest.h)

### TEST_GROUP / TEST_GROUP_BASE / TEST_BASE (UtestMacros.h:41-50)

```c
#define TEST_GROUP_BASE(testGroup, baseclass) \
  extern int externTestGroup##testGroup; \
  int externTestGroup##testGroup = 0; \
  struct TEST_GROUP_##CppUTestGroup##testGroup : public baseclass

#define TEST_BASE(testBaseClass) \
  struct testBaseClass : public Utest

#define TEST_GROUP(testGroup) TEST_GROUP_BASE(testGroup, Utest)
```

The group struct name is the token-paste `TEST_GROUP_CppUTestGroup<testGroup>`
(e.g. `TEST_GROUP(Foo)` → `struct TEST_GROUP_CppUTestGroupFoo : public Utest`).
The `externTestGroup<testGroup>` int (defined = 0) exists solely so
`IMPORT_TEST_GROUP` can force-link the translation unit.

### TEST_SETUP / TEST_TEARDOWN (UtestMacros.h:52-56)

```c
#define TEST_SETUP()    virtual void setup() CPPUTEST_OVERRIDE
#define TEST_TEARDOWN() virtual void teardown() CPPUTEST_OVERRIDE
```

### TEST (UtestMacros.h:58-70) — exact generated names

`TEST(testGroup, testName)` generates:

- class `TEST_<group>_<name>_Test : public TEST_GROUP_CppUTestGroup<group>` with
  `void testBody() CPPUTEST_OVERRIDE;` (the macro body becomes its definition).
- class `TEST_<group>_<name>_TestShell : public UtestShell` overriding
  `virtual Utest* createTest()` to `return new TEST_<group>_<name>_Test;`
- global instance `TEST_<group>_<name>_TestShell_instance` (with a preceding
  `class ...; extern ...;` forward declaration pair "for strict compilers").
- `static TestInstaller TEST_<group>_<name>_Installer(TEST_<group>_<name>_TestShell_instance, #testGroup, #testName, __FILE__, __LINE__);`

### IGNORE_TEST (UtestMacros.h:72-84)

Same shape but names are `IGNORE<group>_<name>_Test` and
`IGNORE<group>_<name>_TestShell` (**no underscore between `IGNORE` and group**),
shell derives from `IgnoredUtestShell`. Quirk: the installer variable is named
`TEST_<group><name>_Installer` — `TEST_##testGroup##testName##_Installer`, i.e. **no
underscore between group and name** (UtestMacros.h:83).

### IMPORT_TEST_GROUP (UtestMacros.h:86-89)

```c
#define IMPORT_TEST_GROUP(testGroup) \
  extern int externTestGroup##testGroup;\
  extern int* p##testGroup; \
  int* p##testGroup = &externTestGroup##testGroup
```

### CPPUTEST_DEFAULT_MAIN (UtestMacros.h:91-96)

Expands to `int main(int argc, char** argv) { return CommandLineTestRunner::RunAllTests(argc, argv); }`.

### Static auto-registration: TestInstaller (Utest.h:321-335, Utest.cpp:970-986)

```cpp
TestInstaller::TestInstaller(UtestShell& shell, const char* groupName, const char* testName,
                             const char* fileName, size_t lineNumber)
{
    shell.setGroupName(groupName); shell.setTestName(testName);
    shell.setFileName(fileName);   shell.setLineNumber(lineNumber);
    TestRegistry::getCurrentRegistry()->addTest(&shell);
}
void TestInstaller::unDo() { TestRegistry::getCurrentRegistry()->unDoLastAddTest(); }
```

`TestRegistry::addTest` **prepends**: `tests_ = test->addTest(tests_)`
(TestRegistry.cpp:41-44), and `UtestShell::addTest` sets `next_` and returns `this`
(Utest.cpp:259-263). Net effect: tests run in *reverse* static-initialization order
within a registry (which, with typical link order, runs them in reverse declaration
order per file). `unDoLastAddTest` pops the head (TestRegistry.cpp:169-173).

### Test execution model

- `UtestShell` is the persistent registered object; the `Utest` (test fixture) is
  created fresh per run via `createTest()` and destroyed via `destroyTest()`
  (Utest.cpp:207-252).
- `Utest::run()` (Utest.cpp:654-732): `setup()` under setjmp; if setup didn't longjmp,
  run `testBody()` under setjmp; always run `teardown()` under setjmp. With exceptions:
  `CppUTestFailedException` is caught and swallowed (jump buffer restored);
  `std::exception` / `...` add `UnexpectedExceptionFailure` and rethrow iff
  `UtestShell::isRethrowingExceptions()`.
- `IgnoredUtestShell::runOneTest` only does `result.countIgnored()` unless
  `setRunIgnored()` was called (Utest.cpp:872-886). `getMacroName()` returns
  `"IGNORE_TEST"` (or `"TEST"` when running ignored) vs base `"TEST"` (Utest.cpp:270-273, 865-870).
- `UtestShell::getFormattedName()` = `"<macroName>(<group>, <name>)"` (Utest.cpp:285-295).
- Outside a test run, `UtestShell::getCurrent()` returns a singleton whose group/name are
  `"\n\t NOTE: Assertion happened without being in a test run (perhaps in main?)"` /
  `"\n\t       Something is very wrong. Check this assertion and fix"`, file
  `"unknown file"` line 0 (Utest.cpp:54-79).

---

## 2. Assertion macros and exact failure-message formats

### Complete macro list (UtestMacros.h)

| Macro | Dispatches to (UtestShell method) |
|---|---|
| `CHECK(cond)` / `CHECK_TEXT(cond,text)` | `assertTrue(cond,"CHECK",#cond,...)` |
| `CHECK_TRUE(cond)` / `CHECK_TRUE_TEXT` | `assertTrue(cond,"CHECK_TRUE",#cond,...)` |
| `CHECK_FALSE(cond)` / `CHECK_FALSE_TEXT` | `assertTrue(!(cond),"CHECK_FALSE",#cond,...)` |
| `CHECK_EQUAL(e,a)` / `CHECK_EQUAL_TEXT` | `assertEquals(true, StringFrom(e), StringFrom(a),...)` only when `(e)!=(a)`; else `assertLongsEqual(0,0,NULL,...)` to count the check |
| `CHECK_EQUAL_ZERO(a)` / `_TEXT` | `CHECK_EQUAL(0,(a))` |
| `CHECK_COMPARE(f,relop,s)` / `_TEXT` | `assertCompare(false,"CHECK_COMPARE", "<f> relop <s>",...)` on failure |
| `STRCMP_EQUAL` / `_TEXT` | `assertCstrEqual` |
| `STRNCMP_EQUAL` / `_TEXT` | `assertCstrNEqual` |
| `STRCMP_NOCASE_EQUAL` / `_TEXT` | `assertCstrNoCaseEqual` |
| `STRCMP_CONTAINS` / `_TEXT` | `assertCstrContains` |
| `STRCMP_NOCASE_CONTAINS` / `_TEXT` | `assertCstrNoCaseContains` |
| `LONGS_EQUAL(e,a)` | `assertLongsEqual` with default text `"LONGS_EQUAL(" #e ", " #a ") failed"` (UtestMacros.h:216) |
| `LONGS_EQUAL_TEXT` | `assertLongsEqual` with user text |
| `UNSIGNED_LONGS_EQUAL` / `_TEXT` | `assertUnsignedLongsEqual` |
| `LONGLONGS_EQUAL` / `_TEXT` (if `CPPUTEST_USE_LONG_LONG`) | `assertLongLongsEqual` |
| `UNSIGNED_LONGLONGS_EQUAL` / `_TEXT` | `assertUnsignedLongLongsEqual` |
| `BYTES_EQUAL(e,a)` / `_TEXT` | `LONGS_EQUAL((e)&0xff,(a)&0xff)` |
| `SIGNED_BYTES_EQUAL` / `_TEXT` | `assertSignedBytesEqual` |
| `POINTERS_EQUAL` / `_TEXT` | `assertPointersEqual` |
| `FUNCTIONPOINTERS_EQUAL` / `_TEXT` | `assertFunctionPointersEqual` |
| `DOUBLES_EQUAL(e,a,threshold)` / `_TEXT` | `assertDoublesEqual` |
| `MEMCMP_EQUAL(e,a,size)` / `_TEXT` | `assertBinaryEqual` |
| `BITS_EQUAL(e,a,mask)` / `_TEXT` | `assertBitsEqual(e,a,mask,sizeof(actual),...)` |
| `ENUMS_EQUAL_INT` / `_TEXT` | `ENUMS_EQUAL_TYPE(int,...)` |
| `ENUMS_EQUAL_TYPE(T,e,a)` / `_TEXT` | casts both to T; `assertEquals(true, StringFrom(e_T), StringFrom(a_T),...)` on mismatch, else `assertLongsEqual(0,0,...)` |
| `FAIL(text)` / `FAIL_TEST(text)` | `fail(text, file, line)` |
| `TEST_EXIT` | `UtestShell::getCurrent()->exitTest()` |
| `UT_PRINT(text)` / `UT_PRINT_LOCATION` | `print(text, file, line)` |
| `CHECK_THROWS(expected, expr)` (if `CPPUTEST_HAVE_EXCEPTIONS`) | catches; fails with `"expected to throw "#expected "\nbut threw nothing"` or `"expected to throw " #expected "\nbut threw a different type"`; on success `countCheck()` (UtestMacros.h:366-385) |
| `UT_CRASH()` | `UtestShell::crash()` |
| `RUN_ALL_TESTS(ac,av)` | `CommandLineTestRunner::RunAllTests(ac, av)` |

Every macro has a `*_LOCATION` variant taking explicit `text, file, line` (all listed in
UtestMacros.h). Quirks:

- `CHECK_EQUAL_LOCATION` (UtestMacros.h:132-143) evaluates `expected`/`actual` multiple
  times; if `(actual)!=(actual)` it prints (via `UtestShell::print`):
  `"WARNING:\n\tThe \"Actual Parameter\" parameter is evaluated multiple times resulting in different values.\n\tThus the value in the error message is probably incorrect."`
  and analogously for `"Expected Parameter"`.
- `CHECK_COMPARE_LOCATION` (UtestMacros.h:155-165) ignores its `file`/`line` parameters
  and uses `__FILE__, __LINE__` directly (upstream bug, must be reproduced for
  source-compat; only observable via `_LOCATION` use).
- Comparison string for `CHECK_COMPARE` is built as
  `StringFrom(first) + " " + #relop + " " + StringFrom(second)`.

### Shared failure message helpers (src/CppUTest/TestFailure.cpp)

- `createButWasString` (TestFailure.cpp:112-115):
  `"expected <%s>\n\tbut was  <%s>"` — note **two spaces** after "was".
- `createDifferenceAtPosString` (TestFailure.cpp:117-133): given actual string, byte
  offset (into printable string) and reported position. Window of 20 chars
  (10 padding spaces added on both sides of actual to avoid OOB):

  ```
  "\n" +
  "\t" + "difference starts at position %lu at: <" + <20-char substring starting at offset> + ">\n" +
  "\t" + <spaces of length(differentString) + 10> + "^"
  ```

  where `differentString = StringFromFormat("difference starts at position %lu at: <", (unsigned long) reportedPosition)`.
  The caret line is `"\t%s^"` with `%s` = `strlen(differentString) + 10` spaces.
- `createUserText` (TestFailure.cpp:135-149): if text non-empty, prefix the message
  with `"Message: " + text + "\n\t"` — **except** when text starts with
  `"LONGS_EQUAL"`, in which case the `"Message: "` lead-in is omitted (kludge so the
  default `LONGS_EQUAL(a, b) failed` text appears bare).
- Failure default message when none given: `"no message"` (TestFailure.cpp:53).

### Per-failure-class formats (all prefixed by `createUserText(text)`)

- **EqualsFailure** (TestFailure.cpp:151-165) — POINTERS_EQUAL/FUNCTIONPOINTERS_EQUAL:
  `createButWasString(StringFromOrNull(expected), StringFromOrNull(actual))`. Null
  c-strings render as `"(null)"`.
- **DoublesEqualFailure** (TestFailure.cpp:167-179):
  `createButWasString(StringFrom(expected,7), StringFrom(actual,7))` +
  `" threshold used was <" + StringFrom(threshold,7) + ">"`; if any value is NaN,
  append `"\n\tCannot make comparisons with Nan"`. Doubles printed `"%.7g"`.
- **CheckEqualFailure** (TestFailure.cpp:181-198) — CHECK_EQUAL/ENUMS_EQUAL:
  printable-escaped expected/actual via `PrintableStringFromOrNull`, then
  `createButWasString(...)` + `createDifferenceAtPosString(printableActual,
  failStartPrintable, failStart)`. `failStart` = first differing index on the raw
  strings; `failStartPrintable` = first differing index on printable strings.
- **ComparisonFailure** (TestFailure.cpp:200-208) — CHECK_COMPARE:
  `checkString + "(" + comparisonString + ") failed"` →
  e.g. `CHECK_COMPARE(1 > 2) failed`.
- **ContainsFailure** (TestFailure.cpp:210-216) — STRCMP_CONTAINS:
  `"actual <%s>\n\tdid not contain  <%s>"` (two spaces after "contain").
- **CheckFailure** (TestFailure.cpp:218-227) — CHECK/CHECK_TRUE/CHECK_FALSE:
  `checkString + "(" + conditionString + ") failed"` → e.g. `CHECK(a == b) failed`.
- **FailFailure** (TestFailure.cpp:229-232) — FAIL: message is the text verbatim
  (no "Message:" wrapper).
- **LongsEqualFailure / UnsignedLongsEqualFailure / LongLongsEqualFailure /
  UnsignedLongLongsEqualFailure** (TestFailure.cpp:234-293): decimal strings of
  expected and actual are **left-padded with spaces to equal length**
  (`SimpleString::padStringsToSameLength`, SimpleString.cpp), then each is reported as
  `"<decimal> (0x<hex>)"`; the pair goes through `createButWasString`. Hex via
  `BracketsFormattedHexStringFrom` = `"(0x" + HexStringFrom(v) + ")"`;
  `HexStringFrom(unsigned long)` is `"%lx"` (lowercase, no padding),
  long long `"%llx"`. Example: `expected <  1 (0x1)>\n\tbut was  <100 (0x64)>`.
- **SignedBytesEqualFailure** (TestFailure.cpp:295-308): decimals from `(int)` cast,
  padded; hex via `HexStringFrom(signed char)`: `"%x"` then, if value < 0, the result is
  truncated to the **last 2 hex digits** (SimpleString.cpp:721-729, e.g. -1 → `ff`).
- **StringEqualFailure** (TestFailure.cpp:310-329) — STRCMP_EQUAL/STRNCMP_EQUAL:
  printable expected/actual via `PrintableStringFromOrNull` then `createButWasString`;
  if both non-null also `createDifferenceAtPosString` as in CheckEqualFailure.
- **StringEqualNoCaseFailure** (TestFailure.cpp:331-352): same but difference scan is
  case-insensitive (`SimpleString::ToLower`).
- **BinaryEqualFailure** (TestFailure.cpp:354-370) — MEMCMP_EQUAL: expected/actual via
  `StringFromBinaryOrNull` (hex bytes `"%02X "` joined, trailing space stripped —
  uppercase); if both non-null, `createDifferenceAtPosString(actualHex,
  failStart*3 + 1, failStart)` (offset accounts for 3 chars per byte; reported
  position is the byte index).
- **BitsEqualFailure** (TestFailure.cpp:372-379) — BITS_EQUAL:
  `createButWasString(StringFromMaskedBits(expected,mask,byteCount), StringFromMaskedBits(actual,mask,byteCount))`.
  `StringFromMaskedBits` (SimpleString.cpp:1007-1030): MSB-first, `1`/`0` where mask bit
  set, `x` where not, space every 8 bits — e.g. `xxxxxxxx 00000001`. Bit count =
  min(byteCount, sizeof(unsigned long)) * 8.
- **FeatureUnsupportedFailure** (TestFailure.cpp:381-388):
  `"The feature \"%s\" is not supported in this environment or with the feature set selected when building the library."`
  (used by LONGLONGS_EQUAL when `CPPUTEST_USE_LONG_LONG` is off, featureName
  `"CPPUTEST_USE_LONG_LONG"`).
- **UnexpectedExceptionFailure** (TestFailure.cpp:390-432):
  `"Unexpected exception of unknown type was thrown."` or, with RTTI,
  `"Unexpected exception of type '%s' was thrown: %s"` (demangled type name via
  `abi::__cxa_demangle` on GCC, `e.what()`).

### Printable string escaping (SimpleString.cpp:433-495)

`SimpleString::printable()`: chars `\a..\r` (0x07–0x0D) become 2-char short escapes
`\a \b \t \n \v \f \r`; other control chars (< 0x20 or 0x7F) become 4 chars
`\xNN ` — uppercase hex **followed by a space** (format `"\\x%02X "`).
`PrintableStringFromOrNull(NULL)` → `"(null)"`.

### UtestShell assertion semantics (src/CppUTest/Utest.cpp:395-556)

Each `assert*` first calls `getTestResult()->countCheck()` and only fails (via
`failWith(failure, terminator)`) on mismatch. Null handling: `assertCstrEqual` etc.
pass when both NULL, fail when exactly one is NULL. `assertBinaryEqual` returns
success when `length == 0` or both NULL. `doubles_equal` (Utest.cpp:37-48): NaN in
any operand → false; both infinite → true; else `fabs(d1-d2) <= threshold`.

---

## 3. Default test output format (src/CppUTest/TestOutput.cpp, TestResult.cpp)

### Progress (normal mode)

- `printCurrentTestStarted` (TestOutput.cpp:97-107): sets progress indicator to `"."`
  if `test.willRun()`, else `"!"` (ignored tests).
- `printCurrentTestEnded` (TestOutput.cpp:109-119): in normal mode prints the
  indicator; after every 50 indicators prints `"\n"` (`++dotCount_ % 50 == 0`,
  TestOutput.cpp:121-125).

### Verbose mode (`-v`)

- At test start, prints `test.getFormattedName()` → `TEST(group, name)` (or
  `IGNORE_TEST(group, name)`). At test end prints `" - "` + ms + `" ms\n"`
  (TestOutput.cpp:109-115). Time is `TestResult::getCurrentTestTotalExecutionTime()`
  in integer milliseconds.

### Very verbose (`-vv`)

Additionally `printVeryVerbose` traces (only at `level_veryVerbose`) from
Utest.cpp/UtestShell::runOneTestInCurrentProcess:
`"\n-- before runAllPreTestAction: "`, `"\n-- after runAllPreTestAction: "`,
`"\n---- before createTest: "`, `"\n---- after createTest: "`,
`"\n------ before runTest: "`, `"\n------ after runTest: "`,
`"\n-------- before setup: "`, `"\n-------- after  setup: "` (two spaces),
`"\n----------  before body: "`, `"\n----------  after body: "`,
`"\n--------  before teardown: "`, `"\n--------  after teardown: "`,
`"\n---- before destroyTest: "`, `"\n---- after destroyTest: "`,
`"\n-- before runAllPostTestAction: "`, `"\n-- after runAllPostTestAction: "`
(Utest.cpp:209-251, 659-697).

### Failure block (TestOutput.cpp:203-266)

If failure is in the test's own file and not in a helper
(`!isOutsideTestFile() && !isInHelperFunction()`; helper = failure line < test line):

```
\n<file>:<line>: error: Failure in TEST(group, name)\n\t<message>\n\n
```

Otherwise (outside test file or helper function), two location lines:

```
\n<testFile>:<testLine>: error: Failure in TEST(group, name)\n<failFile>:<failLine>: error:\n\t<message>\n\n
```

Exact pieces: Eclipse format per location = `"\n" + file + ":" + line + ":" + " error:"`
(TestOutput.cpp:248-256); Visual Studio format = `"\n" + file + "(" + line + "):" + " error:"`
(TestOutput.cpp:258-266; chosen via `TestOutput::getWorkingEnvironment()`, default
detected per platform — Eclipse on non-Windows). Then `" Failure in "` + formatted test
name (TestOutput.cpp:226-230), then `"\n" + "\t" + message + "\n\n"`
(TestOutput.cpp:232-238).

### Final summary (TestOutput.cpp:144-190)

Always starts with `"\n"`. Success:

```
OK (%d tests, %d ran, %d checks, %d ignored, %d filtered out, %d ms)\n\n
```

Failure (`result.isFailure()`, defined as `failureCount != 0 || runCount + ignoredCount == 0`,
TestResult.h:89-91):

```
Errors (%d failures, %d tests, %d ran, %d checks, %d ignored, %d filtered out, %d ms)\n\n
```

If failing with `failureCount == 0` (nothing ran), the prefix is
`"Errors (ran nothing, "` instead of `"Errors (N failures, "`, and after the
closing `)` an extra line is printed:

```
\nNote: test run failed because no tests were run or ignored. Assuming something went wrong. This often happens because of linking errors or typos in test filter.
```

(then `"\n\n"`). All counts printed via `StringFrom(size_t)` = `"%lu"` /
`StringFrom(long)` = `"%ld"`; piecewise prints, not one printf.

### Color mode (`-c`)

Wraps the summary: failure prefix `"\033[31;1m"` (red bold), success prefix
`"\033[32;1m"` (green bold), reset `"\033[m"` printed after the closing `)` and before
the Note/newlines (TestOutput.cpp:150-182).

### Repeat runs (TestOutput.cpp:192-201)

When `-r` total > 1, before each run: `"Test run <n> of <total>\n"`.

### Shuffle notice (CommandLineTestRunner.cpp:134-139)

`"Test order shuffling enabled with seed: "` + seed + `"\n"` (printed once before loop).

### UT_PRINT output (Utest.cpp:558-567)

`"\n" + file + ":" + line + " " + text` (no "error:").

### TestRegistry group lifecycle

`TestResult::currentGroupStarted/Ended`, `currentTestStarted/Ended` drive
`TestOutput::printCurrentGroupStarted/...` (no-ops in console output) and measure times
via `GetPlatformSpecificTimeInMillis()` (TestResult.cpp:44-77).

---

## 4. CommandLineTestRunner / CommandLineArguments

### Flags (src/CppUTest/CommandLineArguments.cpp:54-101)

Parsed in this order (first match wins per argument):

| Flag | Syntax | Semantics |
|---|---|---|
| `-h` | exact | sets needHelp, parse returns false → help text printed, exit 1 |
| `-v` | exact | verbose |
| `-vv` | exact | very verbose |
| `-c` | exact | color output |
| `-p` | exact | run each test in separate process (fork) |
| `-b` | exact | reverse test order |
| `-lg` | exact | list group names, exit 0 |
| `-ln` | exact | list group.name pairs (filtered), exit 0 |
| `-ll` | exact | list `group.name.file.line` lines, exit 0 |
| `-ri` | exact | run ignored tests as normal |
| `-f` | exact | crash on fail (`UtestShell::setCrashOnFail()`) |
| `-e`, `-ci` | exact | do NOT rethrow unexpected exceptions (default rethrow=true) |
| `-r[<#>]` | prefix; `-r3` or `-r 3` | repeat count; bare `-r` or value 0 → 2 (CommandLineArguments.cpp:250-263) |
| `-g <group>` | prefix or next arg | substring group filter |
| `-t <g>.<n>` | prefix/next arg | group+name filter pair, split on `.`; must split into exactly 2 parts else parse error |
| `-st <g>.<n>` | | strict (exact) group+name |
| `-xt <g>.<n>` | | exclude group+name |
| `-xst <g>.<n>` | | exclude strict group+name |
| `-sg <group>` | | strict group filter |
| `-xg <group>` | | exclude group filter |
| `-xsg <group>` | | exclude strict group |
| `-n <name>` | | substring name filter |
| `-sn <name>` | | strict name filter |
| `-xn <name>` | | exclude name filter |
| `-xsn <name>` | | exclude strict name |
| `-s [<seed>]` | prefix/next arg | shuffle; seed defaults to time-in-millis (0 bumped to 1); explicit 0 via `-s0` → parse error (returns seed!=0) |
| `TEST(...)` / `IGNORE_TEST(...)` | whole arg starts with `TEST(` or `IGNORE_TEST(` | adds strict group+name filters parsed from `Group, Name)` form (copy/paste of `-v` output) |
| `-o<type>` | `-onormal`,`-oeclipse`,`-ojunit`,`-oteamcity` (or separated arg) | output type; unknown → parse error |
| `-p<...>` | prefix (only reachable with suffix, since exact `-p` matched earlier) | forwarded to `plugin->parseAllArguments` |
| `-k <packageName>` | prefix/next arg | JUnit package name |
| anything else | | parse error → usage printed, exit 1 |

Parameter-field convention (`getParameterField`, CommandLineArguments.cpp:287-294):
value may be glued (`-ggroup`) or the following argv entry.

`addGroupDotNameFilter` (CommandLineArguments.cpp:302-326): splits on `"."`
(split keeps delimiter; group part strips its trailing char), creates one group filter
and one name filter with strict/invert flags as appropriate.

TestFilter semantics (TestFilter.cpp:56-75): default substring match (`contains`);
`strictMatching` → equality; `invertMatching` → negate. Filters are OR-combined per
dimension; group and name dimensions AND-combined (Utest.cpp:358-371).
`TestFilter::asString()` = `"TestFilter: \"%s\""` plus optional
`" with strict matching"` / `" with invert matching"` / `" with strict, invert matching"`.

### usage / help text (CommandLineArguments.cpp:103-162) — exact strings

`usage()`:

```
use -h for more extensive help
usage [-h] [-v] [-vv] [-c] [-p] [-lg] [-ln] [-ll] [-ri] [-r[<#>]] [-f] [-e] [-ci]
      [-g|sg|xg|xsg <groupName>]... [-n|sn|xn|xsn <testName>]... [-t|st|xt|xst <groupName>.<testName>]...
      [-b] [-s [<seed>]] ["[IGNORE_]TEST(<groupName>, <testName>)"]...
      [-o{normal|eclipse|junit|teamcity}] [-k <packageName>]
```

`help()` is the full multi-line text at CommandLineArguments.cpp:112-161 beginning
`"Thanks for using CppUTest.\n"` — reproduce verbatim (sections: "Options that do not
run tests but query:", "Options that change the output format:", "Options that change
the output location:", "Options that control which tests are run:", "Options that
control how the tests are run:").

### Runner & exit code (src/CppUTest/CommandLineTestRunner.cpp)

`RunAllTests(ac, av)` (CommandLineTestRunner.cpp:40-59):

1. Creates `MemoryLeakWarningPlugin` named `"MemoryLeakPlugin"`
   (`DEF_PLUGIN_MEM_LEAK`, CommandLineTestRunner.h:38), with
   `destroyGlobalDetectorAndTurnOffMemoryLeakDetectionInDestructor(true)`, installs it.
2. Runs `runAllTestsMain()`; that installs a `SetPointerPlugin` named
   `"SetPointerPlugin"` (`DEF_PLUGIN_SET_POINTER`) around the run.
3. If result == 0, prints `memLeakWarn.FinalReport(0)` to a fresh `ConsoleTestOutput`.
4. Removes the plugin, returns result.

`runAllTestsMain` returns **1** if argument parsing failed (after printing help or
usage). `runAllTests()` (CommandLineTestRunner.cpp:102-154): list options return 0;
otherwise loops `repeatCount` times, accumulating
`failedTestCount += tr.getFailureCount()` and `failedExecutionCount++` when
`tr.isFailure()`; returns `(int)(failedTestCount != 0 ? failedTestCount : failedExecutionCount)`.
So exit code = total failure count, or number of failed runs when zero failures but
nothing ran.

Output selection (CommandLineTestRunner.cpp:183-200): junit → `JUnitTestOutput`
(+ composite with console if `-v`/`-vv`); teamcity → `TeamCityTestOutput`; else
`ConsoleTestOutput`.

List formats (TestRegistry.cpp:76-144): `-lg` prints unique group names joined by
single spaces (no trailing space, no newline). `-ln` prints unique `group.name` joined
by spaces (only tests passing filters). `-ll` prints one `group.name.file.line\n`
per test (line via `"%d\n"`).

---

## 5. JUnit XML output (src/CppUTest/JUnitTestOutput.cpp)

- **One file per test group**, written when the group ends
  (`printCurrentGroupEnded` → `writeTestGroupToFile`, JUnitTestOutput.cpp:121-126, 268-277).
- **File name** (JUnitTestOutput.cpp:150-171): `cpputest_` + (`<package>_` if package
  set) + `<group>` with characters `/\?%*:|"<>` each replaced by `_`, then `.xml`.
- **Structure**, in order:
  1. `<?xml version="1.0" encoding="UTF-8" ?>\n`
  2. `<testsuite errors="0" failures="%d" hostname="localhost" name="%s" tests="%d" time="%d.%03d" timestamp="%s">\n`
     — name = group, time = group execution ms as seconds.millis, timestamp from
     `GetPlatformSpecificTimeString()` (`strftime "%Y-%m-%dT%H:%M:%S"` localtime,
     src/Platforms/Gcc/UtestPlatform.cpp:212-225). `errors` is always `"0"`.
  3. `<properties>\n</properties>\n`
  4. Per test case:
     `<testcase classname="%s%s%s" name="%s" assertions="%d" time="%d.%03d" file="%s" line="%d">\n`
     where classname = `package` + (`.` iff package non-empty) + group; assertions =
     this test's cumulative check count minus the previous test's (delta,
     JUnitTestOutput.cpp:222-234); then, if failed,
     `<failure message="%s:%d: %s" type="AssertionFailedError">\n</failure>\n`
     (message = failure file, line, and the **XML-encoded** failure message — file
     name itself is not encoded); else if ignored `<skipped />\n`; then
     `</testcase>\n`.
  5. `<system-out>` + encoded accumulated `print(const char*)` text + `</system-out>\n`
  6. `<system-err></system-err>\n`
  7. `</testsuite>\n`
- **Escaping** (`encodeXmlText`, JUnitTestOutput.cpp:205-215), in this order:
  `&`→`&amp;`, `"`→`&quot;`, `<`→`&lt;`, `>`→`&gt;`, `\r`→`&#13;`, `\n`→`&#10;`.
- Only the **first** failure per test is recorded (JUnitTestOutput.cpp:304-310);
  group failure count increments once per failed test.
- `setPackageName` only used for file name + classname; package set via `-k`.

---

## 6. TeamCity output (src/CppUTest/TeamCityTestOutput.cpp)

Messages (each ends `\n`):

- Group start: `##teamcity[testSuiteStarted name='<group-escaped>']`
- Test start: `##teamcity[testStarted name='<name-escaped>']`; if the test won't run,
  immediately followed by `##teamcity[testIgnored name='<name-escaped>']`
- Test end: `##teamcity[testFinished name='<name-escaped>' duration='<ms>']`
- Group end: `##teamcity[testSuiteFinished name='<group-escaped>']` (skipped if no
  group started)
- Failure (TeamCityTestOutput.cpp:80-100):
  `##teamcity[testFailed name='<testNameOnly-escaped>' message='`
  then if outside test file / helper function: `TEST failed (` + testFile (**unescaped**)
  + `:` + testLine + `): `; then failure file **escaped** + `:` + line; then
  `' details='` + escaped failure message + `']\n`.

Escaping (`printEscaped`, TeamCityTestOutput.cpp:55-78): `'`, `|`, `[`, `]` are
prefixed with `|`; `\r` → `|r`; `\n` → `|n`; all other chars verbatim.

---

## 7. Memory leak detection — user-facing surface

### MemoryLeakWarningPlugin API (include/CppUTest/MemoryLeakWarningPlugin.h)

```cpp
MemoryLeakWarningPlugin(const SimpleString& name, MemoryLeakDetector* localDetector = NULLPTR);
void preTestAction(UtestShell&, TestResult&);   // startChecking, snapshot failure count
void postTestAction(UtestShell&, TestResult&);  // stopChecking; add TestFailure if leaks
const char* FinalReport(size_t toBeDeletedLeaks = 0); // "" if leaks == toBeDeletedLeaks
void ignoreAllLeaksInTest();
void expectLeaksInTest(size_t n);
void destroyGlobalDetectorAndTurnOffMemoryLeakDetectionInDestructor(bool);
MemoryLeakDetector* getMemoryLeakDetector();
static MemoryLeakWarningPlugin* getFirstPlugin();
static MemoryLeakDetector* getGlobalDetector();
static MemoryLeakFailure* getGlobalFailureReporter();
static void setGlobalDetector(MemoryLeakDetector*, MemoryLeakFailure*);
static void destroyGlobalDetector();
static void turnOffNewDeleteOverloads();
static void turnOnDefaultNotThreadSafeNewDeleteOverloads();
static void turnOnThreadSafeNewDeleteOverloads();
static bool areNewDeleteOverloaded();
static void saveAndDisableNewDeleteOverloads();   // ref-counted (save_counter)
static void restoreNewDeleteOverloads();
```

Macros (MemoryLeakWarningPlugin.h:34-35):

```c
#define IGNORE_ALL_LEAKS_IN_TEST() if (MemoryLeakWarningPlugin::getFirstPlugin()) MemoryLeakWarningPlugin::getFirstPlugin()->ignoreAllLeaksInTest()
#define EXPECT_N_LEAKS(n)          if (MemoryLeakWarningPlugin::getFirstPlugin()) MemoryLeakWarningPlugin::getFirstPlugin()->expectLeaksInTest(n)
```

`postTestAction` (MemoryLeakWarningPlugin.cpp:640-656): fails only if
`!ignoreAllWarnings_ && expectedLeaks_ != leaks && failureCount unchanged`. If new/delete
overloads are disabled and `expectedLeaks_ > 0`, prints
`"Warning: Expected %d leak(s), but leak detection was disabled"` instead of failing.
Flags reset after each test. `expectLeaksInTest` requires the **exact** leak count.
Also: `void crash_on_allocation_number(unsigned alloc_number)` installs a crashing
allocator (MemoryLeakWarningPlugin.cpp:529-536).

### Leak report format (src/CppUTest/MemoryLeakDetector.cpp)

Report built into a fixed 4096-byte `SimpleStringBuffer` (`SIMPLE_STRING_BUFFER_LEN`,
MemoryLeakDetector.h:56). Per `MemoryLeakDetector::report(period)`:

- Header (first leak only): `"Memory leak(s) found.\n"` (MemoryLeakDetector.cpp:199-202)
- Per leak (MemoryLeakDetector.cpp:171-172):

  ```
  Alloc num (%u) Leak size: %lu Allocated at: %s and line: %d. Type: "%s"
  	Memory: <%p> Content:
  ```

  (format string: `"Alloc num (%u) Leak size: %lu Allocated at: %s and line: %d. Type: \"%s\"\n\tMemory: <%p> Content:\n"`)
  followed by a hex dump (`addMemoryDump`, MemoryLeakDetector.cpp:62-101): 16 bytes per
  line, `"    %04lx: "` offset prefix, bytes `"%02hx "` with an extra space after byte 8,
  3-space padding for missing bytes (plus 1 extra space if more than 8 missing), then
  `"|"` + ASCII (non-printables as `.`) + `"|\n"`.
- If the buffer write-limit was hit:
  `"\netc etc etc etc. !!!! Too many memory leaks to report. Bailing out\n"`
  (`MEM_LEAK_TOO_MUCH`, MemoryLeakDetector.cpp:126).
- Footer: `outputBuffer_.add("%s %d\n", MEM_LEAK_FOOTER, n)` with
  `MEM_LEAK_FOOTER "Total number of leaks: "` → note the resulting **double space**:
  `"Total number of leaks:  %d\n"` (MemoryLeakDetector.cpp:127, 209-212).
- If any leak's allocator name is `"malloc"`, append `MEM_LEAK_ADDITION_MALLOC_WARNING`
  (MemoryLeakDetector.cpp:128-131):

  ```
  NOTE:
  	Memory leak reports about malloc and free can be caused by allocating using the cpputest version of malloc,
  	but deallocate using the standard free.
  	If this is the case, check whether your malloc/free replacements are working (#define malloc cpputest_malloc etc).
  ```
- No leaks: `"No memory leaks were detected."` (MemoryLeakDetector.cpp:148-151).

Deallocation error reports (`reportFailure`, MemoryLeakDetector.cpp:219-241), message
first line one of `"Deallocating non-allocated memory\n"`,
`"Allocation/deallocation type mismatch\n"`,
`"Memory corruption (written out of bounds?)\n"`, then:

```
   allocated at file: %s line: %d size: %lu type: %s
   deallocated at file: %s line: %d type: %s
```

(three leading spaces each). These fail the current test immediately via
`MemoryLeakFailure::fail` → `FailFailure` with the current test's name as "file" and
its line number (MemoryLeakWarningPlugin.cpp:538-550). Unknown locations render file
`"<unknown>"` line 0. Allocator type names: `"new"`, `"new []"`, `"malloc"`, etc.
(TestMemoryAllocator). Guard bytes are `'B','A','S'` repeating, 3 bytes
(`memory_corruption_buffer_size = 3`); freed memory is poisoned with `0xCD`.

### Redefinition mechanism

- **C++** (include/CppUTest/MemoryLeakDetectorNewMacros.h): declares global
  `operator new/new[]/delete/delete[]` overloads incl. `(size_t, const char*, int)` and
  `(size_t, const char*, size_t)` placement-debug forms, then
  `#define new new(__FILE__, __LINE__)` (line 97) unless `CPPUTEST_USE_NEW_MACROS == 0`.
  The actual operators (defined in MemoryLeakWarningPlugin.cpp:315-409) route through
  swappable function pointers (`operator_new_fptr` etc.) so detection can be turned
  on/off/thread-safe at runtime.
- **C** (include/CppUTest/MemoryLeakDetectorMallocMacros.h):
  `#define malloc(a) cpputest_malloc_location(a, __FILE__, __LINE__)` and likewise
  `calloc`, `realloc`, `free`; `strdup`/`strndup` if `CPPUTEST_HAVE_STRDUP`. Guarded by
  `CPPUTEST_USE_MALLOC_MACROS` / `CPPUTEST_USE_STRDUP_MACROS` include-once defines.
  The `cpputest_*_location` functions route through `malloc_fptr`/`free_fptr`/
  `realloc_fptr` (MemoryLeakWarningPlugin.cpp:101-126), which point at
  `mem_leak_*` (default), `threadsafe_mem_leak_*`, or `normal_*` versions.
- Both headers are pulled in by `TestHarness.h`; malloc allocations are tracked with
  node-allocated-separately mode (`allocatNodesSeperately = true`) so a foreign `free`
  is detectable.

---

## 8. TestHarness_c.h — C interface

### Check macros (each also has `_TEXT` variant; `_LOCATION` functions exported)

`CHECK_EQUAL_C_BOOL`, `CHECK_EQUAL_C_INT`, `CHECK_EQUAL_C_UINT`,
`CHECK_EQUAL_C_LONG`, `CHECK_EQUAL_C_ULONG`, `CHECK_EQUAL_C_LONGLONG`,
`CHECK_EQUAL_C_ULONGLONG`, `CHECK_EQUAL_C_REAL` (expected, actual, threshold),
`CHECK_EQUAL_C_CHAR`, `CHECK_EQUAL_C_UBYTE`, `CHECK_EQUAL_C_SBYTE`,
`CHECK_EQUAL_C_STRING`, `CHECK_EQUAL_C_POINTER`, `CHECK_EQUAL_C_MEMCMP`
(expected, actual, size), `CHECK_EQUAL_C_BITS` (expected, actual, mask — size is
`sizeof(actual)`), `FAIL_TEXT_C(text)`, `FAIL_C()`, `CHECK_C(condition)`,
`CHECK_C_TEXT(condition, text)` (TestHarness_c.h:39-139).

Implementations (src/CppUTest/TestHarness_c.cpp:37-125) forward to the corresponding
`UtestShell::getCurrent()->assert*` using
`UtestShell::getCurrentTestTerminatorWithoutExceptions()` (longjmp, not exceptions).
Notable mappings: BOOL → `assertEquals(!!e != !!a, e?"true":"false", a?"true":"false",...)`;
INT/LONG → `assertLongsEqual`; CHAR → `assertEquals` with `StringFrom(char)` (`"%c"`);
UBYTE/SBYTE → `assertEquals` with `StringFrom((int)v)`; `FAIL_C()` → `fail("")`;
`CHECK_C` → `assertTrue(cond != 0, "CHECK_C", #condition, ...)`.

### C test wrapper macros (TestHarness_c.h:148-192)

For use in C files:

```c
TEST_GROUP_C_SETUP(group)     /* defines void group_<group>_setup_wrapper_c(void) */
TEST_GROUP_C_TEARDOWN(group)  /* defines void group_<group>_teardown_wrapper_c(void) */
TEST_C(group, name)           /* defines void test_<group>_<name>_wrapper_c(void) */
IGNORE_TEST_C(group, name)    /* defines void ignore_<group>_<name>_wrapper_c(void) */
```

For use in the companion C++ file:

```c
TEST_GROUP_C_WRAPPER(group)          /* extern "C" decls of setup/teardown wrappers + TEST_GROUP(group) */
TEST_GROUP_C_SETUP_WRAPPER(group)    /* void setup() override calling the C wrapper */
TEST_GROUP_C_TEARDOWN_WRAPPER(group) /* same for teardown */
TEST_C_WRAPPER(group, name)          /* TEST(group, name) { test_<g>_<n>_wrapper_c(); } */
IGNORE_TEST_C_WRAPPER(group, name)   /* IGNORE_TEST(...) calling ignore wrapper */
```

### C allocation API (TestHarness_c.h:220-238)

```c
void* cpputest_malloc(size_t);  char* cpputest_strdup(const char*);
char* cpputest_strndup(const char*, size_t);  void* cpputest_calloc(size_t, size_t);
void* cpputest_realloc(void*, size_t);  void cpputest_free(void*);
/* _location variants taking (.., const char* file, size_t line) for each */
void cpputest_malloc_set_out_of_memory(void);
void cpputest_malloc_set_not_out_of_memory(void);
void cpputest_malloc_set_out_of_memory_countdown(int);
void cpputest_malloc_count_reset(void);
int  cpputest_malloc_get_count(void);
```

Plus `PUNUSED(x)` (`__attribute__((unused))` rename on GCC/clang, TestHarness_c.h:250-256).

---

## 9. TestPlugin API (include/CppUTest/TestPlugin.h)

```cpp
class TestPlugin {
public:
    TestPlugin(const SimpleString& name);
    virtual ~TestPlugin();
    virtual void preTestAction(UtestShell&, TestResult&) {}
    virtual void postTestAction(UtestShell&, TestResult&) {}
    virtual bool parseArguments(int ac, const char *const * av, int index) { return false; }
    virtual void runAllPreTestAction(UtestShell&, TestResult&);   // chains: this then next
    virtual void runAllPostTestAction(UtestShell&, TestResult&);  // chains: next then this (reverse)
    virtual bool parseAllArguments(int ac, const char *const *av, int index);  // also char** overload
    virtual TestPlugin* addPlugin(TestPlugin*);
    virtual TestPlugin* removePluginByName(const SimpleString& name);
    virtual TestPlugin* getNext();
    virtual void disable(); virtual void enable(); virtual bool isEnabled();
    const SimpleString& getName();
    TestPlugin* getPluginByName(const SimpleString& name);
};
```

Disabled plugins are skipped by the runAll* chain. `NullTestPlugin` terminates the
chain (TestPlugin.h:111-121).

### SetPointerPlugin (TestPlugin.h:89-107)

```cpp
extern void CppUTestStore(void **location);
class SetPointerPlugin : public TestPlugin {
    SetPointerPlugin(const SimpleString& name);
    virtual void postTestAction(UtestShell&, TestResult&) override; // restores all stored pointers
    enum { MAX_SET = 32 };
};
#define UT_PTR_SET(a, b) do { CppUTestStore((void**)&(a)); (a) = b; } while (0)
```

Records up to `MAX_SET` (32) pointer locations + original values per test; after each
test, restores them in reverse order. Exceeding 32 produces a FAIL
("...maximum number of UT_PTR_SET" — see TestPlugin.cpp). Installed automatically by
`CommandLineTestRunner` under the name `"SetPointerPlugin"`.

---

## 10. CppUMock public surface

### Entry point (include/CppUTestExt/MockSupport.h:40)

```cpp
MockSupport& mock(const SimpleString& mockName = "", MockFailureReporter* failureReporterForThisCall = NULLPTR);
```

Scope semantics: `mock("scope")` returns a child MockSupport stored in the global one
under the name `scope`; function names are recorded as `"scope::name"`
(`appendScopeToName`, MockSupport.cpp:152-156). Recursive operations
(`disable/enable/tracing/ignoreOtherCalls/checkExpectations/expectedCallsLeft/clear/crashOnFailure`)
apply to all scopes. Each `mock()` call resets the active reporter to the standard one
unless a reporter is passed.

### MockSupport methods (MockSupport.h:42-126)

`strictOrder()`, `expectOneCall(name)`, `expectNoCall(name)` (= `expectNCalls(0,name)`),
`expectNCalls(amount, name)`, `actualCall(name)`, `hasReturnValue()`, `returnValue()`,
and the typed return-value getters:
`boolReturnValue / intReturnValue / unsignedIntReturnValue / longIntReturnValue /
unsignedLongIntReturnValue / longLongIntReturnValue / unsignedLongLongIntReturnValue /
stringReturnValue / doubleReturnValue / pointerReturnValue / constPointerReturnValue /
functionPointerReturnValue` plus `return<Type>ValueOrDefault(default)` for each
(bool, int, unsigned int, long int, unsigned long int, long long, unsigned long long,
string, double, pointer, const pointer, function pointer).

Data store: `hasData(name)`, `setData(name, v)` overloads for bool, int, unsigned int,
long int, unsigned long int, const char*, double, void*, const void*, `void (*)()`;
`setDataObject(name, type, void*)`, `setDataConstObject(name, type, const void*)`,
`getData(name)` → `MockNamedValue`.

Control: `getMockSupportScope(name)`, `getTraceOutput()`, `disable()`, `enable()`,
`tracing(bool)`, `ignoreOtherCalls()`, `checkExpectations()`, `expectedCallsLeft()`,
`clear()`, `crashOnFailure(bool = true)`, `setMockFailureStandardReporter`,
`setActiveReporter`, `setDefaultComparatorsAndCopiersRepository`,
`installComparator(typeName, MockNamedValueComparator&)`,
`installCopier(typeName, MockNamedValueCopier&)`,
`installComparatorsAndCopiers(repository)`, `removeAllComparatorsAndCopiers()`.

### MockExpectedCall (include/CppUTestExt/MockExpectedCall.h:37-94)

Non-virtual `withParameter` overloads (forwarding): bool, int, unsigned int, long int,
unsigned long int, `cpputest_longlong`, `cpputest_ulonglong`, double,
(double, double tolerance), const char*, void*, const void*, `void (*)()`,
(const unsigned char*, size_t) → memory buffer. Pure virtuals:
`withName`, `withCallOrder(unsigned)`, `withCallOrder(unsigned, unsigned)`,
`withBoolParameter`, `withIntParameter`, `withUnsignedIntParameter`,
`withLongIntParameter`, `withUnsignedLongIntParameter`, `withLongLongIntParameter`,
`withUnsignedLongLongIntParameter`, `withDoubleParameter` (and tolerance overload),
`withStringParameter`, `withPointerParameter`, `withFunctionPointerParameter`,
`withConstPointerParameter`, `withMemoryBufferParameter`,
`withParameterOfType(typeName, name, const void*)`,
`withOutputParameterReturning(name, const void*, size_t)`,
`withOutputParameterOfTypeReturning(typeName, name, const void*)`,
`withUnmodifiedOutputParameter(name)`, `ignoreOtherParameters()`,
`andReturnValue` overloads (bool, int, unsigned int, long int, unsigned long int,
longlong, ulonglong, double, const char*, void*, const void*, `void (*)()`),
`onObject(void*)`.

### MockActualCall (include/CppUTestExt/MockActualCall.h:39-118)

Same `withParameter` overload family (no double-with-tolerance; const void* present).
Pure virtuals: `withName`, `withCallOrder(unsigned)`, typed `with*Parameter` setters,
`withParameterOfType`, `withOutputParameter(name, void*)`,
`withOutputParameterOfType(typeName, name, void*)`, `withMemoryBufferParameter`,
`hasReturnValue()`, `returnValue()`, and paired
`return<Type>Value()` / `return<Type>ValueOrDefault(default)` for bool, int,
unsigned int, long int, unsigned long int, longlong, ulonglong, string, double,
pointer, const pointer, function pointer; `onObject(const void*)`.

`StringFrom(const MockNamedValue&)` is declared in MockExpectedCall.h:35 and returns
`parameter.toString()`.

### MockNamedValue::toString() value rendering (MockNamedValue.cpp:489-525)

- bool → `"true"`/`"false"`; int/unsigned/long/unsigned long/longlong/ulonglong →
  `"<decimal> (0x<hex>)"` (same decimal + `BracketsFormattedHexStringFrom` as
  LONGS_EQUAL, **no padding** here);
- `const char*` → the string; `void*`/`const void*`/`void (*)()` → `"0x<hex>"`;
- double → `StringFrom(double)` (`"%.6g"`);
- `const unsigned char*` (memory buffer) →
  `"Size = %u | HexContents = <up to 128 bytes hex>"` + `" ..."` if truncated
  (`StringFromBinaryWithSizeOrNull`, SimpleString.cpp:990-1005);
- custom object with comparator → `comparator->valueToString(...)`;
- else `"No comparator found for type: \"%s\""`.

Type names used in messages and type matching: `"bool"`, `"int"`, `"unsigned int"`,
`"long int"`, `"unsigned long int"`, `"long long int"`, `"unsigned long long int"`,
`"double"`, `"const char*"`, `"void*"`, `"const void*"`, `"void (*)()"`,
`"const unsigned char*"`, or the user-supplied custom type name.

### Expected-call list dump format (MockExpectedCall.cpp:386-431, MockExpectedCallsList.cpp:308-360)

`MockCheckedExpectedCall::callToString()`:

```
[ (object address: %p):: ]<name> -> [expected call order: <%u> -> | expected calls order: <%u..%u> -> ]
<params> (expected %u call%s, called %u time%s)
```

(one line). `<params>` is `"no parameters"` or `"all parameters ignored"` when none;
otherwise comma-separated `"%s %s: <%s>"` (type, name, value) for input parameters,
then output parameters as `"%s %s: <output>"`, with `", "` between groups, suffixed
`", other parameters are ignored"` if `ignoreOtherParameters()`. Plurals: `"call"`/
`"calls"`, `"time"`/`"times"`.

`missingParametersToString()`: comma-separated `"%s %s"` (type name) of unmatched
parameters.

List rendering: each call on its own line prefixed by the given line prefix; empty
list renders as prefix + `"<none>"`.

### MockFailure messages (src/CppUTestExt/MockFailure.cpp) — exact

All are TestFailures attributed to the current test (file/line = the test's own
file/line). Shared trailer `addExpectationsAndCallHistory` (MockFailure.cpp:72-78):

```
	EXPECTED calls that WERE NOT fulfilled:
		<unfulfilled list or <none>>
	EXPECTED calls that WERE fulfilled:
		<fulfilled list or <none>>
```

(line prefix is two tabs `\t\t`). The "related to function" variant
(MockFailure.cpp:80-96):

```
	EXPECTED calls that WERE NOT fulfilled related to function: <name>
		...
	EXPECTED calls that WERE fulfilled related to function: <name>
		...
```

- **MockExpectedCallsDidntHappenFailure** (98-102):
  `"Mock Failure: Expected call WAS NOT fulfilled.\n"` + history.
- **MockUnexpectedCallHappenedFailure** (104-116): if there were already N fulfilled
  actual calls for that name:
  `"Mock Failure: Unexpected additional (%s) call to function: "` with ordinal
  (`StringFromOrdinalNumber`: 1st/2nd/3rd/Nth; 11–13 get "th"); else
  `"Mock Failure: Unexpected call to function: "`; + name + `"\n"` + history.
  (Also used for `expectNoCall` violations.)
- **MockCallOrderFailure** (118-127): `"Mock Failure: Out of order calls"` + `"\n"` +
  history filtered to out-of-order expectations.
- **MockUnexpectedInputParameterFailure** (129-165): if no expectation for the function
  has that parameter name:
  `"Mock Failure: Unexpected parameter name to function \"<fn>\": <paramName>"`;
  else
  `"Mock Failure: Unexpected parameter value to parameter \"<param>\" to function \"<fn>\": <<value>>"`;
  then `"\n"` + related-to-function history + 
  `"\n\tACTUAL unexpected parameter passed to function: <fn>\n"` +
  `"\t\t<type> <name>: <<value>>"`.
- **MockUnexpectedOutputParameterFailure** (167-200): name case:
  `"Mock Failure: Unexpected output parameter name to function \"<fn>\": <param>"`;
  type case:
  `"Mock Failure: Unexpected parameter type \"<type>\" to output parameter \"<param>\" to function \"<fn>\""`;
  then `"\n"` + related history +
  `"\n\tACTUAL unexpected output parameter passed to function: <fn>\n"` +
  `"\t\t<type> <name>"`.
- **MockExpectedParameterDidntHappenFailure** (202-217):
  `"Mock Failure: Expected parameter for function \"<fn>\" did not happen.\n"` +
  `"\tEXPECTED calls with MISSING parameters related to function: <fn>\n"` +
  calls-with-missing-parameters list (call line prefixed `\t\t`, missing-params line
  prefixed `\t\t` + `"\tMISSING parameters: "`) + `"\n"` + related history.
- **MockNoWayToCompareCustomTypeFailure** (219-222):
  `"MockFailure: No way to compare type <%s>. Please install a MockNamedValueComparator."`
  (note **no space** in "MockFailure:").
- **MockNoWayToCopyCustomTypeFailure** (224-227):
  `"MockFailure: No way to copy type <%s>. Please install a MockNamedValueCopier."`
- **MockUnexpectedObjectFailure** (229-234):
  `"MockFailure: Function called on an unexpected object: %s\n\tActual object for call has address: <%p>\n"`
  + related history.
- **MockExpectedObjectDidntHappenFailure** (236-240):
  `"Mock Failure: Expected call on object for function \"%s\" but it did not happen.\n"`
  + related history.

`MockFailureReporter::failTest` only fails if the test hasn't already failed; honors
`crashOnFailure` (MockFailure.cpp:34-61). Base MockFailure default message:
`"Test failed with MockFailure without an error! Something went seriously wrong."`.

### Tracing (MockActualCall.cpp:668-...)

`mock().tracing(true)` + `getTraceOutput()`: buffer accumulates
`"\nFunction name:"` + name, `" withCallOrder:"` + n, and per parameter
`" <name>:"` + value (integers also get `" (0x..)"` hex suffix).

### MockSupport_c (include/CppUTestExt/MockSupport_c.h)

`MockSupport_c* mock_c(void);` and `MockSupport_c* mock_scope_c(const char* scope);`
return structs of function pointers mirroring the C++ API (full field lists at
MockSupport_c.h:81-231): expected-call struct (`with<Type>Parameters`,
`withDoubleParametersAndTolerance`, `withParameterOfType`,
`withOutputParameterReturning`, `withOutputParameterOfTypeReturning`,
`withUnmodifiedOutputParameter`, `ignoreOtherParameters`, `andReturn<Type>Value`),
actual-call struct (same parameter setters minus tolerance, plus
`withOutputParameter`, `withOutputParameterOfType`, `hasReturnValue`, `returnValue`,
typed `*ReturnValue` / `return*ValueOrDefault`), and support struct
(`strictOrder`, `expectOneCall`, `expectNoCall`, `expectNCalls`, `actualCall`,
return-value getters, `set<Type>Data`/`setDataObject`/`setDataConstObject`/`getData`,
`disable`, `enable`, `ignoreOtherCalls`, `checkExpectations`, `expectedCallsLeft`,
`clear`, `crashOnFailure`, `installComparator(typeName, MockTypeEqualFunction_c,
MockTypeValueToStringFunction_c)`, `installCopier(typeName, MockTypeCopyFunction_c)`,
`removeAllComparatorsAndCopiers`). Values passed via tagged union `MockValue_c` with
enum `MOCKVALUETYPE_BOOL ... MOCKVALUETYPE_OBJECT`.

---

## 11. SimpleString public surface used by user code

### StringFrom overload list (include/CppUTest/SimpleString.h:189-242) with formats

| Overload | Format (SimpleString.cpp) |
|---|---|
| `StringFrom(bool)` | `"true"` / `"false"` (:671-674) |
| `StringFrom(const void*)` | `"0x" + HexStringFrom(value)` (:700-703) |
| `StringFrom(void (*)())` | `"0x" + hex` (:705-708) |
| `StringFrom(char)` | `"%c"` (:904-907) |
| `StringFrom(const char*)` | identity |
| `StringFromOrNull(const char*)` | `"(null)"` when NULL (:681-684) |
| `StringFrom(int)` | `"%d"` |
| `StringFrom(unsigned int)` | `"%u"` |
| `StringFrom(long)` | `"%ld"` |
| `StringFrom(unsigned long)` | `"%lu"` |
| `StringFrom(cpputest_longlong)` | `"%lld"` |
| `StringFrom(cpputest_ulonglong)` | `"%llu"` |
| `StringFrom(double, int precision = 6)` | `"%.*g"`; NaN → `"Nan - Not a number"`; Inf → `"Inf - Infinity"` (:894-902) |
| `StringFrom(const SimpleString&)` | identity |
| `StringFrom(const std::nullptr_t)` | `"(null)"` (C++11 + std lib) |
| `StringFrom(const std::string&)` | `.c_str()` |

HexStringFrom: unsigned int `"%x"`, unsigned long `"%lx"`, ulonglong `"%llx"`;
signed variants cast to unsigned; `signed char` is `"%x"` truncated to last 2 digits
when negative; pointers/function pointers via ulonglong (or `"%lx"` without
long long support). `BracketsFormattedHexStringFrom(v)` = `"(0x" + hex + ")"`;
`BracketsFormattedHexString(s)` = `"(0x" + s + ")"` (SimpleString.cpp:767-770).

Other public functions: `StringFromFormat(const char*, ...)` (printf semantics,
100-byte fast path then exact-size allocation), `VStringFromFormat`,
`StringFromBinary` (`"%02X "` per byte, trailing space stripped),
`StringFromBinaryOrNull`, `StringFromBinaryWithSize`
(`"Size = %u | HexContents = "` + max 128 bytes + `" ..."`),
`StringFromBinaryWithSizeOrNull`, `StringFromMaskedBits` (§2),
`StringFromOrdinalNumber` (`1st 2nd 3rd 4th... 11th 12th 13th... 21st`),
`PrintableStringFromOrNull`.

### SimpleString class API relied on by user code / public-API tests

Constructors `SimpleString(const char* = "")`, `(const char*, size_t repeatCount)`,
copy; `operator= + += (SimpleString/const char*)`; `operator== !=` (friends);
`at, find, findFrom, contains, containsNoCase, startsWith, endsWith, split,
equalsNoCase, count, replace(char,char), replace(const char*,const char*), lowerCase,
subString(pos), subString(pos, amount), subStringFromTill, copyToBuffer, printable,
asCharString, size, isEmpty`; statics `npos`, `padStringsToSameLength`,
`AtoI, AtoU, StrCmp, StrLen, StrNCmp, StrNCpy, StrStr, ToLower, MemCmp`,
`getStringAllocator/setStringAllocator`, `allocStringBuffer/deallocStringBuffer`.
`SimpleStringCollection` (`allocate, size, operator[]`). Upstream
`tests/CppUTest/SimpleStringTest.cpp` exercises all of these plus every StringFrom
format above (treat the format table as the parity contract).

---

## 12. Utest / UtestShell surface touched by user code & public tests

- `Utest` virtuals user code overrides: `setup()`, `teardown()`, `testBody()`,
  (rarely) `run()` (Utest.h:48-58).
- `UtestShell::getCurrent()` — used by all macros; never NULL (falls back to
  OutsideTestRunnerUTest singleton).
- Assertion entry points (full signatures in Utest.h:131-150 — see §2 list); all take
  optional `const TestTerminator&` (defaults `getCurrentTestTerminator()`).
- `fail(text, file, line)`, `exitTest(terminator)` (TEST_EXIT), `countCheck()`,
  `print(text, file, line)` (UT_PRINT), `printVeryVerbose`.
- Identity: `getName()`, `getGroup()`, `getFormattedName()` (`TEST(g, n)`),
  `getFile()`, `getLineNumber()`, `willRun()`, `hasFailed()`,
  `shouldRun(groupFilters, nameFilters)`.
- Registration/run plumbing public-tests use: `addTest`, `getNext`, `countTests`,
  `createTest`/`destroyTest`, `runOneTest`, `runOneTestInCurrentProcess`,
  `failWith(failure[, terminator])`, `addFailure`, `setGroupName/setTestName/
  setFileName/setLineNumber`, `setRunIgnored`, `isRunInSeperateProcess`/
  `setRunInSeperateProcess` (note upstream "Seperate" spelling).
- Statics: `setCrashOnFail()`, `restoreDefaultTestTerminator()`,
  `setRethrowExceptions(bool)` / `isRethrowingExceptions()`, `crash()`,
  `setCrashMethod()` / `resetCrashMethod()`, `getCurrentTestTerminator()`,
  `getCurrentTestTerminatorWithoutExceptions()`.
- `TestTerminator` hierarchy (Utest.h:62-95): `NormalTestTerminator` (throws
  `CppUTestFailedException` or longjmp without exceptions),
  `TestTerminatorWithoutExceptions` (longjmp), crashing variants of both.
- `IgnoredUtestShell`, `ExecFunctionTestShell`/`ExecFunction(WithoutParameters)`
  (function-pointer-based tests), `UtestShellPointerArray` (shuffle/reverse),
  `TestInstaller`. `CppUTestFailedException` has a single `int dummy_;` member.
- `bool doubles_equal(double, double, double threshold)` is a public free function
  (Utest.h:42).

---

## 13. Upstream test files: public-API conformance vs internals (first pass)

### tests/CppUTest

| File | Classification |
|---|---|
| AllTests.cpp / AllTests.h | runner main — not a test source |
| AllocLetTestFree.c/.h, AllocLetTestFreeTest.cpp | internals (leak-detector interop fixture) |
| AllocationInCFile.c/.h, AllocationInCppFile.cpp/.h | support fixtures only |
| CheatSheetTest.cpp | **public-API conformance** (canonical macro usage demo) |
| CommandLineArgumentsTest.cpp | **public conformance** for §4 flag parsing |
| CommandLineTestRunnerTest.cpp | mixed — drives runner via fakes; useful for exit codes/output wiring |
| CompatabilityTests.cpp | public-ish (compiler-compat constructs) |
| DummyMemoryLeakDetector.cpp/.h | support |
| JUnitOutputTest.cpp | **public conformance** — golden JUnit XML strings (§5) |
| MemoryLeakDetectorTest.cpp | internals (detector class), but asserts §7 report strings — useful goldens |
| MemoryLeakWarningTest.cpp | **public conformance** for plugin behavior / EXPECT_N_LEAKS |
| MemoryOperatorOverloadTest.cpp | internals (operator new overload mechanics) |
| PluginTest.cpp | **public conformance** for TestPlugin chaining (§9) |
| PreprocessorTest.cpp | public (macro expansion edge cases) |
| SetPluginTest.cpp | **public conformance** for UT_PTR_SET / SetPointerPlugin |
| SimpleMutexTest.cpp | internals |
| SimpleStringCacheTest.cpp | internals (string cache allocator) |
| SimpleStringTest.cpp | **public conformance** for §11 StringFrom/printable formats |
| TeamCityOutputTest.cpp | **public conformance** — golden TeamCity strings (§6) |
| TestFailureNaNTest.cpp | **public conformance** (DOUBLES_EQUAL NaN message) |
| TestFailureTest.cpp | **public conformance** — golden failure-message strings (§2) |
| TestFilterTest.cpp | **public conformance** for filter semantics |
| TestHarness_cTest.cpp (+ CFile.c) | **public conformance** for §8 C interface |
| TestInstallerTest.cpp | internals (installer/unDo) |
| TestMemoryAllocatorTest.cpp | internals |
| TestOutputTest.cpp | **public conformance** — golden console output (§3) |
| TestRegistryTest.cpp | internals (registry plumbing) |
| TestResultTest.cpp | internals |
| TestUTestMacro.cpp | **public conformance** — every assertion macro incl. messages |
| TestUTestStringMacro.cpp | **public conformance** — STRCMP* macros |
| UtestPlatformTest.cpp | internals/platform (fork mode etc.) |
| UtestTest.cpp | mixed — Utest lifecycle behavior, mostly public-visible semantics |

### tests/CppUTestExt

| File | Classification |
|---|---|
| AllTests.cpp | runner main |
| CodeMemoryReporterTest.cpp, MemoryReportAllocatorTest.cpp, MemoryReportFormatterTest.cpp, MemoryReporterPluginTest.cpp | extension internals (memory report plugin) |
| ExpectedFunctionsListTest.cpp | internals (MockExpectedCallsList) |
| GMockTest.cpp, GTest1Test.cpp, GTest2ConvertorTest.cpp | GTest/GMock convertor — out of scope unless shimmed |
| IEEE754PluginTest.cpp (+ _c.c/.h) | public-ish plugin conformance (IEEE754ExceptionsPlugin) |
| MockActualCallTest.cpp | semi-internal (MockCheckedActualCall direct) but pins behaviors |
| MockExpectedCallTest.cpp | semi-internal (MockCheckedExpectedCall, callToString goldens) |
| MockCallTest.cpp | **public conformance** (expectOneCall/expectNCalls semantics) |
| MockCheatSheetTest.cpp | **public conformance** (canonical mock usage) |
| MockComparatorCopierTest.cpp | **public conformance** (installComparator/Copier) |
| MockFailureReporterForTest.cpp/.h | support |
| MockFailureTest.cpp | **public conformance** — golden mock failure strings (§10) |
| MockFakeLongLong.cpp | internals (no-long-long fallback) |
| MockHierarchyTest.cpp | **public conformance** (mock scopes) |
| MockNamedValueTest.cpp | internals (MockNamedValue) |
| MockParameterTest.cpp | **public conformance** (withParameter overloads) |
| MockPluginTest.cpp | **public conformance** (MockSupportPlugin) |
| MockReturnValueTest.cpp | **public conformance** (return value surface) |
| MockStrictOrderTest.cpp | **public conformance** (strictOrder / call order failures) |
| MockSupportTest.cpp | **public conformance** (core mock() behavior, data store) |
| MockSupport_cTest.cpp (+ CFile.c/.h) | **public conformance** for §10 C mock interface |
| OrderedTestTest.cpp/.h/_c.c | extension (OrderedTest macros) — public if OrderedTest shipped |

---

## Appendix: misc constants worth pinning

- Progress line wrap: 50 indicators per line (TestOutput.cpp:124).
- Leak report buffer: 4096 bytes total; footer space reserved =
  `sizeof(footer)+10+sizeof(too-much)` (+ malloc warning) (MemoryLeakDetector.cpp:153-162).
- Guard bytes `B A S`, poison byte `0xCD`, hash table `MEMORY_LEAK_HASH_TABLE_SIZE`,
  corruption buffer 3 bytes (0 when `CPPUTEST_DISABLE_MEM_CORRUPTION_CHECK`).
- `SetPointerPlugin::MAX_SET` = 32.
- Plugin names: `"MemoryLeakPlugin"`, `"SetPointerPlugin"`.
- Shuffle: `srand(seed)` then Fisher-Yates from top with `rand() % (i+1)`
  (Utest.cpp:919-933) — reproduce exactly for `-s <seed>` parity.
- `OutsideTestRunnerUTest` group/name strings (§1) appear verbatim in failure output
  when assertions run outside tests.
