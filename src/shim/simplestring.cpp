/* cpputest-revibed: out-of-line SimpleString implementation. Kept out of the
 * header so user translation units stay cheap to compile. Rendering rules
 * delegate to the C core for byte parity with the assertion engine. */

#include "CppUTest/SimpleString.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void SimpleString::copyFrom(const char *value)
{
    len_ = strlen(value);
    buffer_ = (char *)malloc(len_ + 1);
    memcpy(buffer_, value, len_ + 1);
}

void SimpleString::append(const char *value, size_t n)
{
    buffer_ = (char *)realloc(buffer_, len_ + n + 1);
    memcpy(buffer_ + len_, value, n);
    len_ += n;
    buffer_[len_] = '\0';
}

SimpleString::SimpleString(const char *value)
{
    copyFrom(value ? value : "");
}

SimpleString::SimpleString(const char *value, size_t repeatCount)
{
    size_t unit = value ? strlen(value) : 0;
    len_ = unit * repeatCount;
    buffer_ = (char *)malloc(len_ + 1);
    for (size_t i = 0; i < repeatCount; i++)
        memcpy(buffer_ + i * unit, value, unit);
    buffer_[len_] = '\0';
}

SimpleString::SimpleString(const SimpleString &other)
{
    copyFrom(other.buffer_);
}

SimpleString &SimpleString::operator=(const SimpleString &other)
{
    if (this != &other) {
        free(buffer_);
        copyFrom(other.buffer_);
    }
    return *this;
}

SimpleString::~SimpleString()
{
    free(buffer_);
}

bool SimpleString::contains(const SimpleString &other) const
{
    return NULL != strstr(buffer_, other.buffer_);
}

bool SimpleString::startsWith(const SimpleString &other) const
{
    return 0 == strncmp(buffer_, other.buffer_, other.len_);
}

bool SimpleString::endsWith(const SimpleString &other) const
{
    return len_ >= other.len_ &&
           0 == strcmp(buffer_ + len_ - other.len_, other.buffer_);
}

SimpleString SimpleString::subString(size_t beginPos, size_t amount) const
{
    if (beginPos > len_)
        return "";
    SimpleString result;
    size_t avail = len_ - beginPos;
    size_t take = avail < amount ? avail : amount;
    result.append(buffer_ + beginPos, take);
    return result;
}

SimpleString SimpleString::subString(size_t beginPos) const
{
    return subString(beginPos, beginPos > len_ ? 0 : len_ - beginPos);
}

SimpleString &SimpleString::operator+=(const SimpleString &other)
{
    append(other.buffer_, other.len_);
    return *this;
}

SimpleString &SimpleString::operator+=(const char *other)
{
    if (other)
        append(other, strlen(other));
    return *this;
}

SimpleString SimpleString::operator+(const SimpleString &other) const
{
    SimpleString result(*this);
    result += other;
    return result;
}

bool SimpleString::operator==(const SimpleString &other) const
{
    return 0 == strcmp(buffer_, other.buffer_);
}

bool SimpleString::operator!=(const SimpleString &other) const
{
    return !(*this == other);
}

SimpleString SimpleString::printable() const
{
    char *p = cu_str_printable(buffer_);
    SimpleString result(p);
    cu_str_free(p);
    return result;
}

void SimpleString::padStringsToSameLength(SimpleString &str1, SimpleString &str2,
                                          char padCharacter)
{
    if (str1.size() > str2.size()) {
        padStringsToSameLength(str2, str1, padCharacter);
        return;
    }
    char pad[2] = { padCharacter, '\0' };
    str1 = SimpleString(pad, str2.size() - str1.size()) + str1;
}

/* ----------------------------- StringFrom ------------------------------- */

static SimpleString takeOwnership(char *s)
{
    SimpleString result(s);
    cu_str_free(s);
    return result;
}

SimpleString StringFrom(bool value) { return SimpleString(value ? "true" : "false"); }
SimpleString StringFrom(const char *value) { return SimpleString(value); }
SimpleString StringFrom(char value) { return takeOwnership(cu_str_printf("%c", value)); }
SimpleString StringFrom(int value) { return takeOwnership(cu_str_printf("%d", value)); }
SimpleString StringFrom(unsigned int value) { return takeOwnership(cu_str_printf("%u", value)); }
SimpleString StringFrom(long value) { return takeOwnership(cu_str_printf("%ld", value)); }
SimpleString StringFrom(unsigned long value) { return takeOwnership(cu_str_printf("%lu", value)); }
SimpleString StringFrom(cpputest_longlong value) { return takeOwnership(cu_str_printf("%lld", value)); }
SimpleString StringFrom(cpputest_ulonglong value) { return takeOwnership(cu_str_printf("%llu", value)); }
SimpleString StringFrom(double value, int precision) { return takeOwnership(cu_str_from_double(value, precision)); }
SimpleString StringFrom(const SimpleString &value) { return value; }
SimpleString StringFrom(const void *value) { return takeOwnership(cu_str_printf("0x%llx", (cpputest_ulonglong)(size_t)value)); }
SimpleString StringFrom(void (*value)()) { return takeOwnership(cu_str_printf("0x%llx", (cpputest_ulonglong)(size_t)value)); }
#if defined(__cplusplus) && __cplusplus >= 201103L
SimpleString StringFrom(decltype(nullptr)) { return "(null)"; }
#endif

SimpleString StringFromOrNull(const char *expected)
{
    return expected ? StringFrom(expected) : StringFrom("(null)");
}

SimpleString PrintableStringFromOrNull(const char *expected)
{
    return expected ? StringFrom(expected).printable() : StringFrom("(null)");
}

SimpleString StringFromFormat(const char *format, ...)
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

SimpleString HexStringFrom(unsigned long value) { return takeOwnership(cu_str_printf("%lx", value)); }
SimpleString HexStringFrom(long value) { return HexStringFrom((unsigned long)value); }
SimpleString HexStringFrom(unsigned int value) { return takeOwnership(cu_str_printf("%x", value)); }
SimpleString HexStringFrom(int value) { return HexStringFrom((unsigned int)value); }
SimpleString HexStringFrom(signed char value) { return takeOwnership(cu_str_hex_from_signed_char(value)); }
SimpleString HexStringFrom(cpputest_ulonglong value) { return takeOwnership(cu_str_printf("%llx", value)); }
SimpleString HexStringFrom(cpputest_longlong value) { return HexStringFrom((cpputest_ulonglong)value); }
SimpleString HexStringFrom(const void *value) { return HexStringFrom((cpputest_ulonglong)(size_t)value); }
SimpleString HexStringFrom(void (*value)()) { return HexStringFrom((cpputest_ulonglong)(size_t)value); }

SimpleString BracketsFormattedHexString(SimpleString hexString)
{
    return SimpleString("(0x") + hexString + ")";
}

SimpleString BracketsFormattedHexStringFrom(int value) { return BracketsFormattedHexString(HexStringFrom(value)); }
SimpleString BracketsFormattedHexStringFrom(unsigned int value) { return BracketsFormattedHexString(HexStringFrom(value)); }
SimpleString BracketsFormattedHexStringFrom(long value) { return BracketsFormattedHexString(HexStringFrom(value)); }
SimpleString BracketsFormattedHexStringFrom(unsigned long value) { return BracketsFormattedHexString(HexStringFrom(value)); }
SimpleString BracketsFormattedHexStringFrom(signed char value) { return BracketsFormattedHexString(HexStringFrom(value)); }
SimpleString BracketsFormattedHexStringFrom(cpputest_longlong value) { return BracketsFormattedHexString(HexStringFrom(value)); }
SimpleString BracketsFormattedHexStringFrom(cpputest_ulonglong value) { return BracketsFormattedHexString(HexStringFrom(value)); }

SimpleString StringFromBinary(const unsigned char *value, size_t size)
{
    return takeOwnership(cu_str_from_binary(value, size));
}

SimpleString StringFromBinaryOrNull(const unsigned char *value, size_t size)
{
    return value ? StringFromBinary(value, size) : StringFrom("(null)");
}

SimpleString StringFromBinaryWithSize(const unsigned char *value, size_t size)
{
    SimpleString result = StringFromFormat("Size = %u | HexContents = ", (unsigned)size);
    size_t displayedSize = (size > 128) ? 128 : size;
    result += StringFromBinaryOrNull(value, displayedSize);
    if (size > displayedSize)
        result += " ...";
    return result;
}

SimpleString StringFromBinaryWithSizeOrNull(const unsigned char *value, size_t size)
{
    return value ? StringFromBinaryWithSize(value, size) : StringFrom("(null)");
}

SimpleString StringFromMaskedBits(unsigned long value, unsigned long mask,
                                  size_t byteCount)
{
    return takeOwnership(cu_str_from_masked_bits(value, mask, byteCount));
}
