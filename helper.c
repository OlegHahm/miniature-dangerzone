#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msg.h"
#include "sixlowpan/ip.h"
#include "transceiver.h"
#include "ieee802154_frame.h"

#include "demo.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

msg_t msg_q[RCV_BUFFER_SIZE];

/* prints current IPv6 adresses */
void ip(char *unused)
{
    (void) unused;
    ipv6_iface_print_addrs();
}

void monitor(void) {
    msg_t m;
    radio_packet_t *p;
    ieee802154_frame_t *frame;
    ipv6_hdr_t *ipv6_buf;

    msg_init_queue(msg_q, RCV_BUFFER_SIZE);

    while (1) {
        msg_receive(&m);
        if (m.type == PKT_PENDING) {
            p = (radio_packet_t*) m.content.ptr;
            /* m: ID X received msg TYPE from ID Y #color */
            frame = (ieee802154_frame_t*) p->data;

            printf("m: ID sn%u", id);
            printf(" received msg %04hu", frame->payload_len); 
            printf(" from ID sn%u #color3", p->src);
            /*
            printf("Length:\t%u\n", p->length);
            printf("\tSrc:\t%u\n", p->src);
            printf("\tDst:\t%u\n", p->dst);
            */
            DEBUG("\tLQI:\t%u", p->lqi);
            DEBUG("\tRSSI:\t%u", p->rssi);

            /*
            for (uint8_t i = 0; i < p->length; i++) {
                printf("%02X ", p->data[i]);
            }
            */
            p->processing--;
            printf("\n");
        }
        else if (m.type == IPV6_PACKET_RECEIVED) {
            ipv6_buf = (ipv6_hdr_t*) m.content.ptr;
            printf("m: ADDR %s", ipv6_addr_to_str(addr_str, &ipv6_buf->srcaddr));
            printf(" received msg %02X", ipv6_buf->nextheader); 
            printf(" from ADDR %s\n", ipv6_addr_to_str(addr_str, &ipv6_buf->destaddr));
        }
        else if (m.type == ENOBUFFER) {
            puts("Transceiver buffer full");
        }
        else {
            printf("Unknown packet received, type %04X\n", m.type);
        }
    }
}

transceiver_command_t tcmd;

void ignore(char *addr) {
    uint16_t a;
    msg_t mesg;
    mesg.type = DBG_IGN;
    mesg.content.ptr = (char*) &tcmd;

    tcmd.transceivers = TRANSCEIVER_CC1100;
    tcmd.data = &a;
    a = atoi(addr + strlen("ign "));
    if (strlen(addr) > strlen("ign ")) {
        printf("sending to transceiver (%u): %u\n", transceiver_pid, (*(uint8_t*)tcmd.data));
        msg_send(&mesg, transceiver_pid, 1);
    }
    else {
        puts("Usage:\tign <addr>");
    }
}
