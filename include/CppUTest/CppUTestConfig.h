#ifndef D_CppUTestConfig_h
#define D_CppUTestConfig_h

/* cpputest-turbo: thin configuration shim. Source-compatible subset of
 * upstream CppUTestConfig.h — only what user code and the public-API tests
 * actually reference. */

#include <stddef.h>

#if defined(__cplusplus) && __cplusplus >= 201103L
#define CPPUTEST_OVERRIDE override
#define CPPUTEST_DESTRUCTOR_OVERRIDE override
#define NULLPTR nullptr
#else
#define CPPUTEST_OVERRIDE
#define CPPUTEST_DESTRUCTOR_OVERRIDE
#define NULLPTR NULL
#endif

#if defined(__GNUC__) || defined(__clang__)
#define CPPUTEST_NORETURN __attribute__((noreturn))
#else
#define CPPUTEST_NORETURN
#endif

/* upstream guards PUNUSED against a user pre-definition */
#ifndef PUNUSED
#if defined(__GNUC__) || defined(__clang__)
#define PUNUSED(x) PUNUSED_##x __attribute__((unused))
#else
#define PUNUSED(x) x
#endif
#endif

#ifndef CPPUTEST_USE_LONG_LONG
#define CPPUTEST_USE_LONG_LONG 1
#endif

#ifndef CPPUTEST_USE_STD_CPP_LIB
#define CPPUTEST_USE_STD_CPP_LIB 1
#endif

/* upstream: -DCPPUTEST_MEM_LEAK_DETECTION_DISABLED turns the new/malloc
 * macros off wholesale (needed for ASan builds and embedded targets) */
#ifndef CPPUTEST_USE_MEM_LEAK_DETECTION
#ifdef CPPUTEST_MEM_LEAK_DETECTION_DISABLED
#define CPPUTEST_USE_MEM_LEAK_DETECTION 0
#else
#define CPPUTEST_USE_MEM_LEAK_DETECTION 1
#endif
#endif

#ifndef CPPUTEST_HAVE_EXCEPTIONS
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#define CPPUTEST_HAVE_EXCEPTIONS 1
#else
#define CPPUTEST_HAVE_EXCEPTIONS 0
#endif
#endif

#if CPPUTEST_USE_LONG_LONG
typedef long long cpputest_longlong;
typedef unsigned long long cpputest_ulonglong;
#endif

#endif
