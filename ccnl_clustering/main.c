/*
 * Copyright (C) 2016 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       Basic ccn-lite relay example (produce and consumer via shell)
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include "tlsf-malloc.h"
#include "msg.h"
#include "shell.h"
#include "xtimer.h"
#include "net/gnrc/netapi.h"

#include "cluster.h"
#include "ccnlriot.h"

/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

static uint32_t _tlsf_heap[TLSF_BUFFER];

static int _stats(int argc, char **argv);
static int _cs(int argc, char **argv);

const shell_command_t shell_commands[] = {
/*  {name, desc, cmd },                         */
    {"stats", "Print CCNL statistics", _stats},
    {"ccnl_cs", "Print CCNL content store", _cs},
    {NULL, NULL, NULL}
};

int _stats(int argc, char **argv) {
    (void) argc; (void) argv;
    printf("RX: %04" PRIu32 ", TX: %04" PRIu32 "\n", ccnl_relay.ifs[0].rx_cnt, ccnl_relay.ifs[0].tx_cnt);
    return 0;
}

int _cs(int argc, char **argv) {
    (void) argc; (void) argv;
    gnrc_netapi_get(ccnl_pid, NETOPT_CCN, CCNL_CTX_PRINT_CS, &ccnl_relay, sizeof(ccnl_relay));
    return 0;
}

int main(void)
{
    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    printf("CCN cluster started\n");

    cluster_init();
    thread_yield_higher();

    /* start the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
