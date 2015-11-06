/*
 * Copyright (C) 2015 Freie Universität Berlin
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
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "byteorder.h"
#include "thread.h"
#include "net/af.h"
#include "net/ipv6/addr.h"
#include "net/conn.h"
#include "net/conn/udp.h"

#include "shell.h"
#include "msg.h"

#include "coap.h"

#include "coapping.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
#define COAP_QUEUE_SIZE     (8)
static msg_t _coap_msg_queue[COAP_QUEUE_SIZE];

static char _server_stack[THREAD_STACKSIZE_MAIN];

extern int udp_cmd(int argc, char **argv);
extern int coap_cmd(int argc, char **argv);

static const shell_command_t shell_commands[] = {
    { "udp", "send data over UDP and listen on UDP ports", udp_cmd },
    { "coap", "sends a CoAP ping", coap_cmd},
    { NULL, NULL, NULL }
};

static void *_coap_server(void *dummy)
{
    msg_init_queue(_coap_msg_queue, COAP_QUEUE_SIZE);
    (void) dummy;
    conn_udp_t conn;
    uint8_t buf[COAPPING_SIZE];

    ipv6_addr_t srcaddr = IPV6_ADDR_UNSPECIFIED;
    ipv6_addr_t raddr;
    uint16_t rport;

    conn_udp_create(&conn, &srcaddr, sizeof(ipv6_addr_t), AF_INET6, COAPPING_UDP_PORT);

    while (1) {
        size_t addr_len = sizeof(ipv6_addr_t);
        int n = conn_udp_recvfrom(&conn, buf, sizeof(buf), &raddr, &addr_len, &rport);
        int rc;
		coap_packet_t pkt;
        if (0 != (rc = coap_parse(&pkt, buf, n))) {
            printf("Bad packet rc=%d\n", rc);
        }
        else {
			size_t rsplen = COAPPING_SIZE; 
            coap_packet_t rsppkt;
            memset(&rsppkt, 0, sizeof(coap_packet_t));
            printf("Content:\n");
            coap_dumpPacket(&pkt);
            puts("");
            if ((pkt.hdr.t == COAP_TYPE_CON) && (pkt.hdr.code == 0)) {
                rsppkt.hdr.ver = 0x01;
                rsppkt.hdr.t = COAP_TYPE_RESET;
                rsppkt.hdr.tkl = 0;
                rsppkt.hdr.code = 0;
                rsppkt.hdr.id[0] = pkt.hdr.id[0];
                rsppkt.hdr.id[1] = pkt.hdr.id[1];
                rsppkt.numopts = 1;

                if (0 != (rc = coap_build(buf, &rsplen, &rsppkt)))
                    printf("coap_build failed rc=%d\n", rc);
                else
                {
                    printf("Sending packet: ");
                    coap_dump(buf, rsplen, true);
                    printf("\n");
                    printf("content:\n");
                    coap_dumpPacket(&rsppkt);
                    conn_udp_sendto(buf, COAPPING_UDP_PORT, NULL, 0, &raddr,
                                    sizeof(ipv6_addr_t), AF_INET6,
                                    COAPPING_UDP_PORT, byteorder_htons(rport).u16);
                }
            }
        }
    }

    return NULL;
}

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    thread_create(_server_stack, sizeof(_server_stack), THREAD_PRIORITY_MAIN -
                  1, CREATE_STACKTEST, _coap_server, NULL, "CoAP");

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
