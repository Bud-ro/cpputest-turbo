/* Pure-C mock interface tests, driven from C test functions wrapped by the
 * usual C++ pairing file. */

#include "CppUTestExt/MockSupport_c.h"
#include "CppUTest/TestHarness_c.h"

TEST_GROUP_C_SETUP(MockC)
{
}

TEST_GROUP_C_TEARDOWN(MockC)
{
    mock_c()->checkExpectations();
    mock_c()->clear();
}

TEST_C(MockC, expectAndActual)
{
    mock_c()->expectOneCall("foo")->withIntParameters("a", 10)
        ->withStringParameters("s", "hello")->andReturnIntValue(42);
    int r = mock_c()->actualCall("foo")->withIntParameters("a", 10)
        ->withStringParameters("s", "hello")->intReturnValue();
    CHECK_EQUAL_C_INT(42, r);
}

TEST_C(MockC, returnDefault)
{
    mock_c()->expectOneCall("bar");
    CHECK_EQUAL_C_INT(7, mock_c()->actualCall("bar")->returnIntValueOrDefault(7));
    CHECK_C(!mock_c()->hasReturnValue());
}

TEST_C(MockC, outputParameter)
{
    int provided = 99;
    int received = 0;
    mock_c()->expectOneCall("get")->withOutputParameterReturning("v", &provided,
                                                                 sizeof provided);
    mock_c()->actualCall("get")->withOutputParameter("v", &received);
    CHECK_EQUAL_C_INT(99, received);
}

TEST_C(MockC, dataStore)
{
    mock_c()->setIntData("count", 3);
    MockValue_c v = mock_c()->getData("count");
    CHECK_EQUAL_C_INT(MOCKVALUETYPE_INTEGER, (int)v.type);
    CHECK_EQUAL_C_INT(3, v.value.intValue);
}

TEST_C(MockC, scopes)
{
    mock_scope_c("driver")->expectOneCall("init");
    mock_scope_c("driver")->actualCall("init");
}

TEST_C(MockC, supportLevelReturn)
{
    mock_c()->expectOneCall("calc")->andReturnDoubleValue(2.5);
    mock_c()->actualCall("calc");
    CHECK_C(mock_c()->hasReturnValue());
    CHECK_EQUAL_C_REAL(2.5, mock_c()->doubleReturnValue(), 0.001);
}
