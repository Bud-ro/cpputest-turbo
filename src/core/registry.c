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
