#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msg.h"
#include "sixlowpan/ip.h"
#include "transceiver.h"
#include "ieee802154_frame.h"
#include "rpl_structs.h"

#include "demo.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define LL_HDR_LEN  (0x4)
#define IPV6_HDR_LEN    (0x28)

extern uint8_t ipv6_ext_hdr_len;

msg_t msg_q[RCV_BUFFER_SIZE];

/* prints current IPv6 adresses */
void ip(char *unused)
{
    (void) unused;
    ipv6_iface_print_addrs();
}

void set_id(char *id_str)
{
    int res = sscanf(id_str, "set %hu", &id);

    if (res < 1) {
        printf("Usage: init address\n");
        printf("\taddress must be an 8 bit integer\n");
        printf("\n\t(Current address is %u)\n", id);
        return;
    }

    printf("Set node ID to %u\n", id);
}

void monitor(void)
{
    msg_t m;
    radio_packet_t *p;
    ieee802154_frame_t *frame;
    ipv6_hdr_t *ipv6_buf;
    uint8_t icmp_type, icmp_code;
    icmpv6_hdr_t *icmpv6_buf = NULL;
    radio_address_t last_sender = 0;

    msg_init_queue(msg_q, RCV_BUFFER_SIZE);

    while (1) {
        msg_receive(&m);
        if (m.type == PKT_PENDING) {
            p = (radio_packet_t*) m.content.ptr;
            /* m: ID X received msg TYPE from ID Y #color */
            frame = (ieee802154_frame_t*) p->data;

            /*
            printf("m: ID sn%u", id);
            printf(" received msg %04hu", frame->payload_len); 
            printf(" from ID sn%u #color3", p->src);
            */
            last_sender = p->src;
            /*
            printf("Length:\t%u\n", p->length);
            printf("\tSrc:\t%u\n", p->src);
            printf("\tDst:\t%u\n", p->dst);
            */
            DEBUG("Packet from %u received:", last_sender);
            DEBUG("\tLQI:\t%u", p->lqi);
            DEBUG("\tRSSI:\t%i", (int8_t) p->rssi);

            /*
            for (uint8_t i = 0; i < p->length; i++) {
                printf("%02X ", p->data[i]);
            }
            */
            p->processing--;
            DEBUG("\n");
        }
        else if (m.type == IPV6_PACKET_RECEIVED) {
            ipv6_buf = (ipv6_hdr_t*) m.content.ptr;
            printf("m: ID sn%u", id);
            printf(" received msg %02X", ipv6_buf->nextheader); 
            printf(" from ID sn%u ", last_sender);
            if (ipv6_buf->nextheader == IPV6_PROTO_NUM_ICMPV6) {
                icmpv6_buf = (icmpv6_hdr_t*) &ipv6_buf[(LL_HDR_LEN + IPV6_HDR_LEN) + ipv6_ext_hdr_len];
                icmp_type = icmpv6_buf->type;
                icmp_code = icmpv6_buf->code;
                if (icmp_code == ICMP_CODE_DIO) {
                    printf("#color6 ");
                }
                else {
                    printf("#color6 ");
                }
            }
            else if (ipv6_buf->nextheader == IPV6_PROTO_NUM_ICMPV6) {
                printf("#color30 ");
            }
            else {
                printf("#color5 ");
            }

            DEBUG("\t origin: %s", ipv6_addr_to_str(addr_str, &ipv6_buf->srcaddr));
            DEBUG("\t destination: %s", ipv6_addr_to_str(addr_str, &ipv6_buf->destaddr));
            if (ipv6_buf->nextheader == IPV6_PROTO_NUM_ICMPV6) {
                DEBUG("\t ICMP type: %02X ", icmp_type);
                DEBUG("\t ICMP code: %02X ", icmp_code);
            }
            printf("\n");
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
