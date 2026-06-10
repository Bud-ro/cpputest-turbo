#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Assertion engine. Every failure message below reproduces upstream
 * TestFailure.cpp / SimpleString.cpp byte-for-byte (docs/INTERFACE.md §2).
 * Messages are built on the heap (failure path only — the passing path does
 * no allocation at all). */

/* ------------------------- string building ------------------------------ */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sb;

static void sb_init(sb *b)
{
    b->cap = 256;
    b->len = 0;
    b->buf = cu_xmalloc(b->cap);
    b->buf[0] = '\0';
}

static void sb_reserve(sb *b, size_t extra)
{
    if (b->len + extra + 1 > b->cap) {
        while (b->len + extra + 1 > b->cap)
            b->cap *= 2;
        b->buf = cu_xrealloc(b->buf, b->cap);
    }
}

static void sb_add_n(sb *b, const char *s, size_t n)
{
    sb_reserve(b, n);
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void sb_add(sb *b, const char *s)
{
    sb_add_n(b, s, strlen(s));
}

static void sb_addf(sb *b, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (n > 0) {
        sb_reserve(b, (size_t)n);
        vsnprintf(b->buf + b->len, (size_t)n + 1, format, args);
        b->len += (size_t)n;
    }
    va_end(args);
}

static void sb_add_repeat(sb *b, char c, size_t n)
{
    sb_reserve(b, n);
    memset(b->buf + b->len, c, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

char *cu_str_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    char *s = cu_xmalloc(n > 0 ? (size_t)n + 1 : 1);
    if (n > 0)
        vsnprintf(s, (size_t)n + 1, format, args);
    else
        s[0] = '\0';
    va_end(args);
    return s;
}

void cu_str_free(char *s)
{
    free(s);
}

/* ----------------- upstream SimpleString helpers in C ------------------- */

/* SimpleString::ToLower — ASCII only, like upstream */
static char tolower_c(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch + ('a' - 'A')) : ch;
}

static int strcasecmp_c(const char *a, const char *b)
{
    while (*a && tolower_c(*a) == tolower_c(*b)) {
        a++;
        b++;
    }
    return (unsigned char)tolower_c(*a) - (unsigned char)tolower_c(*b);
}

static int starts_with_nocase(const char *s, const char *prefix)
{
    while (*prefix) {
        if (tolower_c(*s) != tolower_c(*prefix))
            return 0;
        s++;
        prefix++;
    }
    return 1;
}

static const char *strcasestr_c(const char *haystack, const char *needle)
{
    if (!*needle)
        return haystack;
    for (const char *p = haystack; *p; p++)
        if (starts_with_nocase(p, needle))
            return p;
    return NULL;
}

static int is_control_short_escape(char c)
{
    return c >= '\a' && c <= '\r';
}

static int is_control(char c)
{
    unsigned char u = (unsigned char)c;
    return u < ' ' || u == 0x7F;
}

/* SimpleString::printable(): \a..\r as two-char escapes, other control chars
 * as "\xNN " — uppercase hex with a trailing space. */
char *cu_str_printable(const char *s)
{
    static const char *short_escapes[] = {"\\a", "\\b", "\\t", "\\n",
                                          "\\v", "\\f", "\\r"};
    sb b;
    sb_init(&b);
    for (const char *p = s; *p; p++) {
        if (is_control_short_escape(*p))
            sb_add(&b, short_escapes[*p - '\a']);
        else if (is_control(*p))
            sb_addf(&b, "\\x%02X ", *p);
        else
            sb_add_n(&b, p, 1);
    }
    return b.buf;
}

/* StringFrom(double, precision): "%.*g" with NaN/Inf special cases */
char *cu_str_from_double(double value, int precision)
{
    if (isnan(value))
        return cu_str_printf("Nan - Not a number");
    if (isinf(value))
        return cu_str_printf("Inf - Infinity");
    return cu_str_printf("%.*g", precision, value);
}

/* StringFromBinary: "%02X " per byte, trailing space stripped */
char *cu_str_from_binary(const unsigned char *value, size_t size)
{
    sb b;
    sb_init(&b);
    for (size_t i = 0; i < size; i++)
        sb_addf(&b, "%02X ", value[i]);
    if (b.len > 0) {
        b.len--;
        b.buf[b.len] = '\0';
    }
    return b.buf;
}

/* StringFromMaskedBits: MSB first, '1'/'0' where mask set, 'x' elsewhere,
 * space every 8 bits */
char *cu_str_from_masked_bits(unsigned long value, unsigned long mask,
                              size_t byte_count)
{
    sb b;
    sb_init(&b);
    size_t bit_count = (byte_count > sizeof(unsigned long))
                           ? sizeof(unsigned long) * 8
                           : byte_count * 8;
    unsigned long msb = 1UL << (bit_count - 1);
    for (size_t i = 0; i < bit_count; i++) {
        if (mask & msb)
            sb_add(&b, (value & msb) ? "1" : "0");
        else
            sb_add(&b, "x");
        if ((i % 8) == 7 && i != bit_count - 1)
            sb_add(&b, " ");
        value <<= 1;
        mask <<= 1;
    }
    return b.buf;
}

/* HexStringFrom(signed char): "%x" (int-promoted), negatives truncated to
 * the last CHAR_BIT/4 == 2 hex digits */
char *cu_str_hex_from_signed_char(signed char value)
{
    char *full = cu_str_printf("%x", value);
    if (value < 0) {
        size_t len = strlen(full);
        char *truncated = cu_str_printf("%s", full + (len - 2));
        free(full);
        return truncated;
    }
    return full;
}

/* ------------------------ failure message parts ------------------------- */

/* createUserText: "Message: " prefix unless the text starts with
 * "LONGS_EQUAL" (upstream kludge for the LONGS_EQUAL default text) */
static void sb_user_text(sb *b, const char *text)
{
    if (text && text[0]) {
        if (0 != strncmp(text, "LONGS_EQUAL", strlen("LONGS_EQUAL")))
            sb_add(b, "Message: ");
        sb_add(b, text);
        sb_add(b, "\n\t");
    }
}

static void sb_but_was(sb *b, const char *expected, const char *actual)
{
    sb_addf(b, "expected <%s>\n\tbut was  <%s>", expected, actual);
}

/* createDifferenceAtPosString: 20-char window into the actual string padded
 * with 10 spaces on each side, then a caret line under the first diff. */
static void sb_difference_at_pos(sb *b, const char *actual, size_t offset,
                                 size_t reported_position)
{
    const size_t window = 20;
    const size_t half = window / 2;
    char *different = cu_str_printf("difference starts at position %lu at: <",
                                    (unsigned long)reported_position);

    sb padded;
    sb_init(&padded);
    sb_add_repeat(&padded, ' ', half);
    sb_add(&padded, actual);
    sb_add_repeat(&padded, ' ', half);

    size_t avail = padded.len > offset ? padded.len - offset : 0;
    size_t take = avail < window ? avail : window;

    sb_add(b, "\n");
    sb_add(b, "\t");
    sb_add(b, different);
    sb_add_n(b, padded.buf + offset, take);
    sb_add(b, ">\n");
    sb_add(b, "\t");
    sb_add_repeat(b, ' ', strlen(different) + half);
    sb_add(b, "^");

    free(padded.buf);
    free(different);
}

static char *printable_or_null(const char *s)
{
    return s ? cu_str_printable(s) : cu_str_printf("(null)");
}

/* finish: raise the failure with the built message and free the buffer.
 * cu_fail_with_message longjmps, so free first. */
static void sb_raise(sb *b, const char *file, size_t line)
{
    /* parked in a static buffer (grown to fit) because the message must
     * outlive the longjmp; upstream messages are unbounded — STRCMP_EQUAL
     * embeds both full strings — so truncating would break byte-parity */
    static char *message;
    static size_t message_cap;
    if (b->len + 1 > message_cap) {
        free(message);
        message_cap = b->len + 1 < 8192 ? 8192 : b->len + 1;
        message = cu_xmalloc(message_cap);
    }
    memcpy(message, b->buf, b->len + 1);
    free(b->buf);
    cu_fail_with_message(file, line, message);
}

/* Lo<decimal> (0x<hex>)" pair with decimals padded to equal length */
static void sb_padded_decimal_hex(sb *b, const char *e_dec, const char *e_hex,
                                  const char *a_dec, const char *a_hex,
                                  const char *text)
{
    size_t elen = strlen(e_dec), alen = strlen(a_dec);
    size_t width = elen > alen ? elen : alen;

    sb_user_text(b, text);
    sb_add(b, "expected <");
    sb_add_repeat(b, ' ', width - elen);
    sb_add(b, e_dec);
    sb_addf(b, " (0x%s)>\n\tbut was  <", e_hex);
    sb_add_repeat(b, ' ', width - alen);
    sb_add(b, a_dec);
    sb_addf(b, " (0x%s)>", a_hex);
}

/* ------------------------------ asserts --------------------------------- */

void cu_fail_check(const char *check_string, const char *condition_string,
                   const char *text, const char *file, size_t line)
{
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_addf(&b, "%s(%s) failed", check_string, condition_string);
    sb_raise(&b, file, line);
}

void cu_assert_true(int condition, const char *check_string,
                    const char *condition_string, const char *text,
                    const char *file, size_t line)
{
    cu_count_check();
    if (condition)
        return;
    cu_fail_check(check_string, condition_string, text, file, line);
}

void cu_fail(const char *text, const char *file, size_t line)
{
    cu_count_check();
    cu_fail_with_message(file, line, text ? text : "");
}

/* StringEqualFailure (used by both case-sensitive and nocase variants; the
 * difference scan honors `nocase`) */
static void raise_string_equal(const char *expected, const char *actual,
                               int nocase, const char *text, const char *file,
                               size_t line)
{
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    char *pe = printable_or_null(expected);
    char *pa = printable_or_null(actual);
    sb_but_was(&b, pe, pa);
    if (expected && actual) {
        size_t fail_start = 0;
        size_t fail_start_printable = 0;
        if (nocase) {
            while (tolower_c(actual[fail_start]) ==
                       tolower_c(expected[fail_start]) &&
                   actual[fail_start])
                fail_start++;
            while (tolower_c(pa[fail_start_printable]) ==
                       tolower_c(pe[fail_start_printable]) &&
                   pa[fail_start_printable])
                fail_start_printable++;
        } else {
            while (actual[fail_start] == expected[fail_start] &&
                   actual[fail_start])
                fail_start++;
            while (pa[fail_start_printable] == pe[fail_start_printable] &&
                   pa[fail_start_printable])
                fail_start_printable++;
        }
        sb_difference_at_pos(&b, pa, fail_start_printable, fail_start);
    }
    free(pe);
    free(pa);
    sb_raise(&b, file, line);
}

void cu_fail_cstr_equal(const char *expected, const char *actual,
                        const char *text, const char *file, size_t line)
{
    raise_string_equal(expected, actual, 0, text, file, line);
}

void cu_assert_cstr_equal(const char *expected, const char *actual,
                          const char *text, const char *file, size_t line)
{
    cu_count_check();
    if (actual == NULL && expected == NULL)
        return;
    if (actual == NULL || expected == NULL || 0 != strcmp(expected, actual))
        raise_string_equal(expected, actual, 0, text, file, line);
}

void cu_assert_cstr_nequal(const char *expected, const char *actual,
                           size_t length, const char *text, const char *file,
                           size_t line)
{
    cu_count_check();
    if (actual == NULL && expected == NULL)
        return;
    if (actual == NULL || expected == NULL ||
        0 != strncmp(expected, actual, length))
        raise_string_equal(expected, actual, 0, text, file, line);
}

void cu_assert_cstr_nocase_equal(const char *expected, const char *actual,
                                 const char *text, const char *file,
                                 size_t line)
{
    cu_count_check();
    if (actual == NULL && expected == NULL)
        return;
    if (actual == NULL || expected == NULL ||
        0 != strcasecmp_c(expected, actual))
        raise_string_equal(expected, actual, 1, text, file, line);
}

/* ContainsFailure: upstream passes the raw char*s through SimpleString,
 * which renders NULL as an EMPTY string (not "(null)") */
static void raise_contains(const char *expected, const char *actual,
                           const char *text, const char *file, size_t line)
{
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_addf(&b, "actual <%s>\n\tdid not contain  <%s>", actual ? actual : "",
            expected ? expected : "");
    sb_raise(&b, file, line);
}

void cu_assert_cstr_contains(const char *expected, const char *actual,
                             const char *text, const char *file, size_t line)
{
    cu_count_check();
    if (actual == NULL && expected == NULL)
        return;
    if (actual == NULL || expected == NULL || NULL == strstr(actual, expected))
        raise_contains(expected, actual, text, file, line);
}

void cu_assert_cstr_nocase_contains(const char *expected, const char *actual,
                                    const char *text, const char *file,
                                    size_t line)
{
    cu_count_check();
    if (actual == NULL && expected == NULL)
        return;
    if (actual == NULL || expected == NULL ||
        NULL == strcasestr_c(actual, expected))
        raise_contains(expected, actual, text, file, line);
}

void cu_assert_longs_equal(long expected, long actual, const char *text,
                           const char *file, size_t line)
{
    cu_count_check();
    if (expected == actual)
        return;
    cu_fail_longs(expected, actual, text, file, line);
}

void cu_fail_longs(long expected, long actual, const char *text,
                   const char *file, size_t line)
{
    char *ed = cu_str_printf("%ld", expected);
    char *ad = cu_str_printf("%ld", actual);
    char *eh = cu_str_printf("%lx", (unsigned long)expected);
    char *ah = cu_str_printf("%lx", (unsigned long)actual);
    sb b;
    sb_init(&b);
    sb_padded_decimal_hex(&b, ed, eh, ad, ah, text);
    free(ed);
    free(ad);
    free(eh);
    free(ah);
    sb_raise(&b, file, line);
}

void cu_assert_unsigned_longs_equal(unsigned long expected,
                                    unsigned long actual, const char *text,
                                    const char *file, size_t line)
{
    cu_count_check();
    if (expected == actual)
        return;
    cu_fail_unsigned_longs(expected, actual, text, file, line);
}

void cu_fail_unsigned_longs(unsigned long expected, unsigned long actual,
                            const char *text, const char *file, size_t line)
{
    char *ed = cu_str_printf("%lu", expected);
    char *ad = cu_str_printf("%lu", actual);
    char *eh = cu_str_printf("%lx", expected);
    char *ah = cu_str_printf("%lx", actual);
    sb b;
    sb_init(&b);
    sb_padded_decimal_hex(&b, ed, eh, ad, ah, text);
    free(ed);
    free(ad);
    free(eh);
    free(ah);
    sb_raise(&b, file, line);
}

void cu_assert_longlongs_equal(long long expected, long long actual,
                               const char *text, const char *file, size_t line)
{
    cu_count_check();
    if (expected == actual)
        return;
    cu_fail_longlongs(expected, actual, text, file, line);
}

void cu_fail_longlongs(long long expected, long long actual, const char *text,
                       const char *file, size_t line)
{
    char *ed = cu_str_printf("%lld", expected);
    char *ad = cu_str_printf("%lld", actual);
    char *eh = cu_str_printf("%llx", (unsigned long long)expected);
    char *ah = cu_str_printf("%llx", (unsigned long long)actual);
    sb b;
    sb_init(&b);
    sb_padded_decimal_hex(&b, ed, eh, ad, ah, text);
    free(ed);
    free(ad);
    free(eh);
    free(ah);
    sb_raise(&b, file, line);
}

void cu_assert_unsigned_longlongs_equal(unsigned long long expected,
                                        unsigned long long actual,
                                        const char *text, const char *file,
                                        size_t line)
{
    cu_count_check();
    if (expected == actual)
        return;
    cu_fail_unsigned_longlongs(expected, actual, text, file, line);
}

void cu_fail_unsigned_longlongs(unsigned long long expected,
                                unsigned long long actual, const char *text,
                                const char *file, size_t line)
{
    char *ed = cu_str_printf("%llu", expected);
    char *ad = cu_str_printf("%llu", actual);
    char *eh = cu_str_printf("%llx", expected);
    char *ah = cu_str_printf("%llx", actual);
    sb b;
    sb_init(&b);
    sb_padded_decimal_hex(&b, ed, eh, ad, ah, text);
    free(ed);
    free(ad);
    free(eh);
    free(ah);
    sb_raise(&b, file, line);
}

void cu_assert_signed_bytes_equal(signed char expected, signed char actual,
                                  const char *text, const char *file,
                                  size_t line)
{
    cu_count_check();
    if (expected == actual)
        return;
    cu_fail_signed_bytes(expected, actual, text, file, line);
}

void cu_fail_signed_bytes(signed char expected, signed char actual,
                          const char *text, const char *file, size_t line)
{
    char *ed = cu_str_printf("%d", (int)expected);
    char *ad = cu_str_printf("%d", (int)actual);
    char *eh = cu_str_hex_from_signed_char(expected);
    char *ah = cu_str_hex_from_signed_char(actual);
    sb b;
    sb_init(&b);
    sb_padded_decimal_hex(&b, ed, eh, ad, ah, text);
    free(ed);
    free(ad);
    free(eh);
    free(ah);
    sb_raise(&b, file, line);
}

/* EqualsFailure with "0x<hex>" rendering (StringFrom(const void*)) */
static void raise_pointers_equal(unsigned long long expected,
                                 unsigned long long actual, const char *text,
                                 const char *file, size_t line)
{
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_addf(&b, "expected <0x%llx>\n\tbut was  <0x%llx>", expected, actual);
    sb_raise(&b, file, line);
}

void cu_assert_pointers_equal(const void *expected, const void *actual,
                              const char *text, const char *file, size_t line)
{
    cu_count_check();
    if (expected != actual)
        raise_pointers_equal((unsigned long long)(size_t)expected,
                             (unsigned long long)(size_t)actual, text, file,
                             line);
}

void cu_fail_pointers(const void *expected, const void *actual,
                      const char *text, const char *file, size_t line)
{
    raise_pointers_equal((unsigned long long)(size_t)expected,
                         (unsigned long long)(size_t)actual, text, file, line);
}

void cu_fail_functionpointers(void (*expected)(void), void (*actual)(void),
                              const char *text, const char *file, size_t line)
{
    raise_pointers_equal((unsigned long long)(size_t)expected,
                         (unsigned long long)(size_t)actual, text, file, line);
}

void cu_assert_functionpointers_equal(void (*expected)(void),
                                      void (*actual)(void), const char *text,
                                      const char *file, size_t line)
{
    cu_count_check();
    if (expected != actual)
        raise_pointers_equal((unsigned long long)(size_t)expected,
                             (unsigned long long)(size_t)actual, text, file,
                             line);
}

/* Utest.cpp doubles_equal: any NaN (incl. threshold) -> unequal; both
 * infinite -> equal regardless of sign (upstream quirk); else fabs diff. */
static int doubles_equal(double d1, double d2, double threshold)
{
    if (isnan(d1) || isnan(d2) || isnan(threshold))
        return 0;
    if (isinf(d1) && isinf(d2))
        return 1;
    return fabs(d1 - d2) <= threshold;
}

void cu_assert_doubles_equal(double expected, double actual, double threshold,
                             const char *text, const char *file, size_t line)
{
    cu_count_check();
    if (doubles_equal(expected, actual, threshold))
        return;
    cu_fail_doubles(expected, actual, threshold, text, file, line);
}

void cu_fail_doubles(double expected, double actual, double threshold,
                     const char *text, const char *file, size_t line)
{
    char *e = cu_str_from_double(expected, 7);
    char *a = cu_str_from_double(actual, 7);
    char *t = cu_str_from_double(threshold, 7);
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_but_was(&b, e, a);
    sb_addf(&b, " threshold used was <%s>", t);
    if (isnan(expected) || isnan(actual) || isnan(threshold))
        sb_add(&b, "\n\tCannot make comparisons with Nan");
    free(e);
    free(a);
    free(t);
    sb_raise(&b, file, line);
}

void cu_assert_binary_equal(const void *expected, const void *actual,
                            size_t length, const char *text, const char *file,
                            size_t line)
{
    cu_count_check();
    if (length == 0)
        return;
    if (actual == NULL && expected == NULL)
        return;
    if (actual != NULL && expected != NULL &&
        0 == memcmp(expected, actual, length))
        return;
    cu_fail_memcmp(expected, actual, length, text, file, line);
}

void cu_fail_memcmp(const void *expected, const void *actual, size_t length,
                    const char *text, const char *file, size_t line)
{
    const unsigned char *e = expected;
    const unsigned char *a = actual;
    char *ehex = e ? cu_str_from_binary(e, length) : cu_str_printf("(null)");
    char *ahex = a ? cu_str_from_binary(a, length) : cu_str_printf("(null)");
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_but_was(&b, ehex, ahex);
    if (e && a) {
        size_t fail_start = 0;
        while (a[fail_start] == e[fail_start])
            fail_start++;
        sb_difference_at_pos(&b, ahex, fail_start * 3 + 1, fail_start);
    }
    free(ehex);
    free(ahex);
    sb_raise(&b, file, line);
}

void cu_assert_bits_equal(unsigned long expected, unsigned long actual,
                          unsigned long mask, size_t byte_count,
                          const char *text, const char *file, size_t line)
{
    cu_count_check();
    if ((expected & mask) == (actual & mask))
        return;
    cu_fail_bits(expected, actual, mask, byte_count, text, file, line);
}

void cu_fail_bits(unsigned long expected, unsigned long actual,
                  unsigned long mask, size_t byte_count, const char *text,
                  const char *file, size_t line)
{
    char *e = cu_str_from_masked_bits(expected, mask, byte_count);
    char *a = cu_str_from_masked_bits(actual, mask, byte_count);
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_but_was(&b, e, a);
    free(e);
    free(a);
    sb_raise(&b, file, line);
}

/* CheckEqualFailure: printable strings plus difference-position block */
void cu_assert_equals(int failed, const char *expected, const char *actual,
                      const char *text, const char *file, size_t line)
{
    cu_count_check();
    if (!failed)
        return;
    cu_fail_equals_strings(expected, actual, text, file, line);
}

void cu_fail_equals_strings(const char *expected, const char *actual,
                            const char *text, const char *file, size_t line)
{
    char *pe = printable_or_null(expected);
    char *pa = printable_or_null(actual);
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_but_was(&b, pe, pa);
    size_t fail_start = 0;
    while (actual[fail_start] == expected[fail_start] && actual[fail_start])
        fail_start++;
    size_t fail_start_printable = 0;
    while (pa[fail_start_printable] == pe[fail_start_printable] &&
           pa[fail_start_printable])
        fail_start_printable++;
    sb_difference_at_pos(&b, pa, fail_start_printable, fail_start);
    free(pe);
    free(pa);
    sb_raise(&b, file, line);
}

void cu_assert_compare(int comparison, const char *check_string,
                       const char *comparison_string, const char *text,
                       const char *file, size_t line)
{
    cu_count_check();
    if (comparison)
        return;
    sb b;
    sb_init(&b);
    sb_user_text(&b, text);
    sb_addf(&b, "%s(%s) failed", check_string, comparison_string);
    sb_raise(&b, file, line);
}

void cu_print(const char *text, const char *file, size_t line)
{
    char *s = cu_str_printf("\n%s:%lu %s", file, (unsigned long)line, text);
    cu_output *out = cu_output_current();
    if (out)
        cu_out_print_str(out, s); /* JUnit captures this into <system-out> */
    else
        fputs(s, stdout);
    free(s);
}
