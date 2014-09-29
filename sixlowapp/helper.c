/*
 * Copyright (C) 2014 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup examples
 * @{
 *
 * @file  helper.c
 * @brief 6LoWPAN example application helper functions
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msg.h"
#include "sixlowpan/ip.h"
#include "transceiver.h"
#include "ieee802154_frame.h"
#include "inet_pton.h"

#include "sixlowapp.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define IPV6_HDR_LEN    (0x28)

extern uint8_t ipv6_ext_hdr_len;

msg_t msg_q[RCV_BUFFER_SIZE];

static unsigned waiting_for_pong;

void sixlowapp_send_ping(int argc, char **argv)
{
    ipv6_addr_t dest;
    const char *icmp_data = ICMP_DATA;
    
    if (argc < 2) {
        puts("! Not enough parameters");
        return;
    }

    if (!inet_pton(AF_INET6, argv[1], &dest)) {
        printf("! %s is not a valid IPv6 address\n", argv[1]);
        return;
    }
    
    /* add the destination to the neighbor cache if is not already in it */
    if (!ndp_neighbor_cache_search(&dest)) {
    ndp_neighbor_cache_add(IF_ID, &dest, &(dest.uint16[7]), 2, 0,
                           NDP_NCE_STATUS_REACHABLE,
                           NDP_NCE_TYPE_TENTATIVE,
                           0xffff);
    }

    /* send an echo request */
    icmpv6_send_echo_request(&dest, 1, 1, (uint8_t *) icmp_data, sizeof(icmp_data));

    waiting_for_pong = 1;
    unsigned rtt = 0;
    while (rtt++ < ICMP_TIMEOUT) {
        if (waiting_for_pong == 0) {
            printf("Echo reply from %s received, rtt: %u ms\n", inet_ntop(AF_INET6,
                                                                          &dest,
                                                                          addr_str,
                                                                          IPV6_MAX_ADDR_STR_LEN),
                                                                rtt);
            break;
        }
        vtimer_usleep(1000);
    }
    if (waiting_for_pong) {
        printf("! Destination %s is unreachable\n", inet_ntop(AF_INET6,
                                                              &dest,
                                                              addr_str,
                                                              IPV6_MAX_ADDR_STR_LEN));
    }
}

void sixlowapp_netcat(int argc, char **argv)
{
    ipv6_addr_t dest;
    
    if (argc < 2) {
        puts("! Not enough parameters");
        puts("  usage: nc [-l] [destination] [port]");
        return;
    }

    if (strlen(argv[1] == 2)) {
        if (strncmp(argv[1], "-l", 2)) {
            puts("! Invalid parameter");
            puts("  usage: nc [-l] [destination] [port]");
            return;
        }
        else {
        }
    }
    if (!inet_pton(AF_INET6, argv[1], &dest)) {
        printf("! %s is not a valid IPv6 address\n", argv[1]);
        return;
    }
}

void *sixlowapp_monitor(void *unused)
{
    (void) unused;

    msg_t m;
    radio_packet_t *p;
    ipv6_hdr_t *ipv6_buf;

    msg_init_queue(msg_q, RCV_BUFFER_SIZE);

    while (1) {
        msg_receive(&m);

        if (m.type == PKT_PENDING) {
            p = (radio_packet_t *) m.content.ptr;

            DEBUGF("Received packet from ID %u\n", p->src);
            DEBUG("\tLength:\t%u\n", p->length);
            DEBUG("\tSrc:\t%u\n", p->src);
            DEBUG("\tDst:\t%u\n", p->dst);
            DEBUG("\tLQI:\t%u\n", p->lqi);
            DEBUG("\tRSSI:\t%i\n", (int8_t) p->rssi);

            for (uint8_t i = 0; i < p->length; i++) {
                DEBUG("%02X ", p->data[i]);
            }

            p->processing--;
            DEBUG("\n");
        }
        else if (m.type == IPV6_PACKET_RECEIVED) {
            ipv6_buf = (ipv6_hdr_t *) m.content.ptr;

            if (ipv6_buf->nextheader == IPV6_PROTO_NUM_ICMPV6) {
                icmpv6_hdr_t *icmpv6_buf = (icmpv6_hdr_t *) &((uint8_t*)ipv6_buf)[(IPV6_HDR_LEN)];
                if (icmpv6_buf->type == ICMPV6_TYPE_ECHO_REPLY) {
                    waiting_for_pong = 0;
                }
            }
            /* add the destination to the neighbor cache if is not already in it */
            if (!ndp_neighbor_cache_search(&(ipv6_buf->srcaddr))) {
                    ndp_neighbor_cache_add(IF_ID, &(ipv6_buf->srcaddr), &(ipv6_buf->srcaddr.uint16[7]), 2, 0,
                        NDP_NCE_STATUS_REACHABLE,
                        NDP_NCE_TYPE_TENTATIVE,
                        0xffff);
            }
            DEBUGF("IPv6 datagram received (next header: %02X)", ipv6_buf->nextheader);
            DEBUG(" from %s\n", ipv6_addr_to_str(addr_str, IPV6_MAX_ADDR_STR_LEN,
                                                 &ipv6_buf->srcaddr));
        }
        else if (m.type == ENOBUFFER) {
            puts("! Transceiver buffer full");
        }
        else {
            printf("! Unknown message received, type %04X\n", m.type);
        }
    }
}
