#ifndef D_MockSupport_h
#define D_MockSupport_h

/* cpputest-turbo: CppUMock facade over the C mock core. Current slice:
 * call expectations/counting, strict order, enable/disable, clear,
 * checkExpectations, ignoreOtherCalls, crashOnFailure. Parameters, return
 * values, comparators and the C interface land in later slices (the methods
 * are deliberately absent until they work). */

#include "CppUTest/TestHarness.h"
#include "cpputest_core/mock.h"

/* value constructors shared by expected/actual facades */
inline cum_value CppUMockBool(bool v)
{
    cum_value x;
    x.type = CUM_T_BOOL;
    x.v.b = v ? 1 : 0;
    return x;
}
inline cum_value CppUMockInt(int v)
{
    cum_value x;
    x.type = CUM_T_INT;
    x.v.i = v;
    return x;
}
inline cum_value CppUMockUInt(unsigned int v)
{
    cum_value x;
    x.type = CUM_T_UINT;
    x.v.ui = v;
    return x;
}
inline cum_value CppUMockLong(long v)
{
    cum_value x;
    x.type = CUM_T_LONG;
    x.v.l = v;
    return x;
}
inline cum_value CppUMockULong(unsigned long v)
{
    cum_value x;
    x.type = CUM_T_ULONG;
    x.v.ul = v;
    return x;
}
inline cum_value CppUMockLongLong(cpputest_longlong v)
{
    cum_value x;
    x.type = CUM_T_LONGLONG;
    x.v.ll = v;
    return x;
}
inline cum_value CppUMockULongLong(cpputest_ulonglong v)
{
    cum_value x;
    x.type = CUM_T_ULONGLONG;
    x.v.ull = v;
    return x;
}
inline cum_value CppUMockDouble(double v, double tolerance)
{
    cum_value x;
    x.type = CUM_T_DOUBLE;
    x.v.dbl.value = v;
    x.v.dbl.tolerance = tolerance;
    return x;
}
inline cum_value CppUMockString(const char *v)
{
    cum_value x;
    x.type = CUM_T_STRING;
    x.v.str = v;
    return x;
}
inline cum_value CppUMockPointer(void *v)
{
    cum_value x;
    x.type = CUM_T_POINTER;
    x.v.ptr = v;
    return x;
}
inline cum_value CppUMockConstPointer(const void *v)
{
    cum_value x;
    x.type = CUM_T_CONST_POINTER;
    x.v.cptr = v;
    return x;
}
inline cum_value CppUMockFunctionPointer(void (*v)())
{
    cum_value x;
    x.type = CUM_T_FUNCTIONPOINTER;
    x.v.fptr = (void (*)(void))v;
    return x;
}
inline cum_value CppUMockMemBuffer(const unsigned char *buf, size_t size)
{
    cum_value x;
    x.type = CUM_T_MEMBUFFER;
    x.v.mem.buf = buf;
    x.v.mem.size = size;
    return x;
}

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

class MockNamedValueComparator
{
  public:
    MockNamedValueComparator() {}
    virtual ~MockNamedValueComparator() {}
    virtual bool isEqual(const void *object1, const void *object2) = 0;
    virtual SimpleString valueToString(const void *object) = 0;
};

class MockNamedValueCopier
{
  public:
    MockNamedValueCopier() {}
    virtual ~MockNamedValueCopier() {}
    virtual void copy(void *out, const void *in) = 0;
};

/* upstream's bulk comparator/copier container (MockNamedValue.h): collect
 * entries once, install them into a MockSupport in one call */
class MockNamedValueComparatorsAndCopiersRepository
{
    struct Node {
        SimpleString name;
        MockNamedValueComparator *comparator;
        MockNamedValueCopier *copier;
        Node *next;
    };
    Node *head_;

    void add(const SimpleString &name, MockNamedValueComparator *comparator,
             MockNamedValueCopier *copier)
    {
        Node *n = new Node;
        n->name = name;
        n->comparator = comparator;
        n->copier = copier;
        n->next = head_;
        head_ = n;
    }

    friend class MockSupport;

  public:
    MockNamedValueComparatorsAndCopiersRepository() : head_(NULLPTR) {}
    virtual ~MockNamedValueComparatorsAndCopiersRepository() { clear(); }

    virtual void installComparator(const SimpleString &name,
                                   MockNamedValueComparator &comparator)
    {
        add(name, &comparator, NULLPTR);
    }
    virtual void installCopier(const SimpleString &name,
                               MockNamedValueCopier &copier)
    {
        add(name, NULLPTR, &copier);
    }
    virtual void installComparatorsAndCopiers(
        const MockNamedValueComparatorsAndCopiersRepository &repository)
    {
        for (Node *n = repository.head_; n; n = n->next)
            add(n->name, n->comparator, n->copier);
    }
    virtual MockNamedValueComparator *
    getComparatorForType(const SimpleString &name)
    {
        for (Node *n = head_; n; n = n->next)
            if (n->comparator && n->name == name)
                return n->comparator;
        return NULLPTR;
    }
    virtual MockNamedValueCopier *getCopierForType(const SimpleString &name)
    {
        for (Node *n = head_; n; n = n->next)
            if (n->copier && n->name == name)
                return n->copier;
        return NULLPTR;
    }
    void clear()
    {
        while (head_) {
            Node *next = head_->next;
            delete head_;
            head_ = next;
        }
    }
};

/* Custom failure reporters are accepted for source compatibility but are
 * INERT: mock failures always report through the standard test-failure
 * path (upstream's default reporter does the same; only custom reporters
 * — used chiefly by upstream's own self-tests — would behave differently,
 * and that injection surface is a documented divergence). */
class MockFailureReporter;

/* C-core adapters for the virtuals above */
extern "C" {

inline int CppUMockComparatorEqual(void *ctx, const void *o1, const void *o2)
{
    return static_cast<MockNamedValueComparator *>(ctx)->isEqual(o1, o2) ? 1
                                                                         : 0;
}

inline char *CppUMockComparatorToString(void *ctx, const void *o)
{
    SimpleString s =
        static_cast<MockNamedValueComparator *>(ctx)->valueToString(o);
    return cu_str_printf("%s", s.asCharString());
}

inline void CppUMockCopierCopy(void *ctx, void *dst, const void *src)
{
    static_cast<MockNamedValueCopier *>(ctx)->copy(dst, src);
}

} /* extern "C" */

/* MockNamedValue facade: a typed value with STRCMP-checked getters,
 * as returned by getData()/returnValue() */
class MockNamedValue
{
  public:
    /* upstream's "empty" MockNamedValue is a named int 0 (MockNamedValue.cpp
     * ctor: type_("int"), intValue_ 0) — int-family getters on it succeed
     * and return 0; only the name distinguishes "" / "no return value" /
     * "returnValue" states */
    MockNamedValue()
    {
        value_.type = CUM_T_INT;
        value_.v.i = 0;
    }
    MockNamedValue(const SimpleString &name) : name_(name)
    {
        value_.type = CUM_T_INT;
        value_.v.i = 0;
    }
    MockNamedValue(const SimpleString &name, cum_value value)
        : name_(name), value_(value)
    {
    }
    explicit MockNamedValue(cum_value value) : value_(value) {}

    SimpleString getName() const { return name_; }
    SimpleString getType() const
    {
        return SimpleString(cum_value_type_name(&value_));
    }

    /* upstream parks a default repository here for its plugin machinery;
     * stored for source compatibility (the active comparator/copier
     * registry lives in the C core) */
    static void setDefaultComparatorsAndCopiersRepository(
        MockNamedValueComparatorsAndCopiersRepository *repository)
    {
        defaultRepository() = repository;
    }
    static MockNamedValueComparatorsAndCopiersRepository *
    getDefaultComparatorsAndCopiersRepository()
    {
        return defaultRepository();
    }

    bool getBoolValue() const
    {
        check("bool");
        return value_.v.b != 0;
    }
    int getIntValue() const
    {
        check("int");
        return value_.v.i;
    }
    /* upstream's typed getters apply integer WIDENING coercion before the
     * type assert (MockNamedValue.cpp:204-285); coerced reads don't count
     * a check. The empty value IS type int 0, so coercion covers it too. */
    unsigned int getUnsignedIntValue() const
    {
        if (value_.type == CUM_T_INT && value_.v.i >= 0)
            return (unsigned int)value_.v.i;
        check("unsigned int");
        return value_.v.ui;
    }
    long int getLongIntValue() const
    {
        if (value_.type == CUM_T_INT)
            return value_.v.i;
        if (value_.type == CUM_T_UINT)
            return (long int)value_.v.ui;
        check("long int");
        return value_.v.l;
    }
    unsigned long int getUnsignedLongIntValue() const
    {
        if (value_.type == CUM_T_UINT)
            return value_.v.ui;
        if (value_.type == CUM_T_INT && value_.v.i >= 0)
            return (unsigned long int)value_.v.i;
        if (value_.type == CUM_T_LONG && value_.v.l >= 0)
            return (unsigned long int)value_.v.l;
        check("unsigned long int");
        return value_.v.ul;
    }
    cpputest_longlong getLongLongIntValue() const
    {
        if (value_.type == CUM_T_INT)
            return value_.v.i;
        if (value_.type == CUM_T_UINT)
            return (cpputest_longlong)value_.v.ui;
        if (value_.type == CUM_T_LONG)
            return value_.v.l;
        if (value_.type == CUM_T_ULONG)
            return (cpputest_longlong)value_.v.ul;
        check("long long int");
        return value_.v.ll;
    }
    cpputest_ulonglong getUnsignedLongLongIntValue() const
    {
        if (value_.type == CUM_T_UINT)
            return value_.v.ui;
        if (value_.type == CUM_T_INT && value_.v.i >= 0)
            return (cpputest_ulonglong)value_.v.i;
        if (value_.type == CUM_T_LONG && value_.v.l >= 0)
            return (cpputest_ulonglong)value_.v.l;
        if (value_.type == CUM_T_ULONG)
            return value_.v.ul;
        if (value_.type == CUM_T_LONGLONG && value_.v.ll >= 0)
            return (cpputest_ulonglong)value_.v.ll;
        check("unsigned long long int");
        return value_.v.ull;
    }
    double getDoubleValue() const
    {
        check("double");
        return value_.v.dbl.value;
    }
    const char *getStringValue() const
    {
        check("const char*");
        return value_.v.str;
    }
    void *getPointerValue() const
    {
        check("void*");
        return value_.v.ptr;
    }
    const void *getConstPointerValue() const
    {
        check("const void*");
        return value_.v.cptr;
    }
    void (*getFunctionPointerValue() const)()
    {
        check("void (*)()");
        return (void (*)())value_.v.fptr;
    }
    /* const_cast, not a C-style cast: this header compiles inside consumer
     * TUs under consumer flags, and -Wcast-qual rejects the C-style form */
    void *getObjectPointer() const
    {
        return const_cast<void *>(value_.v.obj.ptr);
    }
    const void *getConstObjectPointer() const { return value_.v.obj.ptr; }

  private:
    void check(const char *expectedType) const
    {
        STRCMP_EQUAL(expectedType, cum_value_type_name(&value_));
    }

    /* single process-wide slot (function-local static: one instance across
     * all TUs including this inline-heavy header) */
    static MockNamedValueComparatorsAndCopiersRepository *&defaultRepository()
    {
        static MockNamedValueComparatorsAndCopiersRepository *repo = NULLPTR;
        return repo;
    }

    SimpleString name_;
    cum_value value_;
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

    MockExpectedCall &withParameter(const SimpleString &name, bool value)
    {
        return withBoolParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name, int value)
    {
        return withIntParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name,
                                    unsigned int value)
    {
        return withUnsignedIntParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name, long int value)
    {
        return withLongIntParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name,
                                    unsigned long int value)
    {
        return withUnsignedLongIntParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name,
                                    cpputest_longlong value)
    {
        return withLongLongIntParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name,
                                    cpputest_ulonglong value)
    {
        return withUnsignedLongLongIntParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name, double value)
    {
        return withDoubleParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name, double value,
                                    double tolerance)
    {
        return withDoubleParameterAndTolerance(name, value, tolerance);
    }
    MockExpectedCall &withParameter(const SimpleString &name, const char *value)
    {
        return withStringParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name, void *value)
    {
        return withPointerParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name, const void *value)
    {
        return withConstPointerParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name, void (*value)())
    {
        return withFunctionPointerParameter(name, value);
    }
    MockExpectedCall &withParameter(const SimpleString &name,
                                    const unsigned char *value, size_t size)
    {
        return withMemoryBufferParameter(name, value, size);
    }

    virtual MockExpectedCall &withBoolParameter(const SimpleString &name,
                                                bool value)
    {
        return addParam(name, CppUMockBool(value));
    }
    virtual MockExpectedCall &withIntParameter(const SimpleString &name,
                                               int value)
    {
        return addParam(name, CppUMockInt(value));
    }
    virtual MockExpectedCall &withUnsignedIntParameter(const SimpleString &name,
                                                       unsigned int value)
    {
        return addParam(name, CppUMockUInt(value));
    }
    virtual MockExpectedCall &withLongIntParameter(const SimpleString &name,
                                                   long int value)
    {
        return addParam(name, CppUMockLong(value));
    }
    virtual MockExpectedCall &
    withUnsignedLongIntParameter(const SimpleString &name,
                                 unsigned long int value)
    {
        return addParam(name, CppUMockULong(value));
    }
    virtual MockExpectedCall &withLongLongIntParameter(const SimpleString &name,
                                                       cpputest_longlong value)
    {
        return addParam(name, CppUMockLongLong(value));
    }
    virtual MockExpectedCall &
    withUnsignedLongLongIntParameter(const SimpleString &name,
                                     cpputest_ulonglong value)
    {
        return addParam(name, CppUMockULongLong(value));
    }
    virtual MockExpectedCall &withDoubleParameter(const SimpleString &name,
                                                  double value)
    {
        return addParam(
            name, CppUMockDouble(value, MOCK_SUPPORT_DEFAULT_DOUBLE_TOLERANCE));
    }
    /* upstream spells the tolerance form as an overload of
     * withDoubleParameter (MockExpectedCall.h:74); the AndTolerance alias
     * predates that discovery and stays for source compatibility */
    virtual MockExpectedCall &withDoubleParameter(const SimpleString &name,
                                                  double value,
                                                  double tolerance)
    {
        return addParam(name, CppUMockDouble(value, tolerance));
    }
    MockExpectedCall &withDoubleParameterAndTolerance(const SimpleString &name,
                                                      double value,
                                                      double tolerance)
    {
        return withDoubleParameter(name, value, tolerance);
    }
    virtual MockExpectedCall &withStringParameter(const SimpleString &name,
                                                  const char *value)
    {
        return addParam(name, CppUMockString(value));
    }
    virtual MockExpectedCall &withPointerParameter(const SimpleString &name,
                                                   void *value)
    {
        return addParam(name, CppUMockPointer(value));
    }
    virtual MockExpectedCall &
    withConstPointerParameter(const SimpleString &name, const void *value)
    {
        return addParam(name, CppUMockConstPointer(value));
    }
    virtual MockExpectedCall &
    withFunctionPointerParameter(const SimpleString &name, void (*value)())
    {
        return addParam(name, CppUMockFunctionPointer(value));
    }
    virtual MockExpectedCall &
    withMemoryBufferParameter(const SimpleString &name,
                              const unsigned char *value, size_t size)
    {
        return addParam(name, CppUMockMemBuffer(value, size));
    }

    virtual MockExpectedCall &ignoreOtherParameters()
    {
        cum_expectation_ignore_other_parameters(handle_);
        return *this;
    }

    virtual MockExpectedCall &onObject(void *objectPtr)
    {
        cum_expectation_on_object(handle_, objectPtr);
        return *this;
    }

    virtual MockExpectedCall &withName(const SimpleString &name)
    {
        cum_expectation_set_name(handle_, name.asCharString());
        return *this;
    }

    virtual MockExpectedCall &withParameterOfType(const SimpleString &type,
                                                  const SimpleString &name,
                                                  const void *value)
    {
        return addParam(name, CppUMockConstObject(type, value));
    }

    virtual MockExpectedCall &
    withOutputParameterReturning(const SimpleString &name, const void *value,
                                 size_t size)
    {
        cum_expectation_with_output_parameter(handle_, name.asCharString(),
                                              value, size);
        return *this;
    }

    virtual MockExpectedCall &withOutputParameterOfTypeReturning(
        const SimpleString &type, const SimpleString &name, const void *value)
    {
        cum_expectation_with_output_parameter_of_type(
            handle_, type.asCharString(), name.asCharString(), value);
        return *this;
    }

    virtual MockExpectedCall &
    withUnmodifiedOutputParameter(const SimpleString &name)
    {
        cum_expectation_with_unmodified_output_parameter(handle_,
                                                         name.asCharString());
        return *this;
    }

    virtual MockExpectedCall &andReturnValue(bool value)
    {
        return setReturn(CppUMockBool(value));
    }
    virtual MockExpectedCall &andReturnValue(int value)
    {
        return setReturn(CppUMockInt(value));
    }
    virtual MockExpectedCall &andReturnValue(unsigned int value)
    {
        return setReturn(CppUMockUInt(value));
    }
    virtual MockExpectedCall &andReturnValue(long int value)
    {
        return setReturn(CppUMockLong(value));
    }
    virtual MockExpectedCall &andReturnValue(unsigned long int value)
    {
        return setReturn(CppUMockULong(value));
    }
    virtual MockExpectedCall &andReturnValue(cpputest_longlong value)
    {
        return setReturn(CppUMockLongLong(value));
    }
    virtual MockExpectedCall &andReturnValue(cpputest_ulonglong value)
    {
        return setReturn(CppUMockULongLong(value));
    }
    virtual MockExpectedCall &andReturnValue(double value)
    {
        return setReturn(CppUMockDouble(value, 0.0));
    }
    virtual MockExpectedCall &andReturnValue(const char *value)
    {
        return setReturn(CppUMockString(value));
    }
    virtual MockExpectedCall &andReturnValue(void *value)
    {
        return setReturn(CppUMockPointer(value));
    }
    virtual MockExpectedCall &andReturnValue(const void *value)
    {
        return setReturn(CppUMockConstPointer(value));
    }
    virtual MockExpectedCall &andReturnValue(void (*value)())
    {
        return setReturn(CppUMockFunctionPointer(value));
    }

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

    virtual MockActualCall &onObject(const void *objectPtr)
    {
        cum_actual_on_object(handle_, objectPtr);
        return *this;
    }

    virtual MockActualCall &withName(const SimpleString &name)
    {
        cum_actual_with_name(handle_, name.asCharString());
        return *this;
    }

    /* no-op on a checked call; recorded when tracing */
    virtual MockActualCall &withCallOrder(unsigned int order)
    {
        cum_actual_with_call_order(handle_, order);
        return *this;
    }

    MockActualCall &withParameter(const SimpleString &name, bool value)
    {
        return withBoolParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, int value)
    {
        return withIntParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, unsigned int value)
    {
        return withUnsignedIntParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, long int value)
    {
        return withLongIntParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name,
                                  unsigned long int value)
    {
        return withUnsignedLongIntParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name,
                                  cpputest_longlong value)
    {
        return withLongLongIntParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name,
                                  cpputest_ulonglong value)
    {
        return withUnsignedLongLongIntParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, double value)
    {
        return withDoubleParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, const char *value)
    {
        return withStringParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, void *value)
    {
        return withPointerParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, const void *value)
    {
        return withConstPointerParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name, void (*value)())
    {
        return withFunctionPointerParameter(name, value);
    }
    MockActualCall &withParameter(const SimpleString &name,
                                  const unsigned char *value, size_t size)
    {
        return withMemoryBufferParameter(name, value, size);
    }

    virtual MockActualCall &withBoolParameter(const SimpleString &name,
                                              bool value)
    {
        return addParam(name, CppUMockBool(value));
    }
    virtual MockActualCall &withIntParameter(const SimpleString &name,
                                             int value)
    {
        return addParam(name, CppUMockInt(value));
    }
    virtual MockActualCall &withUnsignedIntParameter(const SimpleString &name,
                                                     unsigned int value)
    {
        return addParam(name, CppUMockUInt(value));
    }
    virtual MockActualCall &withLongIntParameter(const SimpleString &name,
                                                 long int value)
    {
        return addParam(name, CppUMockLong(value));
    }
    virtual MockActualCall &
    withUnsignedLongIntParameter(const SimpleString &name,
                                 unsigned long int value)
    {
        return addParam(name, CppUMockULong(value));
    }
    virtual MockActualCall &withLongLongIntParameter(const SimpleString &name,
                                                     cpputest_longlong value)
    {
        return addParam(name, CppUMockLongLong(value));
    }
    virtual MockActualCall &
    withUnsignedLongLongIntParameter(const SimpleString &name,
                                     cpputest_ulonglong value)
    {
        return addParam(name, CppUMockULongLong(value));
    }
    virtual MockActualCall &withDoubleParameter(const SimpleString &name,
                                                double value)
    {
        return addParam(name, CppUMockDouble(value, 0.0));
    }
    virtual MockActualCall &withStringParameter(const SimpleString &name,
                                                const char *value)
    {
        return addParam(name, CppUMockString(value));
    }
    virtual MockActualCall &withPointerParameter(const SimpleString &name,
                                                 void *value)
    {
        return addParam(name, CppUMockPointer(value));
    }
    virtual MockActualCall &withConstPointerParameter(const SimpleString &name,
                                                      const void *value)
    {
        return addParam(name, CppUMockConstPointer(value));
    }
    virtual MockActualCall &
    withFunctionPointerParameter(const SimpleString &name, void (*value)())
    {
        return addParam(name, CppUMockFunctionPointer(value));
    }
    virtual MockActualCall &
    withMemoryBufferParameter(const SimpleString &name,
                              const unsigned char *value, size_t size)
    {
        return addParam(name, CppUMockMemBuffer(value, size));
    }

    virtual MockActualCall &withParameterOfType(const SimpleString &type,
                                                const SimpleString &name,
                                                const void *value)
    {
        /* checked calls only: MockIgnoredActualCall/Trace accept OfType
         * params without a comparator (upstream's check lives in
         * MockCheckedActualCall::withParameterOfType) */
        if (cum_actual_is_checked(handle_) &&
            !cum_has_comparator(type.asCharString())) {
            cum_fail_no_way_to_compare(type.asCharString());
            return *this;
        }
        return addParam(name, CppUMockConstObject(type, value));
    }

    virtual MockActualCall &withOutputParameter(const SimpleString &name,
                                                void *output)
    {
        cum_actual_with_output_parameter(handle_, name.asCharString(), output);
        return *this;
    }

    virtual MockActualCall &withOutputParameterOfType(const SimpleString &type,
                                                      const SimpleString &name,
                                                      void *output)
    {
        /* no copier fail-fast here: upstream raises MockNoWayToCopy at COPY
         * time, inside copyOutputParameters, only when a match completes
         * (MockActualCall.cpp:101) — the core handles it there */
        cum_actual_with_output_parameter_of_type(handle_, type.asCharString(),
                                                 name.asCharString(), output);
        return *this;
    }

    /* Return-value semantics mirror upstream exactly (MockActualCall.cpp).
     * hasReturnValue() is name-based: true for a queued value ("returnValue")
     * AND for an unmatched call ("no return value") — so OrDefault on an
     * unmatched call returns the int-0 empty value, not the default. An
     * ignored call (disabled mock / ignoreOtherCalls) short-circuits every
     * typed getter to zero without the counting type assert, like
     * MockIgnoredActualCall. */
    virtual bool hasReturnValue() { return !returnValue().getName().isEmpty(); }

    virtual MockNamedValue returnValue()
    {
        cum_value v;
        switch (cum_actual_return_value(handle_, &v)) {
        case CUM_RET_VALUE:
            return MockNamedValue("returnValue", v);
        case CUM_RET_UNMATCHED:
            return MockNamedValue("no return value");
        default:
            return MockNamedValue("");
        }
    }

    virtual bool returnBoolValue()
    {
        return ignored() ? false : returnValue().getBoolValue();
    }
    virtual bool returnBoolValueOrDefault(bool d)
    {
        return traced()           ? returnBoolValue()
               : hasReturnValue() ? returnBoolValue()
                                  : d;
    }
    virtual int returnIntValue()
    {
        return ignored() ? 0 : returnValue().getIntValue();
    }
    virtual int returnIntValueOrDefault(int d)
    {
        return traced()           ? returnIntValue()
               : hasReturnValue() ? returnIntValue()
                                  : d;
    }
    virtual unsigned int returnUnsignedIntValue()
    {
        return ignored() ? 0 : returnValue().getUnsignedIntValue();
    }
    virtual unsigned int returnUnsignedIntValueOrDefault(unsigned int d)
    {
        return traced()           ? returnUnsignedIntValue()
               : hasReturnValue() ? returnUnsignedIntValue()
                                  : d;
    }
    virtual long int returnLongIntValue()
    {
        return ignored() ? 0 : returnValue().getLongIntValue();
    }
    virtual long int returnLongIntValueOrDefault(long int d)
    {
        return traced()           ? returnLongIntValue()
               : hasReturnValue() ? returnLongIntValue()
                                  : d;
    }
    virtual unsigned long int returnUnsignedLongIntValue()
    {
        return ignored() ? 0 : returnValue().getUnsignedLongIntValue();
    }
    virtual unsigned long int
    returnUnsignedLongIntValueOrDefault(unsigned long int d)
    {
        return traced()           ? returnUnsignedLongIntValue()
               : hasReturnValue() ? returnUnsignedLongIntValue()
                                  : d;
    }
    virtual cpputest_longlong returnLongLongIntValue()
    {
        return ignored() ? 0 : returnValue().getLongLongIntValue();
    }
    virtual cpputest_longlong
    returnLongLongIntValueOrDefault(cpputest_longlong d)
    {
        return traced()           ? returnLongLongIntValue()
               : hasReturnValue() ? returnLongLongIntValue()
                                  : d;
    }
    virtual cpputest_ulonglong returnUnsignedLongLongIntValue()
    {
        return ignored() ? 0 : returnValue().getUnsignedLongLongIntValue();
    }
    virtual cpputest_ulonglong
    returnUnsignedLongLongIntValueOrDefault(cpputest_ulonglong d)
    {
        return traced()           ? returnUnsignedLongLongIntValue()
               : hasReturnValue() ? returnUnsignedLongLongIntValue()
                                  : d;
    }
    virtual double returnDoubleValue()
    {
        return ignored() ? 0.0 : returnValue().getDoubleValue();
    }
    virtual double returnDoubleValueOrDefault(double d)
    {
        return traced()           ? returnDoubleValue()
               : hasReturnValue() ? returnDoubleValue()
                                  : d;
    }
    virtual const char *returnStringValue()
    {
        return ignored() ? "" : returnValue().getStringValue();
    }
    virtual const char *returnStringValueOrDefault(const char *d)
    {
        return traced()           ? returnStringValue()
               : hasReturnValue() ? returnStringValue()
                                  : d;
    }
    virtual void *returnPointerValue()
    {
        return ignored() ? 0 : returnValue().getPointerValue();
    }
    virtual void *returnPointerValueOrDefault(void *d)
    {
        return traced()           ? returnPointerValue()
               : hasReturnValue() ? returnPointerValue()
                                  : d;
    }
    virtual const void *returnConstPointerValue()
    {
        return ignored() ? 0 : returnValue().getConstPointerValue();
    }
    virtual const void *returnConstPointerValueOrDefault(const void *d)
    {
        return traced()           ? returnConstPointerValue()
               : hasReturnValue() ? returnConstPointerValue()
                                  : d;
    }
    virtual void (*returnFunctionPointerValue())()
    {
        return ignored() ? 0 : returnValue().getFunctionPointerValue();
    }
    virtual void (*returnFunctionPointerValueOrDefault(void (*d)()))()
    {
        return traced()           ? returnFunctionPointerValue()
               : hasReturnValue() ? returnFunctionPointerValue()
                                  : d;
    }

  private:
    int retState()
    {
        cum_value v;
        return cum_actual_return_value(handle_, &v);
    }
    /* ignored AND traced calls zero the plain typed getters */
    bool ignored()
    {
        int st = retState();
        return st == CUM_RET_IGNORED || st == CUM_RET_TRACED;
    }
    /* MockActualCallTrace also zeroes the OrDefault getters (the default
     * is ignored); MockIgnoredActualCall returns the default */
    bool traced() { return retState() == CUM_RET_TRACED; }

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

    virtual void expectNoCall(const SimpleString &functionName)
    {
        expectNCalls(0, functionName);
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
        int st = cum_scope_return_value(scope_, &v);
        return st == CUM_RET_VALUE || st == CUM_RET_UNMATCHED;
    }

    virtual MockNamedValue returnValue()
    {
        cum_value v;
        switch (cum_scope_return_value(scope_, &v)) {
        case CUM_RET_VALUE:
            return MockNamedValue("returnValue", v);
        case CUM_RET_UNMATCHED:
            return MockNamedValue("no return value");
        default:
            return MockNamedValue("");
        }
    }

    virtual bool boolReturnValue() { return returnValue().getBoolValue(); }
    virtual int intReturnValue() { return returnValue().getIntValue(); }
    virtual unsigned int unsignedIntReturnValue()
    {
        return returnValue().getUnsignedIntValue();
    }
    virtual long int longIntReturnValue()
    {
        return returnValue().getLongIntValue();
    }
    virtual unsigned long int unsignedLongIntReturnValue()
    {
        return returnValue().getUnsignedLongIntValue();
    }
    virtual cpputest_longlong longLongIntReturnValue()
    {
        return returnValue().getLongLongIntValue();
    }
    virtual cpputest_ulonglong unsignedLongLongIntReturnValue()
    {
        return returnValue().getUnsignedLongLongIntValue();
    }
    virtual const char *stringReturnValue()
    {
        return returnValue().getStringValue();
    }
    virtual double doubleReturnValue()
    {
        return returnValue().getDoubleValue();
    }
    virtual void *pointerReturnValue()
    {
        return returnValue().getPointerValue();
    }
    virtual const void *constPointerReturnValue()
    {
        return returnValue().getConstPointerValue();
    }
    virtual void (*functionPointerReturnValue())()
    {
        return returnValue().getFunctionPointerValue();
    }

    virtual bool returnBoolValueOrDefault(bool d)
    {
        return hasReturnValue() ? boolReturnValue() : d;
    }
    virtual int returnIntValueOrDefault(int d)
    {
        return hasReturnValue() ? intReturnValue() : d;
    }
    virtual unsigned int returnUnsignedIntValueOrDefault(unsigned int d)
    {
        return hasReturnValue() ? unsignedIntReturnValue() : d;
    }
    virtual long int returnLongIntValueOrDefault(long int d)
    {
        return hasReturnValue() ? longIntReturnValue() : d;
    }
    virtual unsigned long int
    returnUnsignedLongIntValueOrDefault(unsigned long int d)
    {
        return hasReturnValue() ? unsignedLongIntReturnValue() : d;
    }
    virtual cpputest_longlong
    returnLongLongIntValueOrDefault(cpputest_longlong d)
    {
        return hasReturnValue() ? longLongIntReturnValue() : d;
    }
    virtual cpputest_ulonglong
    returnUnsignedLongLongIntValueOrDefault(cpputest_ulonglong d)
    {
        return hasReturnValue() ? unsignedLongLongIntReturnValue() : d;
    }
    virtual const char *returnStringValueOrDefault(const char *d)
    {
        return hasReturnValue() ? stringReturnValue() : d;
    }
    virtual double returnDoubleValueOrDefault(double d)
    {
        return hasReturnValue() ? doubleReturnValue() : d;
    }
    virtual void *returnPointerValueOrDefault(void *d)
    {
        return hasReturnValue() ? pointerReturnValue() : d;
    }
    virtual const void *returnConstPointerValueOrDefault(const void *d)
    {
        return hasReturnValue() ? constPointerReturnValue() : d;
    }
    virtual void (*returnFunctionPointerValueOrDefault(void (*d)()))()
    {
        return hasReturnValue() ? functionPointerReturnValue() : d;
    }

    /* data store */
    virtual bool hasData(const SimpleString &name)
    {
        return cum_scope_has_data(scope_, name.asCharString()) != 0;
    }

    virtual void setData(const SimpleString &name, bool value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockBool(value));
    }
    virtual void setData(const SimpleString &name, int value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockInt(value));
    }
    virtual void setData(const SimpleString &name, unsigned int value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockUInt(value));
    }
    virtual void setData(const SimpleString &name, long int value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockLong(value));
    }
    virtual void setData(const SimpleString &name, unsigned long int value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockULong(value));
    }
    virtual void setData(const SimpleString &name, const char *value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockString(value));
    }
    virtual void setData(const SimpleString &name, double value)
    {
        cum_scope_set_data(scope_, name.asCharString(),
                           CppUMockDouble(value, 0.0));
    }
    virtual void setData(const SimpleString &name, void *value)
    {
        cum_scope_set_data(scope_, name.asCharString(), CppUMockPointer(value));
    }
    virtual void setData(const SimpleString &name, const void *value)
    {
        cum_scope_set_data(scope_, name.asCharString(),
                           CppUMockConstPointer(value));
    }
    virtual void setData(const SimpleString &name, void (*value)())
    {
        cum_scope_set_data(scope_, name.asCharString(),
                           CppUMockFunctionPointer(value));
    }

    virtual void setDataObject(const SimpleString &name,
                               const SimpleString &type, void *value)
    {
        cum_scope_set_data(scope_, name.asCharString(),
                           CppUMockObject(type, value));
    }

    virtual void setDataConstObject(const SimpleString &name,
                                    const SimpleString &type, const void *value)
    {
        cum_scope_set_data(scope_, name.asCharString(),
                           CppUMockConstObject(type, value));
    }

    virtual MockNamedValue getData(const SimpleString &name)
    {
        cum_value v;
        int has = cum_scope_get_data(scope_, name.asCharString(), &v);
        return has ? MockNamedValue(name, v) : MockNamedValue("");
    }

    virtual void installComparator(const SimpleString &typeName,
                                   MockNamedValueComparator &comparator)
    {
        cum_install_comparator(typeName.asCharString(), &comparator,
                               CppUMockComparatorEqual,
                               CppUMockComparatorToString);
    }

    virtual void installCopier(const SimpleString &typeName,
                               MockNamedValueCopier &copier)
    {
        cum_install_copier(typeName.asCharString(), &copier,
                           CppUMockCopierCopy);
    }

    virtual void installComparatorsAndCopiers(
        const MockNamedValueComparatorsAndCopiersRepository &repository)
    {
        /* the core registry is global, so this covers child scopes like
         * upstream's recursion over data_-stored children */
        typedef MockNamedValueComparatorsAndCopiersRepository Repo;
        for (Repo::Node *n = repository.head_; n; n = n->next) {
            if (n->comparator)
                installComparator(n->name, *n->comparator);
            if (n->copier)
                installCopier(n->name, *n->copier);
        }
    }

    virtual void removeAllComparatorsAndCopiers()
    {
        cum_remove_all_comparators_and_copiers();
    }

    /* accepted for source compatibility; custom reporters are inert (see
     * MockFailureReporter forward declaration above) */
    virtual void setMockFailureStandardReporter(MockFailureReporter *reporter)
    {
        (void)reporter;
    }
    virtual void setActiveReporter(MockFailureReporter *reporter)
    {
        (void)reporter;
    }

    /* defined after mock() below */
    virtual MockSupport *getMockSupportScope(const SimpleString &name);

    /* like clear: the global mock recurses over children, a named scope
     * checks/counts only itself (upstream children live in the global's
     * data store; named scopes have none) */
    virtual void checkExpectations()
    {
        if (cum_scope_name(scope_)[0] == '\0')
            cum_check_expectations_all();
        else
            cum_check_expectations_scope(scope_);
    }
    virtual void clear()
    {
        /* upstream: the GLOBAL clear deletes all child scopes; a child's
         * clear wipes only its own state (the child survives) */
        if (cum_scope_name(scope_)[0] == '\0')
            cum_clear_all();
        else
            cum_clear_scope(scope_);
    }
    virtual bool expectedCallsLeft()
    {
        if (cum_scope_name(scope_)[0] == '\0')
            return cum_expected_calls_left_all() != 0;
        return cum_expected_calls_left_scope(scope_) != 0;
    }
    virtual void ignoreOtherCalls() { cum_ignore_other_calls(scope_); }
    virtual void tracing(bool enabled)
    {
        cum_set_tracing(scope_, enabled ? 1 : 0);
    }
    virtual const char *getTraceOutput() { return cum_trace_output(); }
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

    static void ensureFacadeFreeInstalled() { cum_set_facade_free(freeFacade); }

    cum_scope *scope_;
    MockActualCall actualCall_;
};

inline MockSupport &
mock(const SimpleString &mockName = "",
     MockFailureReporter * /*failureReporterForThisCall*/ = NULLPTR)
{
    /* one facade per scope, kept for the process lifetime */
    struct ScopeNode {
        SimpleString name;
        MockSupport *support;
        ScopeNode *next;
    };
    static ScopeNode *head = NULLPTR;

    /* every access goes through the core scope lookup: it revives scopes
     * deleted by a global clear (upstream re-clones them) and reproduces
     * upstream's existing-scope sanity check counting */
    cum_scope *scope = cum_scope_get(mockName.asCharString());

    for (ScopeNode *n = head; n; n = n->next)
        if (n->name == mockName)
            return *n->support;

    /* process-lifetime singletons: keep them out of leak accounting, like
     * upstream's saveAndDisableNewDeleteOverloads around mock internals */
    cu_mem_save_and_disable_tracking();
    ScopeNode *n = new ScopeNode;
    n->name = mockName;
    n->support = new MockSupport(scope);
    n->next = head;
    head = n;
    cu_mem_restore_tracking();
    return *n->support;
}

/* upstream getMockSupportScope: fetch-or-create the named child scope's
 * support (the scope registry is global, so `this` is immaterial) */
inline MockSupport *MockSupport::getMockSupportScope(const SimpleString &name)
{
    return &mock(name);
}

#endif
