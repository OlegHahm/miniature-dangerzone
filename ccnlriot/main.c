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

static unsigned char _out[CCNL_MAX_PACKET_SIZE];
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

static void _create_content(void)
{
    char *body = (char*) CCNLRIOT_CONT;
    int offs = CCNL_MAX_PACKET_SIZE;
    int suite = CCNL_SUITE_NDNTLV;
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(ccnlriot_prefix1, suite, NULL, NULL);

    int arg_len = strlen(CCNLRIOT_CONT) + 1;
    arg_len = ccnl_ndntlv_prependContent(prefix, (unsigned char*) body, arg_len, NULL, NULL, &offs, _out);

    unsigned char *olddata;
    unsigned char *data = olddata = _out + offs;

    int len;
    unsigned typ;

    if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
        typ != NDN_TLV_Data) {
        return;
    }

    struct ccnl_content_s *c = 0;
    struct ccnl_pkt_s *pk = ccnl_ndntlv_bytes2pkt(typ, olddata, &data, &arg_len);
    c = ccnl_content_new(&ccnl_relay, &pk);
    ccnl_content_add2cache(&ccnl_relay, c);
    c->flags |= CCNL_CONTENT_FLAGS_STATIC;
}

static void _get_content(uint8_t *next_hop)
{
#if USE_BROADCAST
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[CCNLRIOT_ADDRLEN];
    memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);
    next_hop = (uint8_t*) relay_addr;
#endif

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);
    char pfx[] = CCNLRIOT_PREFIX1;

    for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
        ccnl_send_interest(CCNL_SUITE_NDNTLV, pfx, next_hop, CCNLRIOT_ADDRLEN, NULL, _int_buf, CCNLRIOT_BUF_SIZE);
        if (ccnl_wait_for_chunk(_cont_buf, CCNLRIOT_BUF_SIZE, 0) > 0) {
            printf("Content received: %s\n", _cont_buf);
            return;
        }
    }
    printf("Timeout! No content received in response to the Interest for %s.\n", ccnlriot_prefix1);

}

int main(void)
{
    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    puts("Basic CCN-Lite example");

    ccnl_core_init();
    extern int debug_level;
    debug_level = DEBUG;

    unsigned int res = CCNLRIOT_ADDRLEN;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_SRC_LEN, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        puts("main: error setting addressing mode");
    }

    res = CCNLRIOT_CHANNEL;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_CHANNEL, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        puts("main: error setting channel");
    }
    ccnl_relay.max_cache_entries = 20;
    ccnl_start();
    if (ccnl_open_netif(CCNLRIOT_NETIF, GNRC_NETTYPE_CCN) < 0) {
        puts("main: critical error, aborting");
        return -1;
    }
    ccnlriot_routes_setup();
    int my_id = ccnlriot_get_mypos();
    if (my_id == 0) {
        _create_content();
    }
#if USE_AUTOSTART
    else if (my_id == (CCNLRIOT_NUMBER_OF_NODES - 1)) {
        xtimer_usleep(1000 * 500);
        puts("\n --------#######************* GETTING CONTENT **************#############---------- \n");
        _get_content(ccnlriot_id[my_id - 1]);
    }
#endif
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
