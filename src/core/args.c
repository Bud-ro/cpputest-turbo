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

static cu_filter *filter_new(const char *text, int strict, int invert)
{
    cu_filter *f = malloc(sizeof *f);
    f->text = strdup(text ? text : "");
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

static void set_repeat_count(cu_args *a, int ac, const char *const *av, int *i)
{
    a->repeat = 0;
    if (strlen(av[*i]) > 2)
        a->repeat = (size_t)atol(av[*i] + 2);
    else if (*i + 1 < ac) {
        a->repeat = (size_t)atol(av[*i + 1]);
        if (a->repeat != 0)
            (*i)++;
    }
    if (0 == a->repeat)
        a->repeat = 2;
}

static int set_shuffle(cu_args *a, int ac, const char *const *av, int *i)
{
    a->shuffling = 1;
    a->shuffle_seed = (unsigned)cu_time_in_millis();
    if (a->shuffle_seed == 0)
        a->shuffle_seed++;

    if (strlen(av[*i]) > 2) {
        a->shuffling_preseeded = 1;
        a->shuffle_seed = (unsigned)strtoul(av[*i] + 2, NULL, 10);
    } else if (*i + 1 < ac) {
        unsigned parsed = (unsigned)strtoul(av[*i + 1], NULL, 10);
        if (parsed != 0) {
            a->shuffling_preseeded = 1;
            a->shuffle_seed = parsed;
            (*i)++;
        }
    }
    return a->shuffle_seed != 0;
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

    char *group = strndup(field, (size_t)(dot - field));
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
        group = strndup(field, (size_t)(comma - field));
        const char *name_start = comma;
        size_t name_len = strlen(name_start);
        const char *close = strchr(name_start, ')');
        if (close)
            name_len = (size_t)(close - name_start);
        /* subString(2): drop ", " (or whatever two chars follow the comma) */
        name =
            name_len > 2 ? strndup(name_start + 2, name_len - 2) : strdup("");
    } else {
        group = strdup(field);
        name = strdup("");
    }
    add_filter(&a->group_filters, group, 1, 0);
    add_filter(&a->name_filters, name, 1, 0);
    free(group);
    free(name);
}

static int set_output_type(cu_args *a, int ac, const char *const *av, int *i)
{
    const char *type = parameter_field(ac, av, i, "-o");
    if (type[0] == '\0')
        return 0;
    if (0 == strcmp(type, "normal") || 0 == strcmp(type, "eclipse")) {
        a->output_type = CU_OUTPUT_TYPE_CONSOLE;
        return 1;
    }
    if (0 == strcmp(type, "junit")) {
        a->output_type = CU_OUTPUT_TYPE_JUNIT;
        return 1;
    }
    if (0 == strcmp(type, "teamcity")) {
        a->output_type = CU_OUTPUT_TYPE_TEAMCITY;
        return 1;
    }
    return 0;
}

int cu_args_parse(cu_args *a, int argc, const char *const *argv)
{
    memset(a, 0, sizeof *a);
    a->rethrow_exceptions = 1;
    a->repeat = 1;
    a->output_type = CU_OUTPUT_TYPE_CONSOLE;
    a->package_name = strdup("");

    int correct = 1;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (0 == strcmp(arg, "-h")) {
            a->need_help = 1;
            correct = 0;
        } else if (0 == strcmp(arg, "-v"))
            a->verbose = 1;
        else if (0 == strcmp(arg, "-vv"))
            a->very_verbose = 1;
        else if (0 == strcmp(arg, "-c"))
            a->color = 1;
        else if (0 == strcmp(arg, "-p"))
            a->run_separate_process = 1;
        else if (0 == strcmp(arg, "-b"))
            a->reversing = 1;
        else if (0 == strcmp(arg, "-lg"))
            a->list_groups = 1;
        else if (0 == strcmp(arg, "-ln"))
            a->list_names = 1;
        else if (0 == strcmp(arg, "-ll"))
            a->list_locations = 1;
        else if (0 == strcmp(arg, "-ri"))
            a->run_ignored = 1;
        else if (0 == strcmp(arg, "-f"))
            a->crash_on_fail = 1;
        else if (0 == strcmp(arg, "-e") || 0 == strcmp(arg, "-ci"))
            a->rethrow_exceptions = 0;
        else if (starts_with(arg, "-r"))
            set_repeat_count(a, argc, argv, &i);
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
        else if (starts_with(arg, "-s"))
            correct = set_shuffle(a, argc, argv, &i);
        else if (starts_with(arg, "TEST("))
            add_test_from_verbose_output(a, argc, argv, &i, "TEST(");
        else if (starts_with(arg, "IGNORE_TEST("))
            add_test_from_verbose_output(a, argc, argv, &i, "IGNORE_TEST(");
        else if (starts_with(arg, "-o"))
            correct = set_output_type(a, argc, argv, &i);
        else if (starts_with(arg, "-p"))
            correct =
                cu_plugin_parse_hook ? cu_plugin_parse_hook(argc, argv, i) : 0;
        else if (starts_with(arg, "-k")) {
            free(a->package_name);
            a->package_name = strdup(parameter_field(argc, argv, &i, "-k"));
        }
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
    free(a->package_name);
    a->group_filters = NULL;
    a->name_filters = NULL;
    a->package_name = NULL;
}

const char *cu_usage_text(void)
{
    return "use -h for more extensive help\n"
           "usage [-h] [-v] [-vv] [-c] [-p] [-lg] [-ln] [-ll] [-ri] [-r[<#>]] "
           "[-f] [-e] [-ci]\n"
           "      [-g|sg|xg|xsg <groupName>]... [-n|sn|xn|xsn <testName>]... "
           "[-t|st|xt|xst <groupName>.<testName>]...\n"
           "      [-b] [-s [<seed>]] [\"[IGNORE_]TEST(<groupName>, "
           "<testName>)\"]...\n"
           "      [-o{normal|eclipse|junit|teamcity}] [-k <packageName>]\n";
}

const char *cu_help_text(void)
{
    return "Thanks for using CppUTest.\n"
           "\n"
           "Options that do not run tests but query:\n"
           "  -h                - this wonderful help screen. Joy!\n"
           "  -lg               - print a list of group names, separated by "
           "spaces\n"
           "  -ln               - print a list of test names in the form of "
           "group.name, separated by spaces\n"
           "  -ll               - print a list of test names in the form of "
           "group.name.test_file_path.line\n"
           "\n"
           "Options that change the output format:\n"
           "  -c                - colorize output, print green if OK, or red "
           "if failed\n"
           "  -v                - verbose, print each test name as it runs\n"
           "  -vv               - very verbose, print internal information "
           "during test run\n"
           "\n"
           "Options that change the output location:\n"
           "  -onormal          - no output to files\n"
           "  -oeclipse         - equivalent to -onormal\n"
           "  -oteamcity        - output to xml files (as the name suggests, "
           "for TeamCity)\n"
           "  -ojunit           - output to JUnit ant plugin style xml files "
           "(for CI systems)\n"
           "  -k <packageName>  - add a package name in JUnit output (for "
           "classification in CI systems)\n"
           "\n"
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
           "the -v option on the command line)\n"
           "\n"
           "Options that control how the tests are run:\n"
           "  -p                - run tests in a separate process\n"
           "  -b                - run the tests backwards, reversing the "
           "normal way\n"
           "  -s [<seed>]       - shuffle tests randomly (randomization seed "
           "is optional, must be greater than 0)\n"
           "  -r[<#>]           - repeat the tests <#> times (or twice if <#> "
           "is not specified)\n"
           "  -ri               - run ignored tests as if they are not "
           "ignored\n"
           "  -f                - Cause the tests to crash on failure (to "
           "allow the test to be debugged if necessary)\n"
           "  -e                - do not rethrow unexpected exceptions on "
           "failure\n"
           "  -ci               - continuous integration mode (equivalent to "
           "-e)\n";
}
