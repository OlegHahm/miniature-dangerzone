/*
 * Copyright (C) 2015 Inria
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
 * @brief       Basic ccn-lite relay example (produce and consumer via shell)
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 * @}
 */

#include <stdio.h>

#include "tlsf-malloc.h"
#include "msg.h"
#include "shell.h"
#include "xtimer.h"
#include "net/gnrc/netapi.h"
#include "ccn-lite-riot.h"
#include "ccnlriot.h"
#include "ccnl-pkt-ndntlv.h"

static unsigned char _out[CCNL_MAX_PACKET_SIZE * CCNLRIOT_CHUNKNUMBERS];

/* main thread's message queue */
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

/* 10kB buffer for the heap should be enough for everyone */
#define TLSF_BUFFER     (10240 / sizeof(uint32_t))
static uint32_t _tlsf_heap[TLSF_BUFFER];

/* buffers for interests and content */
static unsigned char _int_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _cont_buf[CCNLRIOT_BUF_SIZE];

char ccnlriot_prefix1[] = CCNLRIOT_PREFIX1;
char ccnlriot_prefix2[] = CCNLRIOT_PREFIX2;

static int _create_cont(int argc, char **argv);
static int _send_int(int argc, char **argv);

const shell_command_t shell_commands[] = {
    {"stats", "Print CCNL statistics", ccnlriot_stats},
    {"int", "Send preconfigured interest", _send_int},
    {"cont", "Create content", _create_cont},
    {NULL, NULL, NULL}
};

static void _create_content(void)
{
    char *body = (char*) CCNLRIOT_CONT;
    int offs = CCNL_MAX_PACKET_SIZE;
    int suite = CCNL_SUITE_NDNTLV;
    struct ccnl_prefix_s *prefix;

    int arg_len;

    unsigned chunk_num = 0;
    for (int i = 0; i < CCNLRIOT_CHUNKNUMBERS; i++) {
        offs = CCNL_MAX_PACKET_SIZE;
        arg_len = strlen(CCNLRIOT_CONT) + 1;
        char pfx[] = CCNLRIOT_PREFIX1;
        prefix = ccnl_URItoPrefix(pfx, suite, NULL, &chunk_num);
        if (i == 9) {
            arg_len = ccnl_ndntlv_prependContent(prefix, (unsigned char*) body, arg_len, NULL, NULL, &offs, _out);
        }
        else {
            arg_len = ccnl_ndntlv_prependContent(prefix, (unsigned char*) body, arg_len, NULL, &chunk_num, &offs, _out);
        }

        chunk_num++;

        int len;
        unsigned typ;

        unsigned char *olddata;
        unsigned char *data = olddata = _out + offs;

        if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
            typ != NDN_TLV_Data) {
            puts("EXIT!");
            return;
        }

        struct ccnl_content_s *c = 0;
        printf("Here I should create chunk number %u\n", chunk_num);
        struct ccnl_pkt_s *pk = ccnl_ndntlv_bytes2pkt(typ, olddata, &data, &arg_len);
        c = ccnl_content_new(&ccnl_relay, &pk);
        ccnl_content_add2cache(&ccnl_relay, c);
        c->flags |= CCNL_CONTENT_FLAGS_STATIC;
    }
}

static int _create_cont(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    _create_content();
    return 0;
}

static void _get_content(uint8_t *next_hop)
{
#if USE_BROADCAST
    next_hop = NULL;
#endif

    if (next_hop == NULL) {
        /* initialize address with 0xFF for broadcast */
        uint8_t relay_addr[CCNLRIOT_ADDRLEN];
        memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);
        next_hop = (uint8_t*) relay_addr;
    }

    char pfx[] = CCNLRIOT_PREFIX1;
    ccnlriot_routes_add(pfx, next_hop, CCNLRIOT_ADDRLEN);

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = 0;
    for (unsigned i = 0; i < CCNLRIOT_CHUNKNUMBERS; i++) {
        for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
            char pfx[] = CCNLRIOT_PREFIX1;
            unsigned cn = i;
            printf("cn is %u\n", cn);

            gnrc_netreg_entry_t _ne;
            /* register for content chunks */
            _ne.demux_ctx =  GNRC_NETREG_DEMUX_CTX_ALL;
            _ne.pid = sched_active_pid;
            gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

            ccnl_send_interest(CCNL_SUITE_NDNTLV, pfx, &cn, _int_buf, CCNLRIOT_BUF_SIZE);
            if (ccnl_wait_for_chunk(_cont_buf, CCNLRIOT_BUF_SIZE, 0) > 0) {
                gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
                LOG_DEBUG("Content received: %s\n", _cont_buf);
                success++;
                break;
            }
            gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
        }
    }
    if (success == CCNLRIOT_CHUNKNUMBERS) {
        LOG_WARNING("\n +++ SUCCESS +++\n");
    }
    else {
        LOG_WARNING("\n !!! TIMEOUT !!!\n");
        LOG_DEBUG("Timeout! No content received in response to the Interest for %s.\n", ccnlriot_prefix1);
    }

}

int ccnlriot_stats(int argc, char **argv) {
    (void) argc; (void) argv;
    extern uint32_t rx_cnt, tx_cnt;
    printf("RX: %04" PRIu32 ", TX: %04" PRIu32 "\n", rx_cnt, tx_cnt);
    return 0;
}

static int _send_int(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    int my_id = ccnlriot_get_mypos();
    if (my_id < 0) {
        _get_content(NULL);
    }
    else {
        _get_content(ccnlriot_id[my_id - 1]);
    }
    return 0;
}

int main(void)
{
    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    puts("Basic CCN-Lite example");

    ccnl_core_init();
    ccnl_debug_level = CCNLRIOT_LOGLEVEL;

#ifndef BOARD_NATIVE
    unsigned int res = CCNLRIOT_ADDRLEN;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_SRC_LEN, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting addressing mode\n");
    }

    res = CCNLRIOT_CHANNEL;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_CHANNEL, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting channel\n");
    }

    res = CCNLRIOT_CSMA_RETRIES;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_CSMA_RETRIES, 0, (uint8_t *)&res, sizeof(uint8_t)) < 0) {
        LOG_ERROR("main: error setting csma retries\n");
    }
#endif

    ccnl_relay.max_cache_entries = 20;
    ccnl_start();
    if (ccnl_open_netif(CCNLRIOT_NETIF, GNRC_NETTYPE_CCN) < 0) {
        LOG_ERROR("main: critical error, aborting\n");
        return -1;
    }
#ifndef BOARD_NATIVE
    ccnlriot_routes_setup();
    int my_id = ccnlriot_get_mypos();
    if (my_id == 0) {
        _create_content();
    }
#if USE_AUTOSTART
    else if (my_id >= (CCNLRIOT_NUMBER_OF_NODES - CCNLRIOT_CONSUMERS)) {
        xtimer_usleep(((CCNLRIOT_NUMBER_OF_NODES - my_id) * 1000 * 500));
        LOG_WARNING("\n --------#######************* GETTING CONTENT **************#############---------- \n");
        _get_content(ccnlriot_id[my_id - 1]);
    }
#endif
#endif
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
