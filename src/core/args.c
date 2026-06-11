#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* Port of upstream CommandLineArguments.cpp. The else-if dispatch order is
 * load-bearing (e.g. "-sn" must be tested before "-s") and is preserved
 * verbatim, as are the usage/help texts. */

static int starts_with(const char *s, const char *prefix)
{
    return 0 == strncmp(s, prefix, strlen(prefix));
}

/* strndup with the checked allocator (callers always pass n <= strlen) */
static char *xstrndup(const char *s, size_t n)
{
    char *p = cu_xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static cu_filter *filter_new(const char *text, int strict, int invert)
{
    cu_filter *f = cu_xmalloc(sizeof *f);
    f->text = cu_xstrdup(text ? text : "");
    f->strict = strict;
    f->invert = invert;
    f->next = NULL;
    return f;
}

static void filters_free(cu_filter *f)
{
    while (f) {
        cu_filter *next = f->next;
        free(f->text);
        free(f);
        f = next;
    }
}

int cu_filters_match(const cu_filter *filters, const char *s)
{
    if (!filters)
        return 1;
    for (const cu_filter *f = filters; f; f = f->next) {
        int eq =
            f->strict ? 0 == strcmp(s, f->text) : NULL != strstr(s, f->text);
        if (f->invert ? !eq : eq)
            return 1;
    }
    return 0;
}

/* getParameterField: value glued to the flag, or the following argument. */
static const char *parameter_field(int ac, const char *const *av, int *i,
                                   const char *parameter_name)
{
    size_t len = strlen(parameter_name);
    if (strlen(av[*i]) > len)
        return av[*i] + len;
    if (*i + 1 < ac)
        return av[++*i];
    return "";
}

static void add_filter(cu_filter **list, const char *text, int strict,
                       int invert)
{
    cu_filter *f = filter_new(text, strict, invert);
    f->next = *list;
    *list = f;
}

/* "-t group.name": split on the only dot; more or fewer parts is an error. */
static int add_group_dot_name(cu_args *a, int ac, const char *const *av, int *i,
                              const char *parameter_name, int strict,
                              int invert)
{
    const char *field = parameter_field(ac, av, i, parameter_name);
    const char *dot = strchr(field, '.');
    if (dot == NULL || dot != strrchr(field, '.'))
        return 0;

    char *group = xstrndup(field, (size_t)(dot - field));
    add_filter(&a->group_filters, group, strict, invert);
    free(group);
    add_filter(&a->name_filters, dot + 1, strict, invert);
    return 1;
}

/* "TEST(Group, name)" copy-pasted from -v output: strict filters on both.
 * Upstream never reports a parse error here, even for malformed input
 * (addTestToRunBasedOnVerboseOutput returns void). Group is everything
 * before the first ','; name skips two chars (", ") and stops before ')'. */
static void add_test_from_verbose_output(cu_args *a, int ac,
                                         const char *const *av, int *i,
                                         const char *prefix)
{
    const char *field = parameter_field(ac, av, i, prefix);
    const char *comma = strchr(field, ',');
    char *group;
    char *name;

    if (comma) {
        group = xstrndup(field, (size_t)(comma - field));
        const char *name_start = comma;
        size_t name_len = strlen(name_start);
        const char *close = strchr(name_start, ')');
        if (close)
            name_len = (size_t)(close - name_start);
        /* subString(2): drop ", " (or whatever two chars follow the comma) */
        name = name_len > 2 ? xstrndup(name_start + 2, name_len - 2)
                            : cu_xstrdup("");
    } else {
        group = cu_xstrdup(field);
        name = cu_xstrdup("");
    }
    add_filter(&a->group_filters, group, 1, 0);
    add_filter(&a->name_filters, name, 1, 0);
    free(group);
    free(name);
}

int cu_args_parse(cu_args *a, int argc, const char *const *argv)
{
    memset(a, 0, sizeof *a);

    int correct = 1;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (0 == strcmp(arg, "-h")) {
            a->need_help = 1;
            correct = 0;
        } else if (0 == strcmp(arg, "-v"))
            a->verbose = 1;
        else if (0 == strcmp(arg, "-ri"))
            a->run_ignored = 1;
        else if (starts_with(arg, "-g"))
            add_filter(&a->group_filters, parameter_field(argc, argv, &i, "-g"),
                       0, 0);
        else if (starts_with(arg, "-t"))
            correct = add_group_dot_name(a, argc, argv, &i, "-t", 0, 0);
        else if (starts_with(arg, "-st"))
            correct = add_group_dot_name(a, argc, argv, &i, "-st", 1, 0);
        else if (starts_with(arg, "-xt"))
            correct = add_group_dot_name(a, argc, argv, &i, "-xt", 0, 1);
        else if (starts_with(arg, "-xst"))
            correct = add_group_dot_name(a, argc, argv, &i, "-xst", 1, 1);
        else if (starts_with(arg, "-sg"))
            add_filter(&a->group_filters,
                       parameter_field(argc, argv, &i, "-sg"), 1, 0);
        else if (starts_with(arg, "-xg"))
            add_filter(&a->group_filters,
                       parameter_field(argc, argv, &i, "-xg"), 0, 1);
        else if (starts_with(arg, "-xsg"))
            add_filter(&a->group_filters,
                       parameter_field(argc, argv, &i, "-xsg"), 1, 1);
        else if (starts_with(arg, "-n"))
            add_filter(&a->name_filters, parameter_field(argc, argv, &i, "-n"),
                       0, 0);
        else if (starts_with(arg, "-sn"))
            add_filter(&a->name_filters, parameter_field(argc, argv, &i, "-sn"),
                       1, 0);
        else if (starts_with(arg, "-xn"))
            add_filter(&a->name_filters, parameter_field(argc, argv, &i, "-xn"),
                       0, 1);
        else if (starts_with(arg, "-xsn"))
            add_filter(&a->name_filters,
                       parameter_field(argc, argv, &i, "-xsn"), 1, 1);
        else if (starts_with(arg, "TEST("))
            add_test_from_verbose_output(a, argc, argv, &i, "TEST(");
        else if (starts_with(arg, "IGNORE_TEST("))
            add_test_from_verbose_output(a, argc, argv, &i, "IGNORE_TEST(");
        else if (starts_with(arg, "-p"))
            correct =
                cu_plugin_parse_hook ? cu_plugin_parse_hook(argc, argv, i) : 0;
        /* cpputest-turbo extension: -jN parallel workers (group granularity)
         */
        else if (starts_with(arg, "-j"))
            a->parallel_workers = atoi(arg + 2);
        else
            correct = 0;

        if (!correct)
            return 0;
    }
    return 1;
}

void cu_args_free(cu_args *a)
{
    filters_free(a->group_filters);
    filters_free(a->name_filters);
    a->group_filters = NULL;
    a->name_filters = NULL;
}

const char *cu_usage_text(void)
{
    return "use -h for more extensive help\n"
           "usage [-h] [-v] [-ri] [-j<#>]\n"
           "      [-g|sg|xg|xsg <groupName>]... [-n|sn|xn|xsn <testName>]... "
           "[-t|st|xt|xst <groupName>.<testName>]...\n"
           "      [\"[IGNORE_]TEST(<groupName>, <testName>)\"]...\n";
}

const char *cu_help_text(void)
{
    return "Thanks for using CppUTest (turbo-lite).\n"
           "\n"
           "  -h                - this wonderful help screen. Joy!\n"
           "  -v                - verbose, print each test name as it runs\n"
           "  -ri               - run ignored tests as if they are not "
           "ignored\n"
           "  -j<#>             - run test groups across <#> parallel worker "
           "processes\n"
           "\n"
           "Options that control which tests are run:\n"
           "  -g <group>        - only run tests whose group contains <group>\n"
           "  -n <name>         - only run tests whose name contains <name>\n"
           "  -t <group>.<name> - only run tests whose group and name contain "
           "<group> and <name>\n"
           "  -sg <group>       - only run tests whose group exactly matches "
           "<group>\n"
           "  -sn <name>        - only run tests whose name exactly matches "
           "<name>\n"
           "  -st <grp>.<name>  - only run tests whose group and name exactly "
           "match <grp> and <name>\n"
           "  -xg <group>       - exclude tests whose group contains <group>\n"
           "  -xn <name>        - exclude tests whose name contains <name>\n"
           "  -xt <grp>.<name>  - exclude tests whose group and name contain "
           "<grp> and <name>\n"
           "  -xsg <group>      - exclude tests whose group exactly matches "
           "<group>\n"
           "  -xsn <name>       - exclude tests whose name exactly matches "
           "<name>\n"
           "  -xst <grp>.<name> - exclude tests whose group and name exactly "
           "match <grp> and <name>\n"
           "  \"[IGNORE_]TEST(<group>, <name>)\"\n"
           "                    - only run tests whose group and name exactly "
           "match <group> and <name>\n"
           "                      (this can be used to copy-paste output from "
           "the -v option on the command line)\n";
}
