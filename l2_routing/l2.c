#include <inttypes.h>
#include <stdio.h>

#include "byteorder.h"
#include "thread.h"
#include "msg.h"
#include "kernel.h"
#include "net/ng_pktdump.h"
#include "net/ng_netbase.h"
#include "net/ng_ipv6/addr.h"
#include "net/ng_ipv6/hdr.h"
#include "thread.h"
#include "icn.h"
#include "l2.h"

eui64_t node_ids[NUMBER_OF_NODES] = {
    NODE_01,
    NODE_02,
    NODE_03,
    NODE_04,
    NODE_05,
    NODE_06,
    NODE_07,
    NODE_08,
    NODE_09,
    NODE_10
};

icn_link_t ssf_int[SSF_INT_SIZE] = {
    {&(node_ids[7]), &(node_ids[9])}, // 1
    {&(node_ids[4]), &(node_ids[6])}, // 2
    {&(node_ids[3]), &(node_ids[4])}, // 3
    {&(node_ids[3]), &(node_ids[5])}, // 4
    {&(node_ids[2]), &(node_ids[9])}, // 5
    {&(node_ids[2]), &(node_ids[4])}, // 6
    {&(node_ids[2]), &(node_ids[6])}, // 7
    {&(node_ids[2]), &(node_ids[8])}, // 8
    {&(node_ids[1]), &(node_ids[2])}, // 9
    {&(node_ids[1]), &(node_ids[3])}, // 10
    {&(node_ids[1]), &(node_ids[4])}, // 11
    {&(node_ids[1]), &(node_ids[5])}, // 12
    {&(node_ids[1]), &(node_ids[6])}, // 13
    {&(node_ids[0]), &(node_ids[1])}, // 14
    {&(node_ids[0]), &(node_ids[3])}, // 15
    {&(node_ids[0]), &(node_ids[5])}, // 16
    {&(node_ids[0]), &(node_ids[6])}, // 17
};

icn_routing_entry_t routing_table[RRT_SIZE] = {
    /* default route for 02 over 01 */
    {&(node_ids[1]), NULL, &(node_ids[0])},
    /* default route for 03 over 02 */
    {&(node_ids[2]), NULL, &(node_ids[1])},
    /* default route for 04 over 01 */
    {&(node_ids[3]), NULL, &(node_ids[0])},
    /* default route for 05 over 02 */
    {&(node_ids[4]), NULL, &(node_ids[1])},
    /* default route for 06 over 01 */
    {&(node_ids[5]), NULL, &(node_ids[0])},
    /* default route for 07 over 01 */
    {&(node_ids[6]), NULL, &(node_ids[0])},
    /* default route for 08 over 10 */
    {&(node_ids[7]), NULL, &(node_ids[9])},
    /* default route for 09 over 03 */
    {&(node_ids[8]), NULL, &(node_ids[2])},
    /* default route for 10 over 03 */
    {&(node_ids[9]), NULL, &(node_ids[2])},

    /* route for 01 to 03 over 02 */
    {&(node_ids[0]), &(node_ids[2]), &(node_ids[1])},
    /* route for 01 to 05 over 02 */
    {&(node_ids[0]), &(node_ids[4]), &(node_ids[1])},
};

eui64_t myId;

eui64_t pit_entry;
unsigned pit_ctr = 0;

uint16_t send_counter = 0;
uint16_t receive_counter = 0;

char l2addr_str[3 * 8];

char *interest = "/ndn/RIOT/sensor";

char *content = "Go! are you ready? Start the riot!";

unsigned _linkIsScheduled(eui64_t *dst) {
   for (int i = 0; i < SSF_INT_SIZE; i++) {
       if ((memcmp(&myId, ssf_int[i].sender, ADDR_LEN_64B) == 0) &&
               (memcmp(dst, ssf_int[i].receiver, ADDR_LEN_64B) == 0)) {
           return 1;
       }
   }
   return 0;
}

eui64_t* _routeLookup(eui64_t *dst) {
    eui64_t *next = NULL;
    /* iterate over routing table  */
   for (int i = 0; i < RRT_SIZE; i++) {
       /* find entries for this node */
       if (memcmp(&myId, routing_table[i].id, ADDR_LEN_64B) == 0) {
           /* dedicated route found, use it */
           if (memcmp(dst, routing_table[i].dst, ADDR_LEN_64B) == 0) {
               return routing_table[i].nextHop;
           }
           /* default route found, remember */
           if (routing_table[i].dst == NULL) {
               next = routing_table[i].nextHop;
           }
       }
   }
   /* return either default route or NULL */
   return next;
}

void icn_initContent(eui64_t *lastHop) {
    /* create packet */
    ng_pktsnip_t *pkt;
    icn_hdr_t icn_pkt;
    icn_pkt.type = ICN_CONTENT;
    icn_pkt.seq = send_counter;

    pkt = ng_pktbuf_add(NULL, &icn_pkt, sizeof(icn_hdr_t), NG_NETTYPE_UNDEF);
    pkt = ng_pktbuf_add(pkt, content, strlen(content) + 1, NG_NETTYPE_UNDEF);

    // send interest packet
    icn_send(lastHop, pkt);
}

void icn_initInterest(void) {
    if (WANT_CONTENT) {
#if FLOW_CONTROL
        if (send_counter > (receive_counter + FLOW_THR)) {
            printf("Flow control, send counter is %u, receive counter is %u\n",
                    send_counter, receive_counter);
            return;
        }
#endif
        /* create packet */
        ng_pktsnip_t *pkt;
        icn_hdr_t icn_pkt;
        icn_pkt.type = ICN_INTEREST;
        icn_pkt.seq = send_counter++;
        
        pkt = ng_pktbuf_add(NULL, &icn_pkt, sizeof(icn_hdr_t), NG_NETTYPE_UNDEF);
        pkt = ng_pktbuf_add(pkt, content, strlen(content) + 1, NG_NETTYPE_UNDEF);

        // send interest packet
        printf("Sending packet to %s\n",
                ng_netif_addr_to_str(l2addr_str, sizeof(l2addr_str),
                                     CONTENT_STORE->uint8, ADDR_LEN_64B));

        if (send_counter < NUMBER_OF_CHUNKS) {
            icn_send(CONTENT_STORE, pkt);
        }
#if TIMED_SENDING
        if (send_counter < NUMBER_OF_CHUNKS) {
            vtimer_set_msg(&vt, interval, thread_getpid(), ICN_SEND_INTEREST, NULL);
        }
#endif
    }
    else {
        puts("nothing to do");
    }
}

void icn_send(eui64_t *dst, ng_pktsnip_t *pkt)
{
    ng_netif_hdr_t *nethdr;
    uint8_t flags = 0x00;

    if (!_linkIsScheduled(dst)) {
        puts("No direct neighbor, look for a route");
        eui64_t *tmp;
        tmp = _routeLookup(dst);
        if (tmp == NULL) {
            printf("Not route for %s found\n",
                    ng_netif_addr_to_str(l2addr_str, sizeof(l2addr_str),
                    dst->uint8, ADDR_LEN_64B));
            ng_pktbuf_release(pkt);
            return;
        }
        dst = tmp;
    }

    /* put packet together */
    pkt = ng_pktbuf_add(pkt, NULL, sizeof(ng_netif_hdr_t) + ADDR_LEN_64B,
                        NG_NETTYPE_NETIF);
    nethdr = (ng_netif_hdr_t *)pkt->data;
    ng_netif_hdr_init(nethdr, 0, ADDR_LEN_64B);
    ng_netif_hdr_set_dst_addr(nethdr, dst->uint8, ADDR_LEN_64B);
    nethdr->flags = flags;
    /* and send it */
    ng_netapi_send(*if_id, pkt);
}

static void rcv(ng_pktsnip_t *pkt)
{
    ng_pktsnip_t *netif_pkt = pkt;
    ng_netif_hdr_t *netif_hdr;
    
    netif_pkt = pkt->next;
    if (netif_pkt->type == NG_NETTYPE_NETIF) {
        netif_hdr = netif_pkt->data;
        printf("Received packet from %s\n",
                ng_netif_addr_to_str(l2addr_str, sizeof(l2addr_str),
                    ng_netif_hdr_get_src_addr(netif_hdr),
                    netif_hdr->src_l2addr_len));

        icn_hdr_t *icn_pkt = (icn_hdr_t*) pkt->data;
        switch (icn_pkt->type) {
            case ICN_INTEREST:
                if (HAS_CONTENT) {
                    printf("received interest, have content, sequence number is %u\n",
                            icn_pkt->seq);
                    send_counter = icn_pkt->seq;
                    icn_initContent((eui64_t*)ng_netif_hdr_get_src_addr(netif_hdr));
                    ng_pktbuf_release(pkt);
                }
                else {
                    memcpy(&pit_entry, ng_netif_hdr_get_src_addr(netif_hdr), ADDR_LEN_64B);
                    pit_ctr++;

                    /* forward to CS node */
                    icn_send(CONTENT_STORE, pkt);
                }
                break;
            case ICN_CONTENT:
                if (pit_ctr) {
                    icn_send(&pit_entry, pkt);
                    if (--pit_ctr <= 0) {
                        pit_ctr = 0;
                    }
                }
                else if (WANT_CONTENT) {
                    printf("received content that I wanted with sequence number %u\n",
                            icn_pkt->seq);
                    receive_counter = (receive_counter < icn_pkt->seq) ? icn_pkt->seq : receive_counter;
                   ng_pktbuf_release(pkt);
#if (TIMED_SENDING == 0)
                    if (send_counter < NUMBER_OF_CHUNKS) {
                        icn_initInterest(0);
                    }
#endif
                }
                else {
                    puts("something went wrong");
                    ng_pktbuf_release(pkt);
                }
                break;
            default:
                puts("unexpected packet received");
                ng_pktbuf_release(pkt);
        }
    }
    else {
        puts("unknown snippet");
    }
}

void *_eventloop(void *arg)
{
    (void)arg;
    msg_t msg;
    msg_t msg_queue[NG_PKTDUMP_MSG_QUEUE_SIZE];

    /* setup the message queue */
    msg_init_queue(msg_queue, NG_PKTDUMP_MSG_QUEUE_SIZE);

    while (1) {
        msg_receive(&msg);

        switch (msg.type) {
            case NG_NETAPI_MSG_TYPE_RCV:
                puts("PKTDUMP: data received:");
                rcv((ng_pktsnip_t *)msg.content.ptr);
                break;
            case ICN_SEND_INTEREST:
                puts("ICN_SEND_INTEREST: data received:");
                icn_initInterest();
                break;
            default:
                puts("PKTDUMP: received something unexpected");
                break;
        }
    }

    /* never reached */
    return NULL;
}
