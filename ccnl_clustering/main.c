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

#ifdef MODULE_TLSF
#include "tlsf-malloc.h"
#endif
#include "msg.h"
#include "shell.h"
#include "xtimer.h"
#include "net/gnrc/netapi.h"

#include "cluster.h"
#include "ccnlriot.h"

/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#ifdef MODULE_TLSF
static uint32_t _tlsf_heap[TLSF_BUFFER];
#endif

static int _stats(int argc, char **argv);
static int _cs(int argc, char **argv);
static int _debug_cache_date(int argc, char **argv);
static int _start_dow(int argc, char **argv);

const shell_command_t shell_commands[] = {
/*  {name, desc, cmd },                         */
    {"stats", "Print CCNL statistics", _stats},
    {"ccnl_cs", "Print CCNL content store", _cs},
    {"dc", "Create a content chunk and put it into cache for debug purposes",
        _debug_cache_date},
    {"start", "Start the application", _start_dow},
    {NULL, NULL, NULL}
};

int _stats(int argc, char **argv) {
    (void) argc; (void) argv;
    printf("RX: %04" PRIu32 ", TX: %04" PRIu32 "\n", ccnl_relay.ifs[0].rx_cnt, ccnl_relay.ifs[0].tx_cnt);
    if (cluster_sleeping) {
        printf("active: %u, sleeping: %u\n", (unsigned) cluster_time_active, (unsigned) (cluster_time_sleeping + (xtimer_now() - cluster_ts_wakeup)));
    }
    else {
        printf("active: %u, sleeping: %u\n", (unsigned) (cluster_time_active + (xtimer_now() - cluster_ts_sleep)), (unsigned) cluster_time_sleeping);
    }
    return 0;
}

int _cs(int argc, char **argv) {
    (void) argc; (void) argv;
    printf("%u CS command\n", (unsigned) xtimer_now());
    if ((cluster_state != CLUSTER_STATE_INACTIVE) && (cluster_state != CLUSTER_STATE_HANDOVER)) {
        gnrc_netapi_get(ccnl_pid, NETOPT_CCN, CCNL_CTX_PRINT_CS, &ccnl_relay, sizeof(ccnl_relay));
        //ccnl_cs_dump(&ccnl_relay);
        printf("%u DONE\n", (unsigned) xtimer_now());
    }
    else {
        puts("NOP");
    }
    return 0;
}

static int _debug_cache_date(int argc, char **argv)
{
    if ((argc < 3) || (strlen(argv[1]) > 8) || (strlen(argv[2]) > 8)) {
        puts("Usage: dc <ID> <timestamp>");
        return 1;
    }

    /* get data into cache by sending to loopback */
    LOG_DEBUG("main: put data into cache via loopback\n");
    size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 9 + 9;
    char pfx[prefix_len];
    snprintf(pfx, prefix_len, "%s%s/%s/%s", CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, argv[1], argv[2]);
    LOG_INFO("main: DEBUG DATA: %s\n", pfx);
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("cluster: We're doomed, WE ARE ALL DOOMED!\n");
    }
    else {
        ccnl_helper_create_cont(prefix, (unsigned char*) argv[2], strlen(argv[2]) + 1, true, false);
        free_prefix(prefix);
    }
    return 0;
}

static int _start_dow(int argc, char **argv)
{
    (void) argc; (void) argv;
    printf("Settings: %s D:%u X:%u p:%f %u\n", (CLUSTER_DEPUTY ? "Deputy" : ""), CLUSTER_D, CLUSTER_X, CLUSTER_P, CLUSTER_PERIOD);
    cluster_init();
    thread_yield_higher();
    return 0;
}

int main(void)
{
#ifdef MODULE_TLSF
    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));
#endif
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    printf("CCN cluster started\n");

#if CLUSTER_AUTOSTART
    cluster_init();
    thread_yield_higher();
#endif
    /* start the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
