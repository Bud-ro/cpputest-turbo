#ifndef D_MockSupportPlugin_h
#define D_MockSupportPlugin_h

/* cpputest-revibed: MockSupportPlugin — upstream semantics:
 *  - comparators/copiers registered on the plugin are installed into mock()
 *    before EVERY test and removed after it
 *  - postTestAction checks expectations only when the test hasn't already
 *    failed (no second failure piled onto a failing test), then clears
 *
 * Registrations are stored inline (no heap) so the plugin never interacts
 * with the leak detector regardless of when it is constructed. */

#include "CppUTest/TestPlugin.h"
#include "CppUTestExt/MockSupport.h"

class MockSupportPlugin : public TestPlugin
{
  public:
    MockSupportPlugin(const SimpleString &name = "MockSupportPlugin")
        : TestPlugin(name), count_(0)
    {
    }

    virtual ~MockSupportPlugin() {}

    virtual void installComparator(const SimpleString &name,
                                   MockNamedValueComparator &comparator)
    {
        addEntry(name, &comparator, NULLPTR);
    }

    virtual void installCopier(const SimpleString &name,
                               MockNamedValueCopier &copier)
    {
        addEntry(name, NULLPTR, &copier);
    }

    virtual void clear() { count_ = 0; }

    virtual void preTestAction(UtestShell &, TestResult &) CPPUTEST_OVERRIDE
    {
        for (int i = 0; i < count_; i++) {
            if (entries_[i].comparator)
                mock().installComparator(entries_[i].name,
                                         *entries_[i].comparator);
            else
                mock().installCopier(entries_[i].name, *entries_[i].copier);
        }
    }

    virtual void postTestAction(UtestShell &test,
                                TestResult &) CPPUTEST_OVERRIDE
    {
        if (!test.hasFailed())
            mock().checkExpectations();
        mock().clear();
        mock().removeAllComparatorsAndCopiers();
    }

  private:
    enum { kMaxEntries = 32, kMaxName = 63 };

    struct Entry {
        char name[kMaxName + 1];
        MockNamedValueComparator *comparator;
        MockNamedValueCopier *copier;
    };

    void addEntry(const SimpleString &name,
                  MockNamedValueComparator *comparator,
                  MockNamedValueCopier *copier)
    {
        if (count_ >= kMaxEntries)
            return;
        const char *s = name.asCharString();
        size_t i = 0;
        for (; s[i] && i < (size_t)kMaxName; i++)
            entries_[count_].name[i] = s[i];
        entries_[count_].name[i] = '\0';
        entries_[count_].comparator = comparator;
        entries_[count_].copier = copier;
        count_++;
    }

    Entry entries_[kMaxEntries];
    int count_;
};

#endif
