/*
 * Copyright (C) 2016 Oliver Hahm <oliver.hahm@inria.fr>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <string.h>
#include "thread.h"
#include "periph/cpuid.h"
#include "hashes.h"
#include "shell.h"
#include "shell_commands.h"
#include "random.h"
#include "xtimer.h"

#include "net/gnrc.h"

#define CONN_TEST_SEND  (0x4444)

static uint32_t conn_test_id;
static char _stack[THREAD_STACKSIZE_MAIN];
#define CONN_TEST_QUEUE_SIZE     (8)
static msg_t _listener_mq[CONN_TEST_QUEUE_SIZE];

#define CONN_TEST_NETIF         (3)
#define CONN_TEST_CHAN          (26)
#define CONN_TEST_PAYLOAD     "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

typedef struct {
    uint32_t id;
    //char txt[63];
} conn_test_payload_t;

static xtimer_t ct_timer = { .target = 0, .long_target = 0 };
static msg_t ct_m = { .type = CONN_TEST_SEND};

static int _send(int argc, char **argv);

static const shell_command_t shell_commands[] = {
    { "bcast", "sends a fixed amount of data via broadcast", _send},
    { NULL, NULL, NULL }
};

static int _send(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    gnrc_pktsnip_t *pkt, *hdr;
    gnrc_netif_hdr_t *nethdr;
    uint8_t addr[8];
    size_t addr_len = 0;
    uint8_t flags = 0x00;

    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    size_t numof = gnrc_netif_get(ifs);

    for (size_t i = 0; i < numof && i < GNRC_NETIF_NUMOF; i++) {

        flags |= GNRC_NETIF_HDR_FLAGS_BROADCAST;

        /* put packet together */
        conn_test_payload_t payload;
        payload.id = conn_test_id;
     //   memcpy(payload.txt, CONN_TEST_PAYLOAD, sizeof(CONN_TEST_PAYLOAD));

        pkt = gnrc_pktbuf_add(NULL, &payload, sizeof(payload), GNRC_NETTYPE_UNDEF);
        hdr = gnrc_netif_hdr_build(NULL, 0, addr, addr_len);
        LL_PREPEND(pkt, hdr);
        nethdr = (gnrc_netif_hdr_t *)hdr->data;
        nethdr->flags = flags;
        /* and send it */
        if (gnrc_netapi_send(ifs[i], pkt) < 1) {
            puts("error: unable to send\n");
            gnrc_pktbuf_release(pkt);
        }
    }

    return 0;
}

static void *_listener(void *unused)
{
    msg_init_queue(_listener_mq, 8);
    msg_t msg;
    while (1) {
        msg_receive(&msg);
        gnrc_pktsnip_t *snip = NULL;

        switch (msg.type) {
            case GNRC_NETAPI_MSG_TYPE_RCV:
                {
                gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *)msg.content.ptr;
                snip = pkt;
                while (snip != NULL) {
                    if (snip->type == GNRC_NETTYPE_NETIF) {
                        //gnrc_netif_hdr_print(snip->data);
                    }
                    else if (snip->type == GNRC_NETTYPE_UNDEF) {
                        printf("ID: %08lX\n",
                               (unsigned long) ((conn_test_payload_t*)snip->data)->id);
                    }
                    snip = snip->next;
                }
                gnrc_pktbuf_release(pkt);
                break;
                }
            case CONN_TEST_SEND:
                _send(0, NULL);
                xtimer_set_msg(&ct_timer, (SEC_IN_USEC * 3) + (random_uint32()
                                                               & 0x001FFFFF),
                               &ct_m, sched_active_pid);
                break;
            default:
                puts("UNEXPECTED MESSAGE TYPE!");
        }
    }

    return NULL;
}

int main(void)
{
    gnrc_netreg_entry_t ne;

    uint8_t cpuid[CPUID_LEN];
    cpuid_get(cpuid);
    conn_test_id = djb2_hash(cpuid, CPUID_LEN);
    random_init(conn_test_id);

    ne.pid = thread_create(_stack, sizeof(_stack), THREAD_PRIORITY_MAIN - 1,
                             THREAD_CREATE_STACKTEST, _listener, NULL,
                             "listener");

    ne.demux_ctx = GNRC_NETREG_DEMUX_CTX_ALL;
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &ne);

    puts("Connectivity Test program!");
    printf("MY ID: %08lX\n", (unsigned long) conn_test_id);
    unsigned res = CONN_TEST_CHAN;
    if (gnrc_netapi_set(CONN_TEST_NETIF, NETOPT_CHANNEL, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        puts("main: error setting channel");
    }

    unsigned int addr_len = 8;
    if (gnrc_netapi_set(CONN_TEST_NETIF, NETOPT_SRC_LEN, 0, (uint16_t *)&addr_len, sizeof(uint16_t)) < 0) {
        printf("main: error setting addressing mode\n");
    }
    xtimer_set_msg(&ct_timer, (SEC_IN_USEC * 3) + (random_uint32() & 0x001FFFFF), &ct_m, ne.pid);

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
