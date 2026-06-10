#ifndef D_MockSupport_h
#define D_MockSupport_h

/* cpputest-revibed: CppUMock facade over the C mock core. Current slice:
 * call expectations/counting, strict order, enable/disable, clear,
 * checkExpectations, ignoreOtherCalls, crashOnFailure. Parameters, return
 * values, comparators and the C interface land in later slices (the methods
 * are deliberately absent until they work). */

#include "CppUTest/TestHarness.h"
#include <cpputest_core/mock.h>

/* value constructors shared by expected/actual facades */
inline cum_value CppUMockBool(bool v) { cum_value x; x.type = CUM_T_BOOL; x.v.b = v ? 1 : 0; return x; }
inline cum_value CppUMockInt(int v) { cum_value x; x.type = CUM_T_INT; x.v.i = v; return x; }
inline cum_value CppUMockUInt(unsigned int v) { cum_value x; x.type = CUM_T_UINT; x.v.ui = v; return x; }
inline cum_value CppUMockLong(long v) { cum_value x; x.type = CUM_T_LONG; x.v.l = v; return x; }
inline cum_value CppUMockULong(unsigned long v) { cum_value x; x.type = CUM_T_ULONG; x.v.ul = v; return x; }
inline cum_value CppUMockLongLong(cpputest_longlong v) { cum_value x; x.type = CUM_T_LONGLONG; x.v.ll = v; return x; }
inline cum_value CppUMockULongLong(cpputest_ulonglong v) { cum_value x; x.type = CUM_T_ULONGLONG; x.v.ull = v; return x; }
inline cum_value CppUMockDouble(double v, double tolerance) { cum_value x; x.type = CUM_T_DOUBLE; x.v.dbl.value = v; x.v.dbl.tolerance = tolerance; return x; }
inline cum_value CppUMockString(const char *v) { cum_value x; x.type = CUM_T_STRING; x.v.str = v; return x; }
inline cum_value CppUMockPointer(void *v) { cum_value x; x.type = CUM_T_POINTER; x.v.ptr = v; return x; }
inline cum_value CppUMockConstPointer(const void *v) { cum_value x; x.type = CUM_T_CONST_POINTER; x.v.cptr = v; return x; }
inline cum_value CppUMockFunctionPointer(void (*v)()) { cum_value x; x.type = CUM_T_FUNCTIONPOINTER; x.v.fptr = (void (*)(void))v; return x; }
inline cum_value CppUMockMemBuffer(const unsigned char *buf, size_t size) { cum_value x; x.type = CUM_T_MEMBUFFER; x.v.mem.buf = buf; x.v.mem.size = size; return x; }

/* upstream's default tolerance for withParameter(name, double) */
#define MOCK_SUPPORT_DEFAULT_DOUBLE_TOLERANCE 0.005

inline cum_value CppUMockObject(const SimpleString &type, void *ptr)
{
    cum_value x;
    x.type = CUM_T_OBJECT;
    x.v.obj.type_name = type.asCharString();
    x.v.obj.ptr = ptr;
    return x;
}

inline cum_value CppUMockConstObject(const SimpleString &type, const void *ptr)
{
    cum_value x;
    x.type = CUM_T_CONST_OBJECT;
    x.v.obj.type_name = type.asCharString();
    x.v.obj.ptr = ptr;
    return x;
}

/* MockNamedValue facade: a typed value with STRCMP-checked getters,
 * as returned by getData()/returnValue() */
class MockNamedValue
{
public:
    MockNamedValue() : has_(false) { value_.type = CUM_T_INT; value_.v.i = 0; }
    MockNamedValue(cum_value value, bool has = true) : value_(value), has_(has) {}

    SimpleString getType() const
    {
        return has_ ? SimpleString(cum_value_type_name(&value_)) : SimpleString("");
    }

    bool getBoolValue() const { check("bool"); return value_.v.b != 0; }
    int getIntValue() const { check("int"); return value_.v.i; }
    unsigned int getUnsignedIntValue() const { check("unsigned int"); return value_.v.ui; }
    long int getLongIntValue() const { check("long int"); return value_.v.l; }
    unsigned long int getUnsignedLongIntValue() const { check("unsigned long int"); return value_.v.ul; }
    cpputest_longlong getLongLongIntValue() const { check("long long int"); return value_.v.ll; }
    cpputest_ulonglong getUnsignedLongLongIntValue() const { check("unsigned long long int"); return value_.v.ull; }
    double getDoubleValue() const { check("double"); return value_.v.dbl.value; }
    const char *getStringValue() const { check("const char*"); return value_.v.str; }
    void *getPointerValue() const { check("void*"); return value_.v.ptr; }
    const void *getConstPointerValue() const { check("const void*"); return value_.v.cptr; }
    void (*getFunctionPointerValue() const)() { check("void (*)()"); return (void (*)())value_.v.fptr; }
    void *getObjectPointer() const { return (void *)value_.v.obj.ptr; }
    const void *getConstObjectPointer() const { return value_.v.obj.ptr; }

    bool hasValue() const { return has_; }
    cum_value rawValue() const { return value_; }

private:
    void check(const char *expectedType) const
    {
        STRCMP_EQUAL(expectedType, has_ ? cum_value_type_name(&value_) : "");
    }

    cum_value value_;
    bool has_;
};

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

    MockExpectedCall &withParameter(const SimpleString &name, bool value) { return withBoolParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, int value) { return withIntParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, unsigned int value) { return withUnsignedIntParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, long int value) { return withLongIntParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, unsigned long int value) { return withUnsignedLongIntParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, cpputest_longlong value) { return withLongLongIntParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, cpputest_ulonglong value) { return withUnsignedLongLongIntParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, double value) { return withDoubleParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, double value, double tolerance) { return withDoubleParameterAndTolerance(name, value, tolerance); }
    MockExpectedCall &withParameter(const SimpleString &name, const char *value) { return withStringParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, void *value) { return withPointerParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, const void *value) { return withConstPointerParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, void (*value)()) { return withFunctionPointerParameter(name, value); }
    MockExpectedCall &withParameter(const SimpleString &name, const unsigned char *value, size_t size) { return withMemoryBufferParameter(name, value, size); }

    virtual MockExpectedCall &withBoolParameter(const SimpleString &name, bool value) { return addParam(name, CppUMockBool(value)); }
    virtual MockExpectedCall &withIntParameter(const SimpleString &name, int value) { return addParam(name, CppUMockInt(value)); }
    virtual MockExpectedCall &withUnsignedIntParameter(const SimpleString &name, unsigned int value) { return addParam(name, CppUMockUInt(value)); }
    virtual MockExpectedCall &withLongIntParameter(const SimpleString &name, long int value) { return addParam(name, CppUMockLong(value)); }
    virtual MockExpectedCall &withUnsignedLongIntParameter(const SimpleString &name, unsigned long int value) { return addParam(name, CppUMockULong(value)); }
    virtual MockExpectedCall &withLongLongIntParameter(const SimpleString &name, cpputest_longlong value) { return addParam(name, CppUMockLongLong(value)); }
    virtual MockExpectedCall &withUnsignedLongLongIntParameter(const SimpleString &name, cpputest_ulonglong value) { return addParam(name, CppUMockULongLong(value)); }
    virtual MockExpectedCall &withDoubleParameter(const SimpleString &name, double value) { return addParam(name, CppUMockDouble(value, MOCK_SUPPORT_DEFAULT_DOUBLE_TOLERANCE)); }
    virtual MockExpectedCall &withDoubleParameterAndTolerance(const SimpleString &name, double value, double tolerance) { return addParam(name, CppUMockDouble(value, tolerance)); }
    virtual MockExpectedCall &withStringParameter(const SimpleString &name, const char *value) { return addParam(name, CppUMockString(value)); }
    virtual MockExpectedCall &withPointerParameter(const SimpleString &name, void *value) { return addParam(name, CppUMockPointer(value)); }
    virtual MockExpectedCall &withConstPointerParameter(const SimpleString &name, const void *value) { return addParam(name, CppUMockConstPointer(value)); }
    virtual MockExpectedCall &withFunctionPointerParameter(const SimpleString &name, void (*value)()) { return addParam(name, CppUMockFunctionPointer(value)); }
    virtual MockExpectedCall &withMemoryBufferParameter(const SimpleString &name, const unsigned char *value, size_t size) { return addParam(name, CppUMockMemBuffer(value, size)); }

    virtual MockExpectedCall &ignoreOtherParameters()
    {
        cum_expectation_ignore_other_parameters(handle_);
        return *this;
    }

    virtual MockExpectedCall &andReturnValue(bool value) { return setReturn(CppUMockBool(value)); }
    virtual MockExpectedCall &andReturnValue(int value) { return setReturn(CppUMockInt(value)); }
    virtual MockExpectedCall &andReturnValue(unsigned int value) { return setReturn(CppUMockUInt(value)); }
    virtual MockExpectedCall &andReturnValue(long int value) { return setReturn(CppUMockLong(value)); }
    virtual MockExpectedCall &andReturnValue(unsigned long int value) { return setReturn(CppUMockULong(value)); }
    virtual MockExpectedCall &andReturnValue(cpputest_longlong value) { return setReturn(CppUMockLongLong(value)); }
    virtual MockExpectedCall &andReturnValue(cpputest_ulonglong value) { return setReturn(CppUMockULongLong(value)); }
    virtual MockExpectedCall &andReturnValue(double value) { return setReturn(CppUMockDouble(value, 0.0)); }
    virtual MockExpectedCall &andReturnValue(const char *value) { return setReturn(CppUMockString(value)); }
    virtual MockExpectedCall &andReturnValue(void *value) { return setReturn(CppUMockPointer(value)); }
    virtual MockExpectedCall &andReturnValue(const void *value) { return setReturn(CppUMockConstPointer(value)); }
    virtual MockExpectedCall &andReturnValue(void (*value)()) { return setReturn(CppUMockFunctionPointer(value)); }

private:
    MockExpectedCall &setReturn(cum_value value)
    {
        cum_expectation_and_return(handle_, value);
        return *this;
    }

    MockExpectedCall &addParam(const SimpleString &name, cum_value value)
    {
        cum_expectation_with_parameter(handle_, name.asCharString(), value);
        return *this;
    }

    cum_expectation *handle_;
};

class MockActualCall
{
public:
    MockActualCall() : handle_(NULLPTR) {}
    virtual ~MockActualCall() {}

    void bind(cum_actual *handle) { handle_ = handle; }

    MockActualCall &withParameter(const SimpleString &name, bool value) { return withBoolParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, int value) { return withIntParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, unsigned int value) { return withUnsignedIntParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, long int value) { return withLongIntParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, unsigned long int value) { return withUnsignedLongIntParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, cpputest_longlong value) { return withLongLongIntParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, cpputest_ulonglong value) { return withUnsignedLongLongIntParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, double value) { return withDoubleParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, const char *value) { return withStringParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, void *value) { return withPointerParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, const void *value) { return withConstPointerParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, void (*value)()) { return withFunctionPointerParameter(name, value); }
    MockActualCall &withParameter(const SimpleString &name, const unsigned char *value, size_t size) { return withMemoryBufferParameter(name, value, size); }

    virtual MockActualCall &withBoolParameter(const SimpleString &name, bool value) { return addParam(name, CppUMockBool(value)); }
    virtual MockActualCall &withIntParameter(const SimpleString &name, int value) { return addParam(name, CppUMockInt(value)); }
    virtual MockActualCall &withUnsignedIntParameter(const SimpleString &name, unsigned int value) { return addParam(name, CppUMockUInt(value)); }
    virtual MockActualCall &withLongIntParameter(const SimpleString &name, long int value) { return addParam(name, CppUMockLong(value)); }
    virtual MockActualCall &withUnsignedLongIntParameter(const SimpleString &name, unsigned long int value) { return addParam(name, CppUMockULong(value)); }
    virtual MockActualCall &withLongLongIntParameter(const SimpleString &name, cpputest_longlong value) { return addParam(name, CppUMockLongLong(value)); }
    virtual MockActualCall &withUnsignedLongLongIntParameter(const SimpleString &name, cpputest_ulonglong value) { return addParam(name, CppUMockULongLong(value)); }
    virtual MockActualCall &withDoubleParameter(const SimpleString &name, double value) { return addParam(name, CppUMockDouble(value, 0.0)); }
    virtual MockActualCall &withStringParameter(const SimpleString &name, const char *value) { return addParam(name, CppUMockString(value)); }
    virtual MockActualCall &withPointerParameter(const SimpleString &name, void *value) { return addParam(name, CppUMockPointer(value)); }
    virtual MockActualCall &withConstPointerParameter(const SimpleString &name, const void *value) { return addParam(name, CppUMockConstPointer(value)); }
    virtual MockActualCall &withFunctionPointerParameter(const SimpleString &name, void (*value)()) { return addParam(name, CppUMockFunctionPointer(value)); }
    virtual MockActualCall &withMemoryBufferParameter(const SimpleString &name, const unsigned char *value, size_t size) { return addParam(name, CppUMockMemBuffer(value, size)); }

    virtual bool hasReturnValue()
    {
        cum_value v;
        return cum_actual_return_value(handle_, &v) != 0;
    }

    virtual MockNamedValue returnValue()
    {
        cum_value v;
        int has = cum_actual_return_value(handle_, &v);
        return has ? MockNamedValue(v) : MockNamedValue();
    }

    virtual bool returnBoolValue() { return returnValue().getBoolValue(); }
    virtual bool returnBoolValueOrDefault(bool d) { return hasReturnValue() ? returnBoolValue() : d; }
    virtual int returnIntValue() { return returnValue().getIntValue(); }
    virtual int returnIntValueOrDefault(int d) { return hasReturnValue() ? returnIntValue() : d; }
    virtual unsigned int returnUnsignedIntValue() { return returnValue().getUnsignedIntValue(); }
    virtual unsigned int returnUnsignedIntValueOrDefault(unsigned int d) { return hasReturnValue() ? returnUnsignedIntValue() : d; }
    virtual long int returnLongIntValue() { return returnValue().getLongIntValue(); }
    virtual long int returnLongIntValueOrDefault(long int d) { return hasReturnValue() ? returnLongIntValue() : d; }
    virtual unsigned long int returnUnsignedLongIntValue() { return returnValue().getUnsignedLongIntValue(); }
    virtual unsigned long int returnUnsignedLongIntValueOrDefault(unsigned long int d) { return hasReturnValue() ? returnUnsignedLongIntValue() : d; }
    virtual cpputest_longlong returnLongLongIntValue() { return returnValue().getLongLongIntValue(); }
    virtual cpputest_longlong returnLongLongIntValueOrDefault(cpputest_longlong d) { return hasReturnValue() ? returnLongLongIntValue() : d; }
    virtual cpputest_ulonglong returnUnsignedLongLongIntValue() { return returnValue().getUnsignedLongLongIntValue(); }
    virtual cpputest_ulonglong returnUnsignedLongLongIntValueOrDefault(cpputest_ulonglong d) { return hasReturnValue() ? returnUnsignedLongLongIntValue() : d; }
    virtual double returnDoubleValue() { return returnValue().getDoubleValue(); }
    virtual double returnDoubleValueOrDefault(double d) { return hasReturnValue() ? returnDoubleValue() : d; }
    virtual const char *returnStringValue() { return returnValue().getStringValue(); }
    virtual const char *returnStringValueOrDefault(const char *d) { return hasReturnValue() ? returnStringValue() : d; }
    virtual void *returnPointerValue() { return returnValue().getPointerValue(); }
    virtual void *returnPointerValueOrDefault(void *d) { return hasReturnValue() ? returnPointerValue() : d; }
    virtual const void *returnConstPointerValue() { return returnValue().getConstPointerValue(); }
    virtual const void *returnConstPointerValueOrDefault(const void *d) { return hasReturnValue() ? returnConstPointerValue() : d; }
    virtual void (*returnFunctionPointerValue())() { return returnValue().getFunctionPointerValue(); }
    virtual void (*returnFunctionPointerValueOrDefault(void (*d)()))() { return hasReturnValue() ? returnFunctionPointerValue() : d; }

private:
    MockActualCall &addParam(const SimpleString &name, cum_value value)
    {
        cum_actual_with_parameter(handle_, name.asCharString(), value);
        return *this;
    }

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

    virtual bool hasReturnValue()
    {
        cum_value v;
        return cum_scope_return_value(scope_, &v) != 0;
    }

    virtual MockNamedValue returnValue()
    {
        cum_value v;
        int has = cum_scope_return_value(scope_, &v);
        return has ? MockNamedValue(v) : MockNamedValue();
    }

    virtual bool boolReturnValue() { return returnValue().getBoolValue(); }
    virtual int intReturnValue() { return returnValue().getIntValue(); }
    virtual unsigned int unsignedIntReturnValue() { return returnValue().getUnsignedIntValue(); }
    virtual long int longIntReturnValue() { return returnValue().getLongIntValue(); }
    virtual unsigned long int unsignedLongIntReturnValue() { return returnValue().getUnsignedLongIntValue(); }
    virtual cpputest_longlong longLongIntReturnValue() { return returnValue().getLongLongIntValue(); }
    virtual cpputest_ulonglong unsignedLongLongIntReturnValue() { return returnValue().getUnsignedLongLongIntValue(); }
    virtual const char *stringReturnValue() { return returnValue().getStringValue(); }
    virtual double doubleReturnValue() { return returnValue().getDoubleValue(); }
    virtual void *pointerReturnValue() { return returnValue().getPointerValue(); }
    virtual const void *constPointerReturnValue() { return returnValue().getConstPointerValue(); }
    virtual void (*functionPointerReturnValue())() { return returnValue().getFunctionPointerValue(); }

    virtual bool returnBoolValueOrDefault(bool d) { return hasReturnValue() ? boolReturnValue() : d; }
    virtual int returnIntValueOrDefault(int d) { return hasReturnValue() ? intReturnValue() : d; }
    virtual unsigned int returnUnsignedIntValueOrDefault(unsigned int d) { return hasReturnValue() ? unsignedIntReturnValue() : d; }
    virtual long int returnLongIntValueOrDefault(long int d) { return hasReturnValue() ? longIntReturnValue() : d; }
    virtual unsigned long int returnUnsignedLongIntValueOrDefault(unsigned long int d) { return hasReturnValue() ? unsignedLongIntReturnValue() : d; }
    virtual cpputest_longlong returnLongLongIntValueOrDefault(cpputest_longlong d) { return hasReturnValue() ? longLongIntReturnValue() : d; }
    virtual cpputest_ulonglong returnUnsignedLongLongIntValueOrDefault(cpputest_ulonglong d) { return hasReturnValue() ? unsignedLongLongIntReturnValue() : d; }
    virtual const char *returnStringValueOrDefault(const char *d) { return hasReturnValue() ? stringReturnValue() : d; }
    virtual double returnDoubleValueOrDefault(double d) { return hasReturnValue() ? doubleReturnValue() : d; }
    virtual void *returnPointerValueOrDefault(void *d) { return hasReturnValue() ? pointerReturnValue() : d; }
    virtual const void *returnConstPointerValueOrDefault(const void *d) { return hasReturnValue() ? constPointerReturnValue() : d; }
    virtual void (*returnFunctionPointerValueOrDefault(void (*d)()))() { return hasReturnValue() ? functionPointerReturnValue() : d; }

    /* data store */
    virtual bool hasData(const SimpleString &name)
    {
        return cum_scope_has_data(scope_, name.asCharString()) != 0;
    }

    virtual void setData(const SimpleString &name, bool value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockBool(value)); }
    virtual void setData(const SimpleString &name, int value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockInt(value)); }
    virtual void setData(const SimpleString &name, unsigned int value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockUInt(value)); }
    virtual void setData(const SimpleString &name, long int value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockLong(value)); }
    virtual void setData(const SimpleString &name, unsigned long int value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockULong(value)); }
    virtual void setData(const SimpleString &name, const char *value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockString(value)); }
    virtual void setData(const SimpleString &name, double value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockDouble(value, 0.0)); }
    virtual void setData(const SimpleString &name, void *value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockPointer(value)); }
    virtual void setData(const SimpleString &name, const void *value) { cum_scope_set_data(scope_, name.asCharString(), CppUMockConstPointer(value)); }
    virtual void setData(const SimpleString &name, void (*value)()) { cum_scope_set_data(scope_, name.asCharString(), CppUMockFunctionPointer(value)); }

    virtual void setDataObject(const SimpleString &name, const SimpleString &type, void *value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockObject(type, value));
    }

    virtual void setDataConstObject(const SimpleString &name, const SimpleString &type, const void *value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockConstObject(type, value));
    }

    virtual MockNamedValue getData(const SimpleString &name)
    {
        cum_value v;
        int has = cum_scope_get_data(scope_, name.asCharString(), &v);
        return has ? MockNamedValue(v) : MockNamedValue();
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
