#include <stdio.h>
#include <string.h>

#include "byteorder.h"
#include "net/af.h"
#include "net/ipv6/addr.h"
#include "net/conn.h"
#include "net/conn/udp.h"
#include "coap.h"

#include "coapping.h"

uint16_t msg_seq_cnt = 0;
uint8_t cbuf[COAPPING_SIZE];

int coap_cmd(int argc, char **argv)
{
    ipv6_addr_t addr;
    coap_packet_t cpkt;
    memset(&cpkt, 0, sizeof(coap_packet_t));

    if (argc < 2) {
        printf("usage: %s <IPv6>\n", argv[0]);
    }

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, argv[1]) == NULL) {
        puts("Error: unable to parse destination address");
        return 1;
    }

    cpkt.hdr.ver = 0x01;
    cpkt.hdr.t = COAP_TYPE_CON;
    cpkt.hdr.tkl = 0;
    cpkt.hdr.code = 0;
    cpkt.numopts = 1;
    memcpy(&(cpkt.hdr.id), byteorder_htonl(msg_seq_cnt).u16, sizeof(uint16_t));
    size_t buflen = COAPPING_SIZE;

    coap_build(cbuf, &buflen, &cpkt);

    conn_udp_sendto(cbuf, COAPPING_SIZE, NULL, 0, &addr, sizeof(ipv6_addr_t),
                    AF_INET6, COAPPING_UDP_PORT, COAPPING_UDP_PORT);
    msg_seq_cnt++;

    return 0;
}
