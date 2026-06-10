#ifndef D_OrderedTest_h
#define D_OrderedTest_h

/* cpputest-turbo: OrderedTest port — TEST_ORDERED(group, name, level)
 * runs tests in ascending level order regardless of registration order.
 * Logic mirrors upstream OrderedTest.cpp, threading an ordered chain
 * through the registry list. */

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestRegistry.h"

class OrderedTestShell : public UtestShell
{
  public:
    OrderedTestShell() : _nextOrderedTest(NULLPTR), _level(0) {}
    virtual ~OrderedTestShell() CPPUTEST_DESTRUCTOR_OVERRIDE {}

    virtual OrderedTestShell *addOrderedTest(OrderedTestShell *test)
    {
        UtestShell::addTest(test);
        _nextOrderedTest = test;
        return this;
    }

    virtual OrderedTestShell *getNextOrderedTest() { return _nextOrderedTest; }

    int getLevel() { return _level; }
    void setLevel(int level) { _level = level; }

    static void addOrderedTestToHead(OrderedTestShell *test)
    {
        TestRegistry *reg = TestRegistry::getCurrentRegistry();
        UtestShell *head = getOrderedTestHead();

        if (NULLPTR == reg->getFirstTest() || head == reg->getFirstTest()) {
            reg->addTest(test);
        } else {
            reg->getTestWithNext(head)->addTest(test);
            test->addTest(head);
        }

        test->_nextOrderedTest = getOrderedTestHead();
        setOrderedTestHead(test);
    }

    static OrderedTestShell *getOrderedTestHead() { return orderedTestHead(); }
    static bool firstOrderedTest() { return getOrderedTestHead() == NULLPTR; }
    static void setOrderedTestHead(OrderedTestShell *test)
    {
        orderedTestHead() = test;
    }

  private:
    static OrderedTestShell *&orderedTestHead()
    {
        static OrderedTestShell *head = NULLPTR;
        return head;
    }

    OrderedTestShell *_nextOrderedTest;
    int _level;
};

class OrderedTestInstaller
{
  public:
    explicit OrderedTestInstaller(OrderedTestShell &test, const char *groupName,
                                  const char *testName, const char *fileName,
                                  size_t lineNumber, int level)
    {
        test.setTestName(testName);
        test.setGroupName(groupName);
        test.setFileName(fileName);
        test.setLineNumber(lineNumber);
        test.setLevel(level);

        if (OrderedTestShell::firstOrderedTest())
            OrderedTestShell::addOrderedTestToHead(&test);
        else
            addOrderedTestInOrder(&test);
    }

    virtual ~OrderedTestInstaller() {}

  private:
    void addOrderedTestInOrder(OrderedTestShell *test)
    {
        if (test->getLevel() <
            OrderedTestShell::getOrderedTestHead()->getLevel())
            OrderedTestShell::addOrderedTestToHead(test);
        else
            addOrderedTestInOrderNotAtHeadPosition(test);
    }

    void addOrderedTestInOrderNotAtHeadPosition(OrderedTestShell *test)
    {
        OrderedTestShell *current = OrderedTestShell::getOrderedTestHead();
        while (current->getNextOrderedTest()) {
            if (current->getNextOrderedTest()->getLevel() > test->getLevel()) {
                test->addOrderedTest(current->getNextOrderedTest());
                current->addOrderedTest(test);
                return;
            }
            current = current->getNextOrderedTest();
        }
        test->addOrderedTest(current->getNextOrderedTest());
        current->addOrderedTest(test);
    }
};

#define TEST_ORDERED(testGroup, testName, testLevel)                           \
    /* declarations for compilers */                                           \
    class TEST_##testGroup##_##testName##_TestShell;                           \
    extern TEST_##testGroup##_##testName##_TestShell                           \
        TEST_##testGroup##_##testName##_Instance;                              \
    class TEST_##testGroup##_##testName##_Test                                 \
        : public TEST_GROUP_##CppUTestGroup##testGroup                         \
    {                                                                          \
      public:                                                                  \
        TEST_##testGroup##_##testName##_Test()                                 \
            : TEST_GROUP_##CppUTestGroup##testGroup()                          \
        {                                                                      \
        }                                                                      \
        void testBody() CPPUTEST_OVERRIDE;                                     \
    };                                                                         \
    class TEST_##testGroup##_##testName##_TestShell : public OrderedTestShell  \
    {                                                                          \
        virtual Utest *createTest() CPPUTEST_OVERRIDE                          \
        {                                                                      \
            return new TEST_##testGroup##_##testName##_Test;                   \
        }                                                                      \
    } TEST_##testGroup##_##testName##_Instance;                                \
    static OrderedTestInstaller TEST_##testGroup##_##testName##_Installer(     \
        TEST_##testGroup##_##testName##_Instance, #testGroup, #testName,       \
        __FILE__, __LINE__, testLevel);                                        \
    void TEST_##testGroup##_##testName##_Test::testBody()

#define TEST_ORDERED_C_WRAPPER(group_name, test_name, testLevel)               \
    extern "C" void test_##group_name##_##test_name##_wrapper_c(void);         \
    TEST_ORDERED(group_name, test_name, testLevel)                             \
    {                                                                          \
        test_##group_name##_##test_name##_wrapper_c();                         \
    }

#endif
