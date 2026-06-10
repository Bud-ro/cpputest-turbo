/* cpputest-revibed: global operator new/delete replacements routing into the
 * C core's leak tracker. This is the only C++ translation unit in the
 * library — replacement operators must be out-of-line by the standard.
 * C-only consumers can build the library without it (make CPPUTEST_C_ONLY=1);
 * they lose nothing, since new/delete tracking is meaningless in C. */

#include <cpputest_core/core.h>

#include <cstdlib>
#include <new>

static void *allocate(size_t size, const char *file, size_t line, int type)
{
    if (cu_mem_tracking())
        return cu_mem_alloc_tracked(size, file, line, type);
    return malloc(size);
}

static void deallocate(void *p, int type)
{
    if (!p)
        return;
    if (cu_mem_tracking())
        cu_mem_free_tracked(p, "<unknown>", 0, type);
    else
        cu_mem_release_if_tracked(p); /* p may still be a tracked block */
}

void *operator new(std::size_t size)
{
    void *p = allocate(size, "<unknown>", 0, CU_MEM_NEW);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size, "<unknown>", 0, CU_MEM_NEW);
}

void *operator new(std::size_t size, const char *file, int line)
{
    void *p = allocate(size, file, (size_t)line, CU_MEM_NEW);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void *operator new(std::size_t size, const char *file, size_t line)
{
    void *p = allocate(size, file, line, CU_MEM_NEW);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void operator delete(void *p) noexcept
{
    deallocate(p, CU_MEM_NEW);
}

void operator delete(void *p, std::size_t) noexcept
{
    deallocate(p, CU_MEM_NEW);
}

void operator delete(void *p, const std::nothrow_t &) noexcept
{
    deallocate(p, CU_MEM_NEW);
}

void operator delete(void *p, const char *, int) noexcept
{
    deallocate(p, CU_MEM_NEW);
}

void operator delete(void *p, const char *, size_t) noexcept
{
    deallocate(p, CU_MEM_NEW);
}

void *operator new[](std::size_t size)
{
    void *p = allocate(size, "<unknown>", 0, CU_MEM_NEW_ARRAY);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return allocate(size, "<unknown>", 0, CU_MEM_NEW_ARRAY);
}

void *operator new[](std::size_t size, const char *file, int line)
{
    void *p = allocate(size, file, (size_t)line, CU_MEM_NEW_ARRAY);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void *operator new[](std::size_t size, const char *file, size_t line)
{
    void *p = allocate(size, file, line, CU_MEM_NEW_ARRAY);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void operator delete[](void *p) noexcept
{
    deallocate(p, CU_MEM_NEW_ARRAY);
}

void operator delete[](void *p, std::size_t) noexcept
{
    deallocate(p, CU_MEM_NEW_ARRAY);
}

void operator delete[](void *p, const std::nothrow_t &) noexcept
{
    deallocate(p, CU_MEM_NEW_ARRAY);
}

void operator delete[](void *p, const char *, int) noexcept
{
    deallocate(p, CU_MEM_NEW_ARRAY);
}

void operator delete[](void *p, const char *, size_t) noexcept
{
    deallocate(p, CU_MEM_NEW_ARRAY);
}
