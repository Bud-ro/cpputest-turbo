#ifndef D_CppUTestConfig_h
#define D_CppUTestConfig_h

/* cpputest-revibed: thin configuration shim. Source-compatible subset of
 * upstream CppUTestConfig.h — only what user code and the public-API tests
 * actually reference. */

#include <stddef.h>

#if defined(__cplusplus) && __cplusplus >= 201103L
#define CPPUTEST_OVERRIDE override
#define NULLPTR nullptr
#else
#define CPPUTEST_OVERRIDE
#define NULLPTR NULL
#endif

#if defined(__GNUC__) || defined(__clang__)
#define CPPUTEST_NORETURN __attribute__((noreturn))
#define PUNUSED(x) PUNUSED_##x __attribute__((unused))
#else
#define CPPUTEST_NORETURN
#define PUNUSED(x) x
#endif

#ifndef CPPUTEST_USE_LONG_LONG
#define CPPUTEST_USE_LONG_LONG 1
#endif

#if CPPUTEST_USE_LONG_LONG
typedef long long cpputest_longlong;
typedef unsigned long long cpputest_ulonglong;
#endif

#endif
