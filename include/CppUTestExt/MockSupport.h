#ifndef D_MockSupport_h
#define D_MockSupport_h

/* cpputest-revibed: CppUMock facade over the C mock core. Current slice:
 * call expectations/counting, strict order, enable/disable, clear,
 * checkExpectations, ignoreOtherCalls, crashOnFailure. Parameters, return
 * values, comparators and the C interface land in later slices (the methods
 * are deliberately absent until they work). */

#include "CppUTest/TestHarness.h"
#include <cpputest_core/mock.h>

class MockExpectedCall
{
public:
    MockExpectedCall(cum_expectation *handle = NULLPTR) : handle_(handle) {}
    virtual ~MockExpectedCall() {}

    virtual MockExpectedCall &withCallOrder(unsigned int order)
    {
        return withCallOrder(order, order);
    }

    virtual MockExpectedCall &withCallOrder(unsigned int initialOrder,
                                            unsigned int finalOrder)
    {
        cum_expectation_with_call_order(handle_, initialOrder, finalOrder);
        return *this;
    }

    virtual MockExpectedCall &ignoreOtherParameters()
    {
        cum_expectation_ignore_other_parameters(handle_);
        return *this;
    }

private:
    cum_expectation *handle_;
};

class MockActualCall
{
public:
    MockActualCall() : handle_(NULLPTR) {}
    virtual ~MockActualCall() {}

    void bind(cum_actual *handle) { handle_ = handle; }

private:
    cum_actual *handle_;
};

class MockSupport
{
public:
    MockSupport(cum_scope *scope) : scope_(scope) {}
    virtual ~MockSupport() {}

    virtual void strictOrder() { cum_strict_order(scope_); }

    virtual MockExpectedCall &expectOneCall(const SimpleString &functionName)
    {
        return expectNCalls(1, functionName);
    }

    virtual MockExpectedCall &expectNoCall(const SimpleString &functionName)
    {
        return expectNCalls(0, functionName);
    }

    virtual MockExpectedCall &expectNCalls(unsigned int amount,
                                           const SimpleString &functionName)
    {
        cum_expectation *e =
            cum_expect_n_calls(scope_, amount, functionName.asCharString());
        if (!e)
            return ignoredExpectedCall();
        MockExpectedCall *facade = new MockExpectedCall(e);
        cum_expectation_set_user(e, facade);
        ensureFacadeFreeInstalled();
        return *facade;
    }

    virtual MockActualCall &actualCall(const SimpleString &functionName)
    {
        cum_actual *a = cum_actual_call(scope_, functionName.asCharString());
        actualCall_.bind(a);
        return actualCall_;
    }

    virtual void checkExpectations() { cum_check_expectations_all(); }
    virtual void clear() { cum_clear_all(); }
    virtual bool expectedCallsLeft() { return cum_expected_calls_left_all() != 0; }
    virtual void ignoreOtherCalls() { cum_ignore_other_calls(scope_); }
    virtual void disable() { cum_enable(scope_, 0); }
    virtual void enable() { cum_enable(scope_, 1); }
    virtual void crashOnFailure(bool shouldCrash = true)
    {
        cum_crash_on_failure(shouldCrash ? 1 : 0);
    }

private:
    static MockExpectedCall &ignoredExpectedCall()
    {
        static MockExpectedCall ignored(NULLPTR);
        return ignored;
    }

    static void freeFacade(void *facade)
    {
        delete static_cast<MockExpectedCall *>(facade);
    }

    static void ensureFacadeFreeInstalled()
    {
        cum_set_facade_free(freeFacade);
    }

    cum_scope *scope_;
    MockActualCall actualCall_;
};

inline MockSupport &mock(const SimpleString &mockName = "",
                         void * /*failureReporterForThisCall*/ = NULLPTR)
{
    /* one facade per scope, kept for the process lifetime */
    struct ScopeNode
    {
        SimpleString name;
        MockSupport *support;
        ScopeNode *next;
    };
    static ScopeNode *head = NULLPTR;

    for (ScopeNode *n = head; n; n = n->next)
        if (n->name == mockName)
            return *n->support;

    /* process-lifetime singletons: keep them out of leak accounting, like
     * upstream's saveAndDisableNewDeleteOverloads around mock internals */
    cu_mem_save_and_disable_tracking();
    ScopeNode *n = new ScopeNode;
    n->name = mockName;
    n->support = new MockSupport(cum_scope_get(mockName.asCharString()));
    n->next = head;
    head = n;
    cu_mem_restore_tracking();
    return *n->support;
}

#endif
