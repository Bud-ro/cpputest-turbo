#ifndef D_SimpleString_h
#define D_SimpleString_h

/* cpputest-revibed: minimal SimpleString shim. Covers the public surface
 * user code touches (StringFrom overloads, concatenation, comparison,
 * asCharString). Rendering rules delegate to the C core so failure formats
 * stay byte-identical. Deliberately small: this is an interface-compat
 * class, not upstream's allocation-cached string library. */

#include "CppUTest/CppUTestConfig.h"
#include <cpputest_core/core.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class SimpleString
{
public:
    SimpleString(const char *value = "")
    {
        copyFrom(value ? value : "");
    }

    SimpleString(const char *value, size_t repeatCount)
    {
        size_t unit = value ? strlen(value) : 0;
        len_ = unit * repeatCount;
        buffer_ = (char *)malloc(len_ + 1);
        for (size_t i = 0; i < repeatCount; i++)
            memcpy(buffer_ + i * unit, value, unit);
        buffer_[len_] = '\0';
    }

    SimpleString(const SimpleString &other)
    {
        copyFrom(other.buffer_);
    }

    SimpleString &operator=(const SimpleString &other)
    {
        if (this != &other) {
            free(buffer_);
            copyFrom(other.buffer_);
        }
        return *this;
    }

    ~SimpleString()
    {
        free(buffer_);
    }

    const char *asCharString() const { return buffer_; }
    size_t size() const { return len_; }
    bool isEmpty() const { return len_ == 0; }

    char at(size_t pos) const { return buffer_[pos]; }

    bool contains(const SimpleString &other) const
    {
        return NULL != strstr(buffer_, other.buffer_);
    }

    bool startsWith(const SimpleString &other) const
    {
        return 0 == strncmp(buffer_, other.buffer_, other.len_);
    }

    bool endsWith(const SimpleString &other) const
    {
        return len_ >= other.len_ &&
               0 == strcmp(buffer_ + len_ - other.len_, other.buffer_);
    }

    SimpleString subString(size_t beginPos, size_t amount) const
    {
        if (beginPos > len_)
            return "";
        SimpleString result;
        size_t avail = len_ - beginPos;
        size_t take = avail < amount ? avail : amount;
        result.append(buffer_ + beginPos, take);
        return result;
    }

    SimpleString subString(size_t beginPos) const
    {
        return subString(beginPos, beginPos > len_ ? 0 : len_ - beginPos);
    }

    SimpleString &operator+=(const SimpleString &other)
    {
        append(other.buffer_, other.len_);
        return *this;
    }

    SimpleString &operator+=(const char *other)
    {
        if (other)
            append(other, strlen(other));
        return *this;
    }

    SimpleString operator+(const SimpleString &other) const
    {
        SimpleString result(*this);
        result += other;
        return result;
    }

    bool operator==(const SimpleString &other) const
    {
        return 0 == strcmp(buffer_, other.buffer_);
    }

    bool operator!=(const SimpleString &other) const
    {
        return !(*this == other);
    }

    SimpleString printable() const
    {
        char *p = cu_str_printable(buffer_);
        SimpleString result(p);
        cu_str_free(p);
        return result;
    }

    static void padStringsToSameLength(SimpleString &str1, SimpleString &str2,
                                       char padCharacter)
    {
        if (str1.size() > str2.size()) {
            padStringsToSameLength(str2, str1, padCharacter);
            return;
        }
        char pad[2] = { padCharacter, '\0' };
        str1 = SimpleString(pad, str2.size() - str1.size()) + str1;
    }

private:
    void copyFrom(const char *value)
    {
        len_ = strlen(value);
        buffer_ = (char *)malloc(len_ + 1);
        memcpy(buffer_, value, len_ + 1);
    }

    void append(const char *value, size_t n)
    {
        buffer_ = (char *)realloc(buffer_, len_ + n + 1);
        memcpy(buffer_ + len_, value, n);
        len_ += n;
        buffer_[len_] = '\0';
    }

    char *buffer_;
    size_t len_;
};

/* ----------------------------- StringFrom ------------------------------- */

inline SimpleString takeOwnershipOfCuString(char *s)
{
    SimpleString result(s);
    cu_str_free(s);
    return result;
}

inline SimpleString StringFrom(bool value) { return SimpleString(value ? "true" : "false"); }
inline SimpleString StringFrom(const char *value) { return SimpleString(value); }
inline SimpleString StringFrom(char value) { return takeOwnershipOfCuString(cu_str_printf("%c", value)); }
inline SimpleString StringFrom(int value) { return takeOwnershipOfCuString(cu_str_printf("%d", value)); }
inline SimpleString StringFrom(unsigned int value) { return takeOwnershipOfCuString(cu_str_printf("%u", value)); }
inline SimpleString StringFrom(long value) { return takeOwnershipOfCuString(cu_str_printf("%ld", value)); }
inline SimpleString StringFrom(unsigned long value) { return takeOwnershipOfCuString(cu_str_printf("%lu", value)); }
inline SimpleString StringFrom(cpputest_longlong value) { return takeOwnershipOfCuString(cu_str_printf("%lld", value)); }
inline SimpleString StringFrom(cpputest_ulonglong value) { return takeOwnershipOfCuString(cu_str_printf("%llu", value)); }
inline SimpleString StringFrom(double value, int precision = 6) { return takeOwnershipOfCuString(cu_str_from_double(value, precision)); }
inline SimpleString StringFrom(const SimpleString &value) { return value; }
inline SimpleString StringFrom(const void *value) { return takeOwnershipOfCuString(cu_str_printf("0x%llx", (cpputest_ulonglong)(size_t)value)); }
inline SimpleString StringFrom(void (*value)()) { return takeOwnershipOfCuString(cu_str_printf("0x%llx", (cpputest_ulonglong)(size_t)value)); }
#if defined(__cplusplus) && __cplusplus >= 201103L
inline SimpleString StringFrom(decltype(nullptr)) { return "(null)"; }
#endif

inline SimpleString StringFromOrNull(const char *expected)
{
    return expected ? StringFrom(expected) : StringFrom("(null)");
}

inline SimpleString PrintableStringFromOrNull(const char *expected)
{
    return expected ? StringFrom(expected).printable() : StringFrom("(null)");
}

inline SimpleString StringFromFormat(const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

inline SimpleString StringFromFormat(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(NULLPTR, 0, format, copy);
    va_end(copy);
    char *buf = (char *)malloc(n > 0 ? (size_t)n + 1 : 1);
    if (n > 0)
        vsnprintf(buf, (size_t)n + 1, format, args);
    else
        buf[0] = '\0';
    va_end(args);
    SimpleString result(buf);
    free(buf);
    return result;
}

inline SimpleString HexStringFrom(unsigned long value) { return takeOwnershipOfCuString(cu_str_printf("%lx", value)); }
inline SimpleString HexStringFrom(long value) { return HexStringFrom((unsigned long)value); }
inline SimpleString HexStringFrom(unsigned int value) { return takeOwnershipOfCuString(cu_str_printf("%x", value)); }
inline SimpleString HexStringFrom(int value) { return HexStringFrom((unsigned int)value); }
inline SimpleString HexStringFrom(signed char value) { return takeOwnershipOfCuString(cu_str_hex_from_signed_char(value)); }
inline SimpleString HexStringFrom(cpputest_ulonglong value) { return takeOwnershipOfCuString(cu_str_printf("%llx", value)); }
inline SimpleString HexStringFrom(cpputest_longlong value) { return HexStringFrom((cpputest_ulonglong)value); }
inline SimpleString HexStringFrom(const void *value) { return HexStringFrom((cpputest_ulonglong)(size_t)value); }
inline SimpleString HexStringFrom(void (*value)()) { return HexStringFrom((cpputest_ulonglong)(size_t)value); }

inline SimpleString BracketsFormattedHexString(SimpleString hexString)
{
    return SimpleString("(0x") + hexString + ")";
}

inline SimpleString BracketsFormattedHexStringFrom(int value) { return BracketsFormattedHexString(HexStringFrom(value)); }
inline SimpleString BracketsFormattedHexStringFrom(unsigned int value) { return BracketsFormattedHexString(HexStringFrom(value)); }
inline SimpleString BracketsFormattedHexStringFrom(long value) { return BracketsFormattedHexString(HexStringFrom(value)); }
inline SimpleString BracketsFormattedHexStringFrom(unsigned long value) { return BracketsFormattedHexString(HexStringFrom(value)); }
inline SimpleString BracketsFormattedHexStringFrom(signed char value) { return BracketsFormattedHexString(HexStringFrom(value)); }
inline SimpleString BracketsFormattedHexStringFrom(cpputest_longlong value) { return BracketsFormattedHexString(HexStringFrom(value)); }
inline SimpleString BracketsFormattedHexStringFrom(cpputest_ulonglong value) { return BracketsFormattedHexString(HexStringFrom(value)); }

inline SimpleString StringFromBinary(const unsigned char *value, size_t size)
{
    return takeOwnershipOfCuString(cu_str_from_binary(value, size));
}

inline SimpleString StringFromBinaryOrNull(const unsigned char *value, size_t size)
{
    return value ? StringFromBinary(value, size) : StringFrom("(null)");
}

inline SimpleString StringFromBinaryWithSize(const unsigned char *value, size_t size)
{
    SimpleString result = StringFromFormat("Size = %u | HexContents = ", (unsigned)size);
    size_t displayedSize = (size > 128) ? 128 : size;
    result += StringFromBinaryOrNull(value, displayedSize);
    if (size > displayedSize)
        result += " ...";
    return result;
}

inline SimpleString StringFromBinaryWithSizeOrNull(const unsigned char *value, size_t size)
{
    return value ? StringFromBinaryWithSize(value, size) : StringFrom("(null)");
}

inline SimpleString StringFromMaskedBits(unsigned long value, unsigned long mask,
                                         size_t byteCount)
{
    return takeOwnershipOfCuString(cu_str_from_masked_bits(value, mask, byteCount));
}

#endif
