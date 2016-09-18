/*
 * Copyright (C) 2016 Freie Universität Berlin
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
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "byteorder.h"
#include "msg.h"
#include "sema.h"
#include "net/protnum.h"
#include "thread.h"
#include "xtimer.h"
#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"

#include "netdev.h"
#include "stack.h"

#include "exp.h"

#define PAYLOAD_SIZE    (4)
#define PFX_LEN (76 + 1)
#define BUF_SIZE (134)
#define IEEE802154_MAX_FRAME_SIZE   (125U)
#define MHR_LEN                     (23U)
#define THREAD_PRIO                 (THREAD_PRIORITY_MAIN - 1)
#define THREAD_STACK_SIZE           (THREAD_STACKSIZE_DEFAULT + \
                                     THREAD_EXTRA_STACKSIZE_PRINTF)
#define THREAD_MSG_QUEUE_SIZE       (8)

char* ccnl_prefix_to_path_detailed(struct ccnl_prefix_s *pr,
                                   int ccntlv_skip, int escape_components, int call_slash);
#define ccnl_prefix_to_path(P) ccnl_prefix_to_path_detailed(P, 1, 0, 0)

static netdev2_t const *netdev = (netdev2_t *)&netdevs[0];
static uint8_t dst_l2[] = NETDEV_ADDR_PREFIX;
static const uint8_t src_l2[] = EXP_ADDR_L2;
static uint32_t mytimer;
static uint8_t ccnl_buf[IEEE802154_MAX_FRAME_SIZE];
static size_t ccnl_buf_len;
static char thread_stack[THREAD_STACK_SIZE];
static msg_t thread_msg_queue[THREAD_MSG_QUEUE_SIZE];
static sema_t sema_sync = SEMA_CREATE(0);

static char _pfx_url[PFX_LEN];
static char _tmp_url[PFX_LEN];
static unsigned char _int_buf[BUF_SIZE];
static unsigned char _cont_buf[BUF_SIZE];
static unsigned char _out[BUF_SIZE];
static struct ccnl_face_s *loopback_face;
struct ccnl_interest_s *_tmp_int;

static void _create_interest(struct ccnl_prefix_s *prefix);
static void _init_chunk(struct ccnl_prefix_s *pfx);
static void prepare_mhrs(void);

static void _netdev_isr(netdev2_t *dev)
{
    dev->event_callback(dev, NETDEV2_EVENT_RX_COMPLETE);
}

static int _netdev_recv(netdev2_t *dev, char *buf, int len, void *info)
{
    (void)dev;
    (void)len;
    if (buf != NULL) {
        mytimer = xtimer_now();
        if (info != NULL) {
            netdev2_ieee802154_rx_info_t *radio_info = info;
            radio_info->rssi = 255;
            radio_info->lqi = 35;
        }
        memcpy(buf, ccnl_buf, ccnl_buf_len);
        sema_post(&sema_sync);       /* signal that fragment was sent */
    }
    return ccnl_buf_len;
}

void *_thread(void *arg)
{
    (void)arg;
    msg_init_queue(thread_msg_queue, THREAD_MSG_QUEUE_SIZE);

    memset(_cont_buf, '\0', BUF_SIZE);
    gnrc_netreg_entry_t _ne;
    /* register for content chunks */
    _ne.demux_ctx =  GNRC_NETREG_DEMUX_CTX_ALL;
    _ne.pid = sched_active_pid;
    gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

    while (1) {
        int res;
        

        if ((res = ccnl_wait_for_chunk(_cont_buf, BUF_SIZE, 1000000)) > 0) {
            uint32_t stop = xtimer_now();
#ifndef EXP_STACKTEST
            printf("%d,%" PRIu32 "\n", (unsigned) ccnl_buf_len,
                   stop - mytimer);
#else
            (void)stop;
            (void)id;
#endif
        }
        else {
            /* find a reason to stop to not have conn_udp_close() optimized out
             */
            puts("waiting for content failed.");
        }
    }
    puts("shouldn't happen");
    gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
    /* for size comparison include remaining conn_udp functions */
    return NULL;
}

void exp_run(void)
{
    dst_l2[7] = 0;

    struct ccnl_prefix_s *prefix = NULL;
    struct ccnl_prefix_s *prefix_int = NULL;
    uint16_t comp_cnt;

    ccnl_relay.max_cache_entries = 0;
    prepare_mhrs();
    loopback_face = ccnl_get_face_or_create(&ccnl_relay, -1, NULL, 0);
    if (thread_create(thread_stack, sizeof(thread_stack), THREAD_PRIO,
                      THREAD_CREATE_STACKTEST, _thread, NULL, "exp_receiver") < 0) {
        return;
    }
    netdev2_test_set_isr_cb(&netdevs[0], _netdev_isr);
    netdev2_test_set_recv_cb(&netdevs[0], _netdev_recv);
#ifdef EXP_STACKTEST
    puts("thread,stack_size,stack_free");
#else
    puts("payload_len,rx_traversal_time");
#endif
    for (comp_cnt = 0; comp_cnt < (PFX_LEN / 4); comp_cnt++) {
        free_prefix(prefix);
        _pfx_url[comp_cnt * 4] = '/';
        memcpy(&_pfx_url[(comp_cnt * 4) + 1], EXP_NAME_COMP, strlen(EXP_NAME_COMP) + 1);
        memcpy(_tmp_url, _pfx_url, PFX_LEN);
        prefix = ccnl_URItoPrefix(_tmp_url, CCNL_SUITE_NDNTLV, NULL, 0);
        memcpy(_tmp_url, _pfx_url, PFX_LEN);
        prefix_int = ccnl_URItoPrefix(_tmp_url, CCNL_SUITE_NDNTLV, NULL, 0);
        _init_chunk(prefix);

        for (unsigned id = 0; id < EXP_RUNS; id++) {
            _create_interest(prefix_int);
            ccnl_interest_append_pending(_tmp_int, loopback_face);

            char *tmp_pfx = ccnl_prefix_to_path(prefix);
            //printf("%u/%u: Send interest for %s\n", comp_cnt, (unsigned) id, tmp_pfx);
            ccnl_free(tmp_pfx);

            netdev->event_callback((netdev2_t *)netdev, NETDEV2_EVENT_ISR);
            sema_wait(&sema_sync);   /* wait for fragment to be processed by
                                 * netdev2 */
#if EXP_PACKET_DELAY
            xtimer_usleep(EXP_PACKET_DELAY);
#endif
        }
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

static void _create_interest(struct ccnl_prefix_s *prefix)
{
    extern int ndntlv_mkInterest(struct ccnl_prefix_s *name, int *nonce, unsigned char *out, int outlen);
    int len = ndntlv_mkInterest(prefix, NULL, _int_buf, BUF_SIZE);

    unsigned char *start = _int_buf;
    unsigned char *data = _int_buf;
    struct ccnl_pkt_s *pkt;

    int typ;
    int int_len;

    if (ccnl_ndntlv_dehead(&data, &len, (int*) &typ, &int_len) || (int) int_len > len) {
        printf("ccnl_helper: invalid packet format\n");
        return;
    }
    pkt = ccnl_ndntlv_bytes2pkt(NDN_TLV_Interest, start, &data, &len);

    _tmp_int = ccnl_interest_new(&ccnl_relay, loopback_face, &pkt);
}


static void _init_chunk(struct ccnl_prefix_s *pfx)
{
    puts("init chunk");
    int offs = CCNL_MAX_PACKET_SIZE;

    unsigned char dummy_payload[PAYLOAD_SIZE];
    ssize_t len = PAYLOAD_SIZE;

    ssize_t arg_len = ccnl_ndntlv_prependContent(pfx, dummy_payload, len, NULL, NULL, &offs, _out);

    unsigned char *olddata;
    unsigned char *data = olddata = _out + offs;
    unsigned typ;

    if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
        typ != NDN_TLV_Data) {
        return;
    }

    struct ccnl_content_s *c = 0;
    struct ccnl_pkt_s *pk = ccnl_ndntlv_bytes2pkt(typ, olddata, &data, &arg_len);
    if (pk == NULL) {
        printf("ccnl_helper: something went terribly wrong!\n");
        return;
    }
    c = ccnl_content_new(&ccnl_relay, &pk);
    memcpy((ccnl_buf + MHR_LEN), c->pkt->buf->data, c->pkt->buf->datalen);
    ccnl_buf_len = c->pkt->buf->datalen + MHR_LEN;

    void free_packet(struct ccnl_pkt_s *pkt);
    free_packet(c->pkt);
    ccnl_free(c);
}

static void prepare_mhrs(void)
{
    const le_uint16_t pan_id = byteorder_btols(byteorder_htons(NETDEV_PAN_ID));
    ieee802154_set_frame_hdr(ccnl_buf, src_l2, sizeof(src_l2), dst_l2,
                             sizeof(dst_l2), pan_id, pan_id,
                             IEEE802154_FCF_TYPE_DATA | IEEE802154_FCF_ACK_REQ, 0);
}

/** @} */
