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

/* Upstream also declares the replaced GLOBAL forms. With the std lib they
 * come from <new> (redeclaring without its exact exception-specs breaks
 * C++11 builds), so declare them only for CPPUTEST_USE_STD_CPP_LIB=0
 * builds, which never include <new>. */
#if !CPPUTEST_USE_STD_CPP_LIB
void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *p) noexcept;
void operator delete[](void *p) noexcept;
#if __cplusplus >= 201402L
void operator delete(void *p, size_t size) noexcept;
void operator delete[](void *p, size_t size) noexcept;
#endif
#endif /* !CPPUTEST_USE_STD_CPP_LIB */

#endif /* CPPUTEST_HAVE_ALREADY_DECLARED_NEW_DELETE_OVERLOADS */

#if CPPUTEST_USE_NEW_MACROS
/* redefining the `new` keyword is the entire point of this header; clang
 * (incl. Apple clang) warns -Wkeyword-macro under -Wpedantic, which breaks
 * consumers compiling with -Werror */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define new new (__FILE__, __LINE__)
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

#endif /* CPPUTEST_USE_MEM_LEAK_DETECTION && __cplusplus */
