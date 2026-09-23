/**
 * @file args.c
 * @author Omar Merroun
 * @brief lodestone-capture args parser implementation
 * @version 0.1
 * @date 2026-09-22
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"

static int opt_is_sentinel(const arg_opt_t *opt) {
    return opt->short_name == 0 && opt->long_name == NULL;
}

/* Long names may be declared (opt table) either as "verbose" or "--verbose". */
static const char *bare_long_name(const char *ln) {
    return (ln[0] == '-' && ln[1] == '-') ? ln + 2 : ln;
}

/*
 * Returns 1 if `arg` matches `opt` and 0 else.
 * Accepts "-x", "--name", and "--name=value". In the last case,
 * *inline_val points at "value"; otherwise it is set to NULL.
 */
static int opt_matches(const arg_opt_t *opt, char *arg, char **inline_val) {
    *inline_val = NULL;

    if (arg[0] != '-')
        return 0;

    /* Short option: exactly "-x" */
    if (arg[1] != '-')
        return opt->short_name != 0 && arg[1] == opt->short_name && arg[2] == '\0';

    /* Long option: "--name" or "--name=value" */
    if (opt->long_name == NULL)
        return 0;

    const char *name = bare_long_name(opt->long_name);
    size_t len = strlen(name);
    char *rest = arg + 2;

    if (len == 0 || strncmp(rest, name, len) != 0)
        return 0;
    if (rest[len] == '\0')
        return 1;
    if (rest[len] == '=') {
        *inline_val = rest + len + 1;
        return 1;
    }
    return 0;
}

/* Strict string -> int conversion. Returns 0 on success, -1 on error. */
static int parse_int(const char *s, int *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);

    if (end == s || *end != '\0' || errno == ERANGE || v < INT_MIN || v > INT_MAX)
        return -1;

    *out = (int)v;
    return 0;
}

/*
 * Parses argv against the table of options terminated by an entry with
 * short_name == 0 and long_name == NULL (namely ARG_END).
 * Returns 0 on success, -1 on any error (error message is printed to stderr).
 */
int args_parse(int argc, char **argv, const arg_opt_t *opts) {
    if (argc <= 1)
        return 0;

    if (opts == NULL || opt_is_sentinel(opts)) {
        fprintf(stderr, "%s: this program takes no arguments\n", argv[0]);
        return -1;
    }

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *val = NULL;
        const arg_opt_t *opt;

        for (opt = opts; !opt_is_sentinel(opt); opt++) {
            if (opt_matches(opt, arg, &val))
                break;
        }

        if (opt_is_sentinel(opt)) {
            fprintf(stderr, "%s: unrecognized argument '%s'\n", argv[0], arg);
            return -1;
        }

        if (opt->type == ARG_FLAG) {
            if (val != NULL) {
                fprintf(stderr, "%s: option '%s' does not take a value\n", argv[0], arg);
                return -1;
            }
            *(int *)opt->target = 1;
            continue;
        }

        /* Every other type needs a value: inline (--name=value) or next argv. */
        if (val == NULL) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: option '%s' requires a value\n", argv[0], arg);
                return -1;
            }
            val = argv[++i];
        }

        switch (opt->type) {
        case ARG_INT:
            if (parse_int(val, (int *)opt->target) != 0) {
                fprintf(stderr, "%s: invalid integer for option '%s': '%s'\n",
                        argv[0], arg, val);
                return -1;
            }
            break;

        case ARG_STRING:
            *(const char **)opt->target = val;
            break;

        case ARG_CALLBACK:
            if (opt->callback(val, opt->target) < 0) {
                fprintf(stderr, "%s: invalid value for option '%s': '%s'\n",
                        argv[0], arg, val);
                return -1;
            }
            break;

        default:
            fprintf(stderr, "%s: internal error: option '%s' has unknown type %d\n",
                    argv[0], arg, (int)opt->type);
            return -1;
        }
    }

    return 0;
}

/* Name shown after options that take a value, or NULL for flags. */
static const char *value_name(const arg_opt_t *opt) {
    if (opt->type == ARG_FLAG) {return NULL;}
    if (opt->value_name) {return opt->value_name;}
    return opt->type == ARG_INT ? "int" : opt->type == ARG_STRING ? "str" : "value";
}
 
/*
 * Writes the left column of the help line ("-n, --count <int>") into buf.
 * Returns the number of characters written.
 */
static int format_opt(const arg_opt_t *opt, char *buf, size_t size) {
    const char *ln = opt->long_name ? bare_long_name(opt->long_name) : NULL;
    int n;
 
    const char *vn = value_name(opt);
    if (opt->short_name && ln)
        n = snprintf(buf, size, "-%c, --%s <%s>", opt->short_name, ln, vn);
    else if (opt->short_name)
        n = snprintf(buf, size, "-%c <%s>", opt->short_name, vn);
    else /* long name only: with indentation so "--" lines up with the other long names */
        n = snprintf(buf, size, "    --%s <%s>", ln, vn);
 
    if (n < 0)
        return 0;
    return (size_t)n < size ? n : (int)size - 1;
}
 
/*
 * prints for example:
 *   Usage: prog <usage_buffer>
 *
 *   Options:
 *     -v, --verbose      Enable verbose output
 *     -n, --count <int>  Number of iterations
 *         --out <str>    Output file
 */
void args_print_help(const char *prog,
                     const char *usage_buffer,
                     const arg_opt_t *opts)
{
    printf("Usage: %s %s\n", prog, usage_buffer ? usage_buffer : "[options]");
 
    if (opts == NULL || opt_is_sentinel(opts))
        return;
 
    char col[128];
    int width = 0;
 
    /* find the widest left column so descriptions line up*/
    for (const arg_opt_t *opt = opts; !opt_is_sentinel(opt); opt++) {
        int n = format_opt(opt, col, sizeof col);
        if (n > width)
            width = n;
    }
 
    /* print each option, padded to that width*/
    printf("\nOptions:\n");
    for (const arg_opt_t *opt = opts; !opt_is_sentinel(opt); opt++) {
        format_opt(opt, col, sizeof col);
        if (opt->description && opt->description[0] != '\0')
            printf("  %-*s  %s\n", width, col, opt->description);
        else
            printf("  %s\n", col);
    }
}
