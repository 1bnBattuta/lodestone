/**
 * @file main.c
 * @author Omar Merroun
 * @brief entry point for lodestone-parse
 * @version 0.1
 * @date 2026-09-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/args.h"

// Global config
typedef struct {
    int show_help;
    FILE *input_file;
    FILE *output_file;
} config_t;

static config_t ls_parse_cfg = {
    .show_help = 0,
    .input_file = NULL,
    .output_file = NULL,
};

// Global context
typedef struct {
    FILE *ifp;
    FILE *ofp;
} ctx_t;

static ctx_t ls_parse_ctx = {
    .ifp = NULL,
    .ofp = NULL,
};

// Supported command line arguments
const arg_opt_t opts[] = {
    {'h', "help", ARG_FLAG, &ls_parse_cfg.show_help, NULL, "Show this help menu and exit", NULL},
    {'r', "input", ARG_STRING, &ls_parse_cfg.input_file, NULL, "Input file", "name"},
    {'o', "output", ARG_STRING, &ls_parse_cfg.output_file, NULL, "Output file", "name"},
    ARG_END
};

static volatile sig_atomic_t sigint = 0;

static void sighandler(int num) {
    (void)num;
    sigint = 1;
}

int main(int argc, char **argv) {
    int err;
    err = args_parse(argc, argv, opts);
    if (err < 0) {
        fprintf(stderr, "argument parser\n");
        return EXIT_FAILURE;
    }

    if (ls_parse_cfg.show_help) {
        args_print_help(argv[0], " [OPTIONS]", opts);
        return 0;
    }

    struct sigaction sa;
    sa.sa_handler = sighandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    

    return EXIT_SUCCESS;
}
