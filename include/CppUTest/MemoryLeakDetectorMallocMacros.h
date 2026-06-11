/* cpputest-turbo: malloc-family redefinition routing through the leak
 * tracker. Opt-in like upstream: include this header (or force-include
 * MemoryLeakDetectorForceInclude.h) in C/C++ code under test. */

#include "CppUTest/CppUTestConfig.h"

#ifndef CPPUTEST_USE_MEM_LEAK_DETECTION
#ifdef CPPUTEST_MEM_LEAK_DETECTION_DISABLED
#define CPPUTEST_USE_MEM_LEAK_DETECTION 0
#else
#define CPPUTEST_USE_MEM_LEAK_DETECTION 1
#endif
#endif

#if CPPUTEST_USE_MEM_LEAK_DETECTION

#ifndef CPPUTEST_USE_MALLOC_MACROS
#define CPPUTEST_USE_MALLOC_MACROS 1

#ifdef __cplusplus
extern "C" {
#endif

extern void *cpputest_malloc_location(size_t size, const char *file,
                                      size_t line);
extern void *cpputest_calloc_location(size_t count, size_t size,
                                      const char *file, size_t line);
extern void *cpputest_realloc_location(void *p, size_t size, const char *file,
                                       size_t line);
extern void cpputest_free_location(void *p, const char *file, size_t line);
extern void crash_on_allocation_number(unsigned alloc_number);

#ifdef __cplusplus
}
#endif

#define malloc(a) cpputest_malloc_location(a, __FILE__, __LINE__)
#define calloc(a, b) cpputest_calloc_location(a, b, __FILE__, __LINE__)
#define realloc(a, b) cpputest_realloc_location(a, b, __FILE__, __LINE__)
#define free(a) cpputest_free_location(a, __FILE__, __LINE__)

#endif /* CPPUTEST_USE_MALLOC_MACROS */

/* strdup macros only when the user/config says strdup exists (upstream
 * gates on CPPUTEST_HAVE_STRDUP: strdup is POSIX, not ISO C, and replacing
 * it unasked surprises code calling the real one) */
#ifdef CPPUTEST_HAVE_STRDUP
#ifndef CPPUTEST_USE_STRDUP_MACROS

#ifdef __cplusplus
extern "C" {
#endif

extern char *cpputest_strdup_location(const char *s, const char *file,
                                      size_t line);
extern char *cpputest_strndup_location(const char *s, size_t n,
                                       const char *file, size_t line);

#ifdef __cplusplus
}
#endif

#define strdup(str) cpputest_strdup_location(str, __FILE__, __LINE__)
#define strndup(str, n) cpputest_strndup_location(str, n, __FILE__, __LINE__)

#define CPPUTEST_USE_STRDUP_MACROS 1
#endif /* CPPUTEST_USE_STRDUP_MACROS */
#endif /* CPPUTEST_HAVE_STRDUP */

#endif /* CPPUTEST_USE_MEM_LEAK_DETECTION */
