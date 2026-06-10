#ifndef D_SimpleString_h
#define D_SimpleString_h

/* cpputest-revibed: SimpleString shim. Method bodies live in the library
 * (src/shim/simplestring.cpp) so user translation units only parse
 * declarations — that keeps per-TU compile cost down. Only the trivial
 * accessors stay inline. */

#include "CppUTest/CppUTestConfig.h"
#include <cpputest_core/core.h>

#include <stdarg.h>

#if CPPUTEST_USE_STD_CPP_LIB
#include <string>
#endif

class SimpleStringCollection;

class SimpleString
{
    friend bool operator==(const SimpleString &left, const SimpleString &right);
    friend bool operator!=(const SimpleString &left, const SimpleString &right);

public:
    SimpleString(const char *value = "");
    SimpleString(const char *value, size_t repeatCount);
    SimpleString(const SimpleString &other);
    SimpleString &operator=(const SimpleString &other);
    ~SimpleString();

    const char *asCharString() const { return buffer_; }
    size_t size() const { return len_; }
    bool isEmpty() const { return len_ == 0; }
    char at(size_t pos) const { return buffer_[pos]; }

    static const size_t npos = (size_t)-1;

    bool contains(const SimpleString &other) const;
    bool containsNoCase(const SimpleString &other) const;
    bool startsWith(const SimpleString &other) const;
    bool endsWith(const SimpleString &other) const;
    bool equalsNoCase(const SimpleString &str) const;
    SimpleString subString(size_t beginPos, size_t amount) const;
    SimpleString subString(size_t beginPos) const;
    SimpleString subStringFromTill(char startChar, char lastExcludedChar) const;

    size_t find(char ch) const;
    size_t findFrom(size_t starting_position, char ch) const;
    size_t count(const SimpleString &str) const;
    void split(const SimpleString &delimiter, SimpleStringCollection &col) const;

    void replace(char to, char with);
    void replace(const char *to, const char *with);
    SimpleString lowerCase() const;
    void copyToBuffer(char *buffer, size_t bufferSize) const;

    static int AtoI(const char *str);
    static unsigned AtoU(const char *str);
    static int StrCmp(const char *s1, const char *s2);
    static size_t StrLen(const char *str);
    static int StrNCmp(const char *s1, const char *s2, size_t n);
    static char *StrNCpy(char *s1, const char *s2, size_t n);
    static const char *StrStr(const char *s1, const char *s2);
    static char ToLower(char ch);
    static int MemCmp(const void *s1, const void *s2, size_t n);

    SimpleString &operator+=(const SimpleString &other);
    SimpleString &operator+=(const char *other);
    SimpleString operator+(const SimpleString &other) const;

    SimpleString printable() const;

    static void padStringsToSameLength(SimpleString &str1, SimpleString &str2,
                                       char padCharacter);

private:
    void copyFrom(const char *value);
    void append(const char *value, size_t n);

    char *buffer_;
    size_t len_;
};

/* free operator forms so a const char* converts on EITHER side */
bool operator==(const SimpleString &left, const SimpleString &right);
bool operator!=(const SimpleString &left, const SimpleString &right);

class SimpleStringCollection
{
public:
    SimpleStringCollection();
    ~SimpleStringCollection();

    void allocate(size_t size);

    size_t size() const;
    SimpleString &operator[](size_t index);

private:
    SimpleString *collection_;
    SimpleString empty_;
    size_t size_;

    void operator=(SimpleStringCollection &);
    SimpleStringCollection(SimpleStringCollection &);
};

SimpleString StringFrom(bool value);
SimpleString StringFrom(const char *value);
SimpleString StringFrom(char value);
SimpleString StringFrom(int value);
SimpleString StringFrom(unsigned int value);
SimpleString StringFrom(long value);
SimpleString StringFrom(unsigned long value);
SimpleString StringFrom(cpputest_longlong value);
SimpleString StringFrom(cpputest_ulonglong value);
SimpleString StringFrom(double value, int precision = 6);
SimpleString StringFrom(const SimpleString &value);
SimpleString StringFrom(const void *value);
SimpleString StringFrom(void (*value)());
#if defined(__cplusplus) && __cplusplus >= 201103L
SimpleString StringFrom(decltype(nullptr));
#endif

SimpleString StringFromOrNull(const char *expected);
SimpleString PrintableStringFromOrNull(const char *expected);

#if CPPUTEST_USE_STD_CPP_LIB
SimpleString StringFrom(const std::string &value);
#endif

SimpleString StringFromFormat(const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
SimpleString VStringFromFormat(const char *format, va_list args);
SimpleString StringFromOrdinalNumber(unsigned int number);

SimpleString HexStringFrom(unsigned long value);
SimpleString HexStringFrom(long value);
SimpleString HexStringFrom(unsigned int value);
SimpleString HexStringFrom(int value);
SimpleString HexStringFrom(signed char value);
SimpleString HexStringFrom(cpputest_ulonglong value);
SimpleString HexStringFrom(cpputest_longlong value);
SimpleString HexStringFrom(const void *value);
SimpleString HexStringFrom(void (*value)());

SimpleString BracketsFormattedHexString(SimpleString hexString);
SimpleString BracketsFormattedHexStringFrom(int value);
SimpleString BracketsFormattedHexStringFrom(unsigned int value);
SimpleString BracketsFormattedHexStringFrom(long value);
SimpleString BracketsFormattedHexStringFrom(unsigned long value);
SimpleString BracketsFormattedHexStringFrom(signed char value);
SimpleString BracketsFormattedHexStringFrom(cpputest_longlong value);
SimpleString BracketsFormattedHexStringFrom(cpputest_ulonglong value);

SimpleString StringFromBinary(const unsigned char *value, size_t size);
SimpleString StringFromBinaryOrNull(const unsigned char *value, size_t size);
SimpleString StringFromBinaryWithSize(const unsigned char *value, size_t size);
SimpleString StringFromBinaryWithSizeOrNull(const unsigned char *value, size_t size);
SimpleString StringFromMaskedBits(unsigned long value, unsigned long mask,
                                  size_t byteCount);

#endif
