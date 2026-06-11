#ifndef D_MockNamedValue_h
#define D_MockNamedValue_h

/* cpputest-turbo: upstream splits the mock API across several headers;
 * here everything lives in MockSupport.h. This shim keeps consumer code
 * that includes the upstream header path compiling (MockNamedValue,
 * MockNamedValueComparator/Copier, the comparators-and-copiers
 * repository). */

#include "CppUTestExt/MockSupport.h"

#endif
