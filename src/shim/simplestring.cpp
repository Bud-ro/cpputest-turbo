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

bool operator==(const SimpleString &left, const SimpleString &right)
{
    return 0 == strcmp(left.buffer_, right.buffer_);
}

bool operator!=(const SimpleString &left, const SimpleString &right)
{
    return !(left == right);
}

/* ------------------------- utility surface ------------------------------ */

size_t SimpleString::find(char ch) const
{
    return findFrom(0, ch);
}

size_t SimpleString::findFrom(size_t starting_position, char ch) const
{
    for (size_t i = starting_position; i < len_; i++)
        if (buffer_[i] == ch)
            return i;
    return npos;
}

SimpleString SimpleString::subStringFromTill(char startChar,
                                             char lastExcludedChar) const
{
    size_t beginPos = find(startChar);
    if (beginPos == npos)
        return "";
    size_t endPos = findFrom(beginPos, lastExcludedChar);
    if (endPos == npos)
        return subString(beginPos);
    return subString(beginPos, endPos - beginPos);
}

/* overlap-advance-by-one counting, like upstream */
size_t SimpleString::count(const SimpleString &substr) const
{
    size_t num = 0;
    const char *str = buffer_;
    const char *strpart = NULL;
    if (*str)
        strpart = strstr(str, substr.buffer_);
    while (*str && strpart) {
        str = strpart + 1;
        num++;
        strpart = strstr(str, substr.buffer_);
    }
    return num;
}

/* upstream split: tokens INCLUDE the (first char of the) delimiter; a
 * trailing remainder becomes an extra token unless the string ends with
 * the delimiter */
void SimpleString::split(const SimpleString &delimiter,
                         SimpleStringCollection &col) const
{
    size_t num = count(delimiter);
    size_t extraEndToken = endsWith(delimiter) ? 0 : 1u;
    col.allocate(num + extraEndToken);

    const char *str = buffer_;
    const char *prev;
    for (size_t i = 0; i < num; ++i) {
        prev = str;
        str = strstr(str, delimiter.buffer_) + 1;
        col[i] = SimpleString(prev).subString(0, (size_t)(str - prev));
    }
    if (extraEndToken)
        col[num] = str;
}

void SimpleString::replace(char to, char with)
{
    for (size_t i = 0; i < len_; i++)
        if (buffer_[i] == to)
            buffer_[i] = with;
}

void SimpleString::replace(const char *to, const char *with)
{
    size_t c = count(to);
    if (c == 0)
        return;
    size_t tolen = strlen(to);
    size_t withlen = strlen(with);
    size_t newlen = len_ + withlen * c - tolen * c;

    char *out = (char *)malloc(newlen + 1);
    size_t o = 0;
    const char *p = buffer_;
    while (*p) {
        if (0 == strncmp(p, to, tolen) && tolen > 0) {
            memcpy(out + o, with, withlen);
            o += withlen;
            p += tolen;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
    free(buffer_);
    buffer_ = out;
    len_ = o;
}

char SimpleString::ToLower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch + ('a' - 'A')) : ch;
}

SimpleString SimpleString::lowerCase() const
{
    SimpleString str(*this);
    for (size_t i = 0; i < str.len_; i++)
        str.buffer_[i] = ToLower(str.buffer_[i]);
    return str;
}

bool SimpleString::equalsNoCase(const SimpleString &str) const
{
    return lowerCase() == str.lowerCase();
}

bool SimpleString::containsNoCase(const SimpleString &other) const
{
    return lowerCase().contains(other.lowerCase());
}

void SimpleString::copyToBuffer(char *bufferToCopy, size_t bufferSize) const
{
    if (bufferToCopy == NULLPTR || bufferSize == 0)
        return;
    size_t sizeToCopy = (bufferSize - 1 < len_) ? bufferSize - 1 : len_;
    memcpy(bufferToCopy, buffer_, sizeToCopy);
    bufferToCopy[sizeToCopy] = '\0';
}

static bool ss_isSpace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' ||
           ch == '\v';
}

static bool ss_isDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

int SimpleString::AtoI(const char *str)
{
    while (ss_isSpace(*str))
        str++;
    char first_char = *str;
    if (first_char == '-' || first_char == '+')
        str++;
    int result = 0;
    for (; ss_isDigit(*str); str++) {
        result *= 10;
        result += *str - '0';
    }
    return (first_char == '-') ? -result : result;
}

unsigned SimpleString::AtoU(const char *str)
{
    while (ss_isSpace(*str))
        str++;
    unsigned result = 0;
    for (; ss_isDigit(*str); str++) {
        result *= 10;
        result += (unsigned)(*str - '0');
    }
    return result;
}

int SimpleString::StrCmp(const char *s1, const char *s2)
{
    return strcmp(s1, s2);
}

size_t SimpleString::StrLen(const char *str)
{
    return strlen(str);
}

int SimpleString::StrNCmp(const char *s1, const char *s2, size_t n)
{
    return strncmp(s1, s2, n);
}

char *SimpleString::StrNCpy(char *s1, const char *s2, size_t n)
{
    return strncpy(s1, s2, n);
}

const char *SimpleString::StrStr(const char *s1, const char *s2)
{
    return strstr(s1, s2);
}

int SimpleString::MemCmp(const void *s1, const void *s2, size_t n)
{
    return memcmp(s1, s2, n);
}

/* -------------------------- collection ----------------------------------- */

SimpleStringCollection::SimpleStringCollection()
    : collection_(NULLPTR), size_(0)
{
}

SimpleStringCollection::~SimpleStringCollection()
{
    delete[] collection_;
}

void SimpleStringCollection::allocate(size_t size)
{
    delete[] collection_;
    size_ = size;
    collection_ = new SimpleString[size];
}

size_t SimpleStringCollection::size() const
{
    return size_;
}

SimpleString &SimpleStringCollection::operator[](size_t index)
{
    if (index >= size_)
        return empty_;
    return collection_[index];
}

SimpleString SimpleString::printable() const
{
    char *p = cu_str_printable(buffer_);
    SimpleString result(p);
    cu_str_free(p);
    return result;
}

void SimpleString::padStringsToSameLength(SimpleString &str1,
                                          SimpleString &str2, char padCharacter)
{
    if (str1.size() > str2.size()) {
        padStringsToSameLength(str2, str1, padCharacter);
        return;
    }
    char pad[2] = {padCharacter, '\0'};
    str1 = SimpleString(pad, str2.size() - str1.size()) + str1;
}

/* ----------------------------- StringFrom ------------------------------- */

static SimpleString takeOwnership(char *s)
{
    SimpleString result(s);
    cu_str_free(s);
    return result;
}

SimpleString StringFrom(bool value)
{
    return SimpleString(value ? "true" : "false");
}
SimpleString StringFrom(const char *value)
{
    return SimpleString(value);
}
SimpleString StringFrom(char value)
{
    return takeOwnership(cu_str_printf("%c", value));
}
SimpleString StringFrom(int value)
{
    return takeOwnership(cu_str_printf("%d", value));
}
SimpleString StringFrom(unsigned int value)
{
    return takeOwnership(cu_str_printf("%u", value));
}
SimpleString StringFrom(long value)
{
    return takeOwnership(cu_str_printf("%ld", value));
}
SimpleString StringFrom(unsigned long value)
{
    return takeOwnership(cu_str_printf("%lu", value));
}
SimpleString StringFrom(cpputest_longlong value)
{
    return takeOwnership(cu_str_printf("%lld", value));
}
SimpleString StringFrom(cpputest_ulonglong value)
{
    return takeOwnership(cu_str_printf("%llu", value));
}
SimpleString StringFrom(double value, int precision)
{
    return takeOwnership(cu_str_from_double(value, precision));
}
SimpleString StringFrom(const SimpleString &value)
{
    return value;
}
SimpleString StringFrom(const void *value)
{
    return takeOwnership(
        cu_str_printf("0x%llx", (cpputest_ulonglong)(size_t)value));
}
SimpleString StringFrom(void (*value)())
{
    return takeOwnership(
        cu_str_printf("0x%llx", (cpputest_ulonglong)(size_t)value));
}
#if defined(__cplusplus) && __cplusplus >= 201103L
SimpleString StringFrom(decltype(nullptr))
{
    return "(null)";
}
#endif

SimpleString StringFromOrNull(const char *expected)
{
    return expected ? StringFrom(expected) : StringFrom("(null)");
}

SimpleString PrintableStringFromOrNull(const char *expected)
{
    return expected ? StringFrom(expected).printable() : StringFrom("(null)");
}

SimpleString VStringFromFormat(const char *format, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(NULLPTR, 0, format, copy);
    va_end(copy);
    char *buf = (char *)malloc(n > 0 ? (size_t)n + 1 : 1);
    if (n > 0)
        vsnprintf(buf, (size_t)n + 1, format, args);
    else
        buf[0] = '\0';
    SimpleString result(buf);
    free(buf);
    return result;
}

SimpleString StringFromFormat(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    SimpleString result = VStringFromFormat(format, args);
    va_end(args);
    return result;
}

/* StringFromOrdinalNumber: "1st", "2nd", "3rd", "11th".."13th", else th */
SimpleString StringFromOrdinalNumber(unsigned int number)
{
    const char *suffix = "th";
    if (number < 11 || number > 13) {
        unsigned int onesDigit = number % 10;
        if (onesDigit == 3)
            suffix = "rd";
        else if (onesDigit == 2)
            suffix = "nd";
        else if (onesDigit == 1)
            suffix = "st";
    }
    return StringFromFormat("%u%s", number, suffix);
}

#if CPPUTEST_USE_STD_CPP_LIB
SimpleString StringFrom(const std::string &value)
{
    return SimpleString(value.c_str());
}
#endif

SimpleString HexStringFrom(unsigned long value)
{
    return takeOwnership(cu_str_printf("%lx", value));
}
SimpleString HexStringFrom(long value)
{
    return HexStringFrom((unsigned long)value);
}
SimpleString HexStringFrom(unsigned int value)
{
    return takeOwnership(cu_str_printf("%x", value));
}
SimpleString HexStringFrom(int value)
{
    return HexStringFrom((unsigned int)value);
}
SimpleString HexStringFrom(signed char value)
{
    return takeOwnership(cu_str_hex_from_signed_char(value));
}
SimpleString HexStringFrom(cpputest_ulonglong value)
{
    return takeOwnership(cu_str_printf("%llx", value));
}
SimpleString HexStringFrom(cpputest_longlong value)
{
    return HexStringFrom((cpputest_ulonglong)value);
}
SimpleString HexStringFrom(const void *value)
{
    return HexStringFrom((cpputest_ulonglong)(size_t)value);
}
SimpleString HexStringFrom(void (*value)())
{
    return HexStringFrom((cpputest_ulonglong)(size_t)value);
}

SimpleString BracketsFormattedHexString(SimpleString hexString)
{
    return SimpleString("(0x") + hexString + ")";
}

SimpleString BracketsFormattedHexStringFrom(int value)
{
    return BracketsFormattedHexString(HexStringFrom(value));
}
SimpleString BracketsFormattedHexStringFrom(unsigned int value)
{
    return BracketsFormattedHexString(HexStringFrom(value));
}
SimpleString BracketsFormattedHexStringFrom(long value)
{
    return BracketsFormattedHexString(HexStringFrom(value));
}
SimpleString BracketsFormattedHexStringFrom(unsigned long value)
{
    return BracketsFormattedHexString(HexStringFrom(value));
}
SimpleString BracketsFormattedHexStringFrom(signed char value)
{
    return BracketsFormattedHexString(HexStringFrom(value));
}
SimpleString BracketsFormattedHexStringFrom(cpputest_longlong value)
{
    return BracketsFormattedHexString(HexStringFrom(value));
}
SimpleString BracketsFormattedHexStringFrom(cpputest_ulonglong value)
{
    return BracketsFormattedHexString(HexStringFrom(value));
}

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
    SimpleString result =
        StringFromFormat("Size = %u | HexContents = ", (unsigned)size);
    size_t displayedSize = (size > 128) ? 128 : size;
    result += StringFromBinaryOrNull(value, displayedSize);
    if (size > displayedSize)
        result += " ...";
    return result;
}

SimpleString StringFromBinaryWithSizeOrNull(const unsigned char *value,
                                            size_t size)
{
    return value ? StringFromBinaryWithSize(value, size) : StringFrom("(null)");
}

SimpleString StringFromMaskedBits(unsigned long value, unsigned long mask,
                                  size_t byteCount)
{
    return takeOwnership(cu_str_from_masked_bits(value, mask, byteCount));
}
