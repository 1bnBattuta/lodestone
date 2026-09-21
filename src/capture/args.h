/**
 * @file args.h
 * @author Omar Merroun
 * @brief lodestone-capture args parser API
 * @version 0.1
 * @date 2026-09-21
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef LS_ARGS_H
#define LS_ARGS_H

#include <stddef.h>

typedef enum {
    ARG_FLAG,       /*< bool, takes no value */
    ARG_INT,        /*< int, requires a value */
    ARG_STRING,     /*< const char *, value points into argv */
    ARG_CALLBACK    /*< custom, parser calls callback(value, target) */
} arg_type_t;

typedef int (*arg_callback_t)(const char *value, void *user_data);

typedef struct {
    char short_name;        /*< 0 for none.*/
    const char *long_name;  /*< NULL for none.*/
    arg_type_t type;
    void *target;           /*< points to user data (bool, int, or const char*)*/
    arg_callback_t callback;/*< only used when type == ARG_CALLBACK.*/
    const char *description;/*< used for help menu.*/
    const char *value_name; /*< NULL defaults to VALUE.*/
} arg_opt_t;

#define ARG_END {0, NULL, ARG_FLAG, NULL, NULL, NULL, NULL}

typedef struct {
    const char **items;
    size_t count;
} arg_positional_t;

int  args_parse(int argc, char **argv,
                const arg_opt_t *opts,
                arg_positional_t *positional);

void arg_positional_free(arg_positional_t *positional);

void args_print_help(const char *prog,
                     const char *usage_buffer,
                     const arg_opt_t *opts);

#endif // LS_ARGS_H
