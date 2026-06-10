#include "internal.h"

static cu_test *tests_head;

void cu_registry_add(cu_test *t)
{
    t->next = tests_head;
    tests_head = t;
}

void cu_registry_undo_last_add(void)
{
    if (tests_head)
        tests_head = tests_head->next;
}

cu_test *cu_registry_tests(void)
{
    return tests_head;
}

void cu_registry_set_tests(cu_test *head)
{
    tests_head = head;
}

/* UtestShellPointerArray: snapshot the list into an array, permute, relink. */

#include <stdlib.h>

static size_t registry_to_array(cu_test ***out)
{
    size_t count = 0;
    for (cu_test *t = tests_head; t; t = t->next)
        count++;
    if (count == 0) {
        *out = NULL;
        return 0;
    }
    cu_test **array = cu_xmalloc(count * sizeof *array);
    size_t i = 0;
    for (cu_test *t = tests_head; t; t = t->next)
        array[i++] = t;
    *out = array;
    return count;
}

static void relink_in_order(cu_test **array, size_t count)
{
    cu_test *tests = NULL;
    for (size_t i = 0; i < count; i++) {
        array[count - i - 1]->next = tests;
        tests = array[count - i - 1];
    }
    tests_head = tests;
}

void cu_registry_reverse(void)
{
    cu_test **array;
    size_t count = registry_to_array(&array);
    if (count == 0)
        return;
    for (size_t i = 0; i < count / 2; i++) {
        cu_test *tmp = array[i];
        array[i] = array[count - i - 1];
        array[count - i - 1] = tmp;
    }
    relink_in_order(array, count);
    free(array);
}

/* upstream's swappable platform hooks; tests can UT_PTR_SET them */
void (*PlatformSpecificSrand)(unsigned int) = srand;
int (*PlatformSpecificRand)(void) = rand;

/* Fisher-Yates with srand/rand, matching upstream's PlatformSpecificSrand/
 * Rand so that a given seed yields the same order on the same libc. */
void cu_registry_shuffle(size_t seed)
{
    cu_test **array;
    size_t count = registry_to_array(&array);
    if (count == 0)
        return;
    PlatformSpecificSrand((unsigned)seed);
    for (size_t i = count - 1; i >= 1; --i) {
        size_t j = (size_t)PlatformSpecificRand() % (i + 1);
        cu_test *tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
    relink_in_order(array, count);
    free(array);
}
