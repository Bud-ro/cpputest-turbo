/* Mock-path benchmark: compiles against upstream and cpputest-turbo.
 * Measures the hot CppUMock flow a mock-heavy embedded suite exercises:
 * expectation creation, parameter matching (int + string), return-value
 * delivery, periodic checkExpectations+clear, plus a custom-type
 * (comparator) variant covering the binding path. */

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTestExt/MockSupport.h"

#include <stdio.h>

#define BATCHES 1000
#define PER_BATCH 100

TEST_GROUP(MockBench){TEST_TEARDOWN(){mock().clear();
}
}
;

TEST(MockBench, scalarChurn)
{
    for (int b = 0; b < BATCHES; b++) {
        for (int i = 0; i < PER_BATCH; i++) {
            mock()
                .expectOneCall("driverWrite")
                .withParameter("reg", i)
                .withParameter("name", "ctrl")
                .andReturnValue(i + 1);
            int r = mock()
                        .actualCall("driverWrite")
                        .withParameter("reg", i)
                        .withParameter("name", "ctrl")
                        .returnIntValue();
            if (r != i + 1)
                FAIL("wrong return");
        }
        mock().checkExpectations();
        mock().clear();
    }
}

class IntBoxComparator : public MockNamedValueComparator
{
  public:
    bool isEqual(const void *a, const void *b) CPPUTEST_OVERRIDE
    {
        return *(const int *)a == *(const int *)b;
    }
    SimpleString valueToString(const void *o) CPPUTEST_OVERRIDE
    {
        return StringFrom(*(const int *)o);
    }
};
static IntBoxComparator boxCmp;
static int boxes[4] = {1, 2, 3, 4};

TEST(MockBench, customTypeChurn)
{
    mock().installComparator("IntBox", boxCmp);
    for (int b = 0; b < BATCHES / 2; b++) {
        for (int i = 0; i < PER_BATCH; i++) {
            mock()
                .expectOneCall("submit")
                .withParameterOfType("IntBox", "box", &boxes[i & 3]);
            mock().actualCall("submit").withParameterOfType("IntBox", "box",
                                                            &boxes[i & 3]);
        }
        mock().checkExpectations();
        mock().clear();
    }
    mock().removeAllComparatorsAndCopiers();
}

int main(int argc, char **argv)
{
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
