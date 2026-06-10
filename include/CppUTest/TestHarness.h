#ifndef D_TestHarness_h
#define D_TestHarness_h

/* cpputest-turbo: the one header test files include. Deliberately shallow —
 * keeping this include chain thin is where the compile-time win comes from. */

#include "CppUTest/CppUTestConfig.h"
#include "CppUTest/Utest.h"
#include "CppUTest/UtestMacros.h"
#include "CppUTest/SimpleString.h"
#include "CppUTest/TestResult.h"
#include "CppUTest/TestFailure.h"
#include "CppUTest/TestPlugin.h"
/* Last, like upstream: this one defines the `new` macro, so everything that
 * must compile against the real operator new is already parsed above. */
#include "CppUTest/MemoryLeakWarningPlugin.h"

#endif
