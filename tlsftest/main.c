/*
 * Copyright (C) 2016 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Basic test for dynamic memory allocation using TLSF package
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tlsf-malloc.h"
#include "shell.h"

/* 10kB buffer for the heap should be enough for everyone */
#define TLSF_BUFFER     (20480 / sizeof(uint32_t))
static uint32_t _tlsf_heap[TLSF_BUFFER];

static int _tlsf_stats(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    tlsf_walk_pool(NULL);
    return 0;
}

static int _tlsf_malloc(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: malloc <size>");
        return 1;
    }

    int size = strtol(argv[1], NULL, 10);
    printf("allocating %i bytes ", size);
    int res = (int) malloc(size);
    printf("@ %X\n", res);
    return res;
}

static int _tlsf_free(int argc, char **argv) {
    if (argc < 2) {
        puts("Usage: free <addr>");
        return 1;
    }

    int addr = strtol(argv[1], NULL, 16);
    printf("freeint memory at address %X\n", addr);
    free((void*) addr);
    return 0;
}

const shell_command_t shell_commands[] = {
    {"stats", "Print TLSF statistics", _tlsf_stats},
    {"malloc", "allocates memory", _tlsf_malloc},
    {"free", "free memory", _tlsf_free},
    {NULL, NULL, NULL}
};

int main(void)
{
    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));

    puts("Basic TLSF test");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
