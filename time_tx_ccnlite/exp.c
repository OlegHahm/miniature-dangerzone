/*
 * Copyright (C) 2016 Freie Universität Berlin
 * Copyright (C) 2016 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author Martine Lenders <mlenders@inf.fu-berlin.de>
 * @author Oliver Hahm <oliver.hahm@inria.fr>
 */

#include <inttypes.h>

#include "net/netdev2_test.h"
#include "thread.h"
#include "xtimer.h"

#include "net/gnrc/netapi.h"
#include "net/gnrc/pktbuf.h"
#include "netdev.h"
#include "stack.h"
#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"

#include "exp.h"

#define BUF_SIZE (134)
#define PFX_LEN (96 + 1)

static uint32_t start_time;

static unsigned char _int_buf[BUF_SIZE];
static char _pfx_url[PFX_LEN];
static char _tmp_url[PFX_LEN];
char* ccnl_prefix_to_path_detailed(struct ccnl_prefix_s *pr,
                                   int ccntlv_skip, int escape_components, int call_slash);
#define ccnl_prefix_to_path(P) ccnl_prefix_to_path_detailed(P, 1, 0, 0)

static int _netdev2_send(netdev2_t *dev, const struct iovec *vector, int count)
{
    /* first things first */
    const uint32_t stop = xtimer_now();
    int res = 0;

    (void)dev;
    for (int i = 0; i < count; i++) {
        res += vector[i].iov_len;
    }

#ifndef EXP_STACKTEST
    extern uint32_t ccnl_t1, ccnl_t2;
    printf("%u,%" PRIu32 ", %" PRIu32 "\n", (unsigned)res, (stop - start_time), ((ccnl_t1 - start_time) + (stop - ccnl_t2)));
#else
    (void)stop;
    (void)start;
#endif
    return res;
}

void exp_run(void)
{
    uint16_t comp_cnt;
    netdev2_test_set_send_cb(&netdevs[0], _netdev2_send);
    struct ccnl_prefix_s *prefix = NULL;
    ccnl_interest_t riot_int;
    gnrc_pktsnip_t *pkt;
    unsigned end = EXP_RUNS * 3;

#ifdef EXP_STACKTEST
    puts("thread,stack_size,stack_free");
#else
    puts("payload_len,tx_traversal_time");
#endif
    for (comp_cnt = 0; comp_cnt < (PFX_LEN / 4); comp_cnt++) {
        free_prefix(prefix);
        _pfx_url[comp_cnt * 3] = '/';
        memcpy(&_pfx_url[(comp_cnt * 3) + 1], EXP_NAME_COMP, strlen(EXP_NAME_COMP) + 1);
        memcpy(_tmp_url, _pfx_url, PFX_LEN);
        prefix = ccnl_URItoPrefix(_tmp_url, CCNL_SUITE_NDNTLV, NULL, 0);
        for (unsigned id = 0; id < end; id++) {
            if (ccnl_relay.pit != NULL) {
                ccnl_interest_remove(&ccnl_relay, ccnl_relay.pit);
            }

            char *tmp_pfx = ccnl_prefix_to_path(prefix);
            //printf("%u/%u: Send interest for %s\n", comp_cnt, (unsigned) id, tmp_pfx);
            ccnl_free(tmp_pfx);
            start_time = xtimer_now();
            riot_int.prefix = prefix;
            riot_int.buf = _int_buf;
            riot_int.buflen = BUF_SIZE;
            pkt = gnrc_pktbuf_add(NULL, &riot_int, sizeof(riot_int), GNRC_NETTYPE_CCN);
            gnrc_netapi_send(stack_ccnl_pid, pkt);

            //i = ccnl_send_interest(prefix, _int_buf, BUF_SIZE);
#if EXP_PACKET_DELAY
            xtimer_usleep(EXP_PACKET_DELAY);
#endif
            if (comp_cnt > 0) {
                end = EXP_RUNS;
            }
        }
    }
            if (ccnl_relay.pit != NULL) {
                ccnl_interest_remove(&ccnl_relay, ccnl_relay.pit);
            }
#ifdef EXP_STACKTEST
    for (kernel_pid_t i = 0; i <= KERNEL_PID_LAST; i++) {
        const thread_t *p = (thread_t *)sched_threads[i];
        if ((p != NULL) &&
            (strcmp(p->name, "idle") != 0) &&
            (strcmp(p->name, "main") != 0) &&
            (strcmp(p->name, "exp_receiver") != 0)) {
            printf("%s,%u,%u\n", p->name, p->stack_size,
                   thread_measure_stack_free(p->stack_start));
        }
    }
#endif
}

/** @} */
