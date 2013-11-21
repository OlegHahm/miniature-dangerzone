#include <stdio.h>
#include "msg.h"
#include "sixlowpan/ip.h"
#include "transceiver.h"
#include "ieee802154_frame.h"

#include "demo.h"

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

            printf("m: ID %04X", id);
            printf(" received msg %hu", frame->payload_len); 
            printf(" from ID %04X", p->src);
            /*
            printf("Length:\t%u\n", p->length);
            printf("\tSrc:\t%u\n", p->src);
            printf("\tDst:\t%u\n", p->dst);
            printf("\tLQI:\t%u\n", p->lqi);
            printf("\tRSSI:\t%u\n", p->rssi);

            for (uint8_t i = 0; i < p->length; i++) {
                printf("%02X ", p->data[i]);
            }
            */
            p->processing--;
            printf("\n");
        }
        if (m.type == PKT_PENDING) {
            ipv6_buf = (ipv6_hdr_t*) m.content.ptr;
            printf("m: ADDR %s", ipv6_addr_to_str(addr_str, &ipv6_buf->srcaddr));
            printf(" received msg %02X", ipv6_buf->nextheader); 
            printf(" from ADDR %s\n", ipv6_addr_to_str(addr_str, &ipv6_buf->destaddr));
        }
        else if (m.type == ENOBUFFER) {
            puts("Transceiver buffer full");
        }
        else {
            puts("Unknown packet received");
        }
    }
}


