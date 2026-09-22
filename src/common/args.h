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
    ARG_FLAG,       /**< int, takes no value */
    ARG_INT,        /**< int, requires a value */
    ARG_STRING,     /**< const char *, value points into argv */
    ARG_CALLBACK    /**< custom, parser calls callback(value, target) */
} arg_type_t;

typedef int (*arg_callback_t)(const char *value, void *user_data);

typedef struct {
    char short_name;        /**< 0 for none.*/
    const char *long_name;  /**< NULL for none.*/
    arg_type_t type;
    void *target;           /**< points to user data (bool, int, or const char*)*/
    arg_callback_t callback;/**< only used when type == ARG_CALLBACK.*/
    const char *description;/**< used for help menu.*/
    const char *value_name; /**< NULL uses a default besed on type.*/
} arg_opt_t;

#define ARG_END {0}

/**
 * \brief Parses argv against the table of hardcoded program options
 *      terminated by ARG_END (no short name nor long name)
 *
 * Accepted forms are: -x, --name, --name=value. All unrecognized args
 * are errors including positionals. argc <= 1 is correct (required args
 * feature is to be added)
 * If the callback function returned a negative value, the parser fails.
 * 
 * \param argc number of arguments (passed directly from main)
 * \param argv list of arguments (passed from main)
 * \param opts list of hardcoded options
 * \return int 0 on success, -1 on any error (messages are printed to stderr)
 */
int args_parse(int argc, char **argv,
                const arg_opt_t *opts);


/**
 * \brief Prints help menu to stdout
 * 
 * @param prog name of the program (aka argv[0])
 * @param usage_buffer string to write in the usage section after "prog"
 * @param opts list of program options
 */
void args_print_help(const char *prog,
                     const char *usage_buffer,
                     const arg_opt_t *opts);

#endif // LS_ARGS_H
