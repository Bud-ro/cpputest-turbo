/* differential probe: SimpleString utility surface vs upstream */
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

TEST_GROUP(SS)
{
};

TEST(SS, findAndCount)
{
    SimpleString s("hello world, hello");
    LONGS_EQUAL(4, (long)s.find('o'));
    LONGS_EQUAL(7, (long)s.findFrom(5, 'o'));
    CHECK(s.find('z') == SimpleString::npos);
    LONGS_EQUAL(2, (long)s.count("hello"));
    LONGS_EQUAL(3, (long)s.count("l"));
    SimpleString aa("aaaa");
    LONGS_EQUAL(3, (long)aa.count("aa")); /* overlapping count */
}

TEST(SS, replaceCharAndString)
{
    SimpleString s("a-b-c");
    s.replace('-', '+');
    STRCMP_EQUAL("a+b+c", s.asCharString());
    SimpleString t("one two two");
    t.replace("two", "2");
    STRCMP_EQUAL("one 2 2", t.asCharString());
    SimpleString u("grow");
    u.replace("o", "ooo");
    STRCMP_EQUAL("grooow", u.asCharString());
}

TEST(SS, caseHelpers)
{
    SimpleString s("MiXeD");
    STRCMP_EQUAL("mixed", s.lowerCase().asCharString());
    CHECK(s.equalsNoCase("mIxEd"));
    CHECK(SimpleString("Hello World").containsNoCase("WORLD"));
    CHECK_FALSE(SimpleString("abc").containsNoCase("xyz"));
}

TEST(SS, splitTokens)
{
    SimpleStringCollection col;
    SimpleString s("a,b,c");
    s.split(",", col);
    LONGS_EQUAL(3, (long)col.size());
    STRCMP_EQUAL("a,", col[0].asCharString());
    STRCMP_EQUAL("b,", col[1].asCharString());
    STRCMP_EQUAL("c", col[2].asCharString());
    SimpleString t("x;y;");
    t.split(";", col);
    LONGS_EQUAL(2, (long)col.size());
    STRCMP_EQUAL("x;", col[0].asCharString());
    STRCMP_EQUAL("y;", col[1].asCharString());
}

TEST(SS, subStringFromTillAndBuffer)
{
    SimpleString s("name=<value>;");
    STRCMP_EQUAL("<value", s.subStringFromTill('<', '>').asCharString());
    STRCMP_EQUAL("=<value>;", s.subStringFromTill('=', '!').asCharString());
    char buf[6];
    s.copyToBuffer(buf, sizeof buf);
    STRCMP_EQUAL("name=", buf);
}

TEST(SS, statics)
{
    LONGS_EQUAL(-42, SimpleString::AtoI("  -42abc"));
    LONGS_EQUAL(42, SimpleString::AtoI("+42"));
    LONGS_EQUAL(0, SimpleString::AtoI("abc"));
    UNSIGNED_LONGS_EQUAL(77u, SimpleString::AtoU(" 77x"));
    CHECK(SimpleString::StrCmp("a", "a") == 0);
    LONGS_EQUAL(3, (long)SimpleString::StrLen("abc"));
    CHECK(SimpleString::StrNCmp("abcd", "abXX", 2) == 0);
    STRCMP_EQUAL("cd", SimpleString::StrStr("abcd", "cd"));
    CHECK('a' == SimpleString::ToLower('A'));
    CHECK(SimpleString::MemCmp("ab", "ab", 2) == 0);
}

TEST(SS, formatters)
{
    STRCMP_EQUAL("1st", StringFromOrdinalNumber(1).asCharString());
    STRCMP_EQUAL("2nd", StringFromOrdinalNumber(2).asCharString());
    STRCMP_EQUAL("3rd", StringFromOrdinalNumber(3).asCharString());
    STRCMP_EQUAL("4th", StringFromOrdinalNumber(4).asCharString());
    STRCMP_EQUAL("11th", StringFromOrdinalNumber(11).asCharString());
    STRCMP_EQUAL("12th", StringFromOrdinalNumber(12).asCharString());
    STRCMP_EQUAL("13th", StringFromOrdinalNumber(13).asCharString());
    STRCMP_EQUAL("21st", StringFromOrdinalNumber(21).asCharString());
    STRCMP_EQUAL("ab 7", StringFromFormat("%s %d", "ab", 7).asCharString());
    /* lite: no std::string interop */
}

CPPUTEST_DEFAULT_MAIN
