#ifndef D_TestHarness_h
#define D_TestHarness_h

/* cpputest-revibed: the one header test files include. Deliberately shallow —
 * keeping this include chain thin is where the compile-time win comes from. */

#include "CppUTest/CppUTestConfig.h"
#include "CppUTest/Utest.h"
#include "CppUTest/UtestMacros.h"

/* Memory leak detection macros are pulled in here by upstream too
 * (MemoryLeakDetectorNewMacros.h) — they land in Phase 4. */

#endif
