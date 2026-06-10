/* cpputest-turbo: debug operator new declarations + the `new` macro.
 * No include guard on the macro section, mirroring upstream: the file may be
 * re-included to re-enable the macro after an #undef. */

#include "CppUTest/CppUTestConfig.h"

#ifndef CPPUTEST_USE_MEM_LEAK_DETECTION
#ifdef CPPUTEST_MEM_LEAK_DETECTION_DISABLED
#define CPPUTEST_USE_MEM_LEAK_DETECTION 0
#else
#define CPPUTEST_USE_MEM_LEAK_DETECTION 1
#endif
#endif

#if CPPUTEST_USE_MEM_LEAK_DETECTION && defined(__cplusplus)

#ifndef CPPUTEST_USE_NEW_MACROS
#define CPPUTEST_USE_NEW_MACROS 1
#endif

#ifndef CPPUTEST_HAVE_ALREADY_DECLARED_NEW_DELETE_OVERLOADS
#define CPPUTEST_HAVE_ALREADY_DECLARED_NEW_DELETE_OVERLOADS 1

/* Pre-include the standard headers that use placement new so the `new`
 * macro below cannot break them when user code includes them later
 * (upstream does the same dance). */
#if CPPUTEST_USE_STD_CPP_LIB
#include <new>
#include <memory>
#include <string>
#endif
#include <stddef.h>

void *operator new(size_t size, const char *file, int line);
void *operator new(size_t size, const char *file, size_t line);
void *operator new[](size_t size, const char *file, int line);
void *operator new[](size_t size, const char *file, size_t line);
void operator delete(void *p, const char *file, int line) noexcept;
void operator delete(void *p, const char *file, size_t line) noexcept;
void operator delete[](void *p, const char *file, int line) noexcept;
void operator delete[](void *p, const char *file, size_t line) noexcept;

#endif /* CPPUTEST_HAVE_ALREADY_DECLARED_NEW_DELETE_OVERLOADS */

#if CPPUTEST_USE_NEW_MACROS
#define new new (__FILE__, __LINE__)
#endif

#endif /* CPPUTEST_USE_MEM_LEAK_DETECTION && __cplusplus */
