#include <stdio.h>
#include <string.h>

#include "msg.h"
#include "byteorder.h"
#include "net/af.h"
#include "net/ipv6/addr.h"
#include "net/conn.h"
#include "net/conn/udp.h"
#include "coap.h"

#define COAPPING_SIZE       (4)
#define COAPPING_UDP_PORT   (5683)
#define DEFAULT_SOURCE_PORT (61616)
#define COAP_QUEUE_SIZE     (8)

static msg_t _coap_msg_queue[COAP_QUEUE_SIZE];

static char _server_stack[THREAD_STACKSIZE_MAIN];

uint16_t msg_seq_cnt = 0;
uint8_t cbuf[COAPPING_SIZE];
uint8_t server_running = false;
kernel_pid_t coap_server_pid;
uint16_t srcport;

void *_coap_server(void *src_port)
{
    msg_init_queue(_coap_msg_queue, COAP_QUEUE_SIZE);
    conn_udp_t conn;
    uint8_t buf[COAPPING_SIZE];

    ipv6_addr_t srcaddr = IPV6_ADDR_UNSPECIFIED;
    srcport = *((uint16_t*) src_port);
    ipv6_addr_t raddr;
    uint16_t rport;

    printf("Starting a CoAP ping server at port %" PRIu16 "\n", srcport);

    coap_server_pid = conn_udp_create(&conn, &srcaddr, sizeof(ipv6_addr_t), AF_INET6, srcport);

    while (server_running) {
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
            if (pkt.hdr.t == COAP_TYPE_RESET) {
                puts("Received CoAP RESET");
            }
            else if ((pkt.hdr.t == COAP_TYPE_CON) && (pkt.hdr.code == 0)) {
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
                    conn_udp_sendto(buf, COAPPING_SIZE, NULL, 0, &raddr,
                                    sizeof(ipv6_addr_t), AF_INET6,
                                    srcport, byteorder_htons(rport).u16);
                }
            }
            else {
                puts("Received anything else");
            }
        }
    }
    puts("stopping server");

    return NULL;
}

int coap_server_cmd(int argc, char **argv)
{
    uint16_t port = DEFAULT_SOURCE_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    server_running = true;
    thread_create(_server_stack, sizeof(_server_stack), THREAD_PRIORITY_MAIN -
                  1, CREATE_STACKTEST, _coap_server, (void*) &port, "CoAP");
    return 0;
}

int coap_server_stop_cmd(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    server_running = false;
    ipv6_addr_t addr = IPV6_ADDR_LOOPBACK;
    conn_udp_sendto(cbuf, COAPPING_SIZE, NULL, 0, &addr, sizeof(ipv6_addr_t), AF_INET6,
            srcport, srcport);
    puts("Stop CoAP server");
    return 0;
}

int coap_cmd(int argc, char **argv)
{
    ipv6_addr_t addr;
    uint16_t sport = DEFAULT_SOURCE_PORT, dport = COAPPING_UDP_PORT;
    char addr_str[IPV6_ADDR_MAX_STR_LEN];
    coap_packet_t cpkt;
    memset(&cpkt, 0, sizeof(coap_packet_t));

    if (argc < 2) {
        printf("usage: %s <IPv6> [port] [src port]\n", argv[0]);
    }

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, argv[1]) == NULL) {
        puts("Error: unable to parse destination address");
        return 1;
    }
    if (argc > 2) {
        dport = atoi(argv[2]);
    }
    if (argc > 3) {
        sport = atoi(argv[3]);
    }

    cpkt.hdr.ver = 0x01;
    cpkt.hdr.t = COAP_TYPE_CON;
    cpkt.hdr.tkl = 0;
    cpkt.hdr.code = 0;
    cpkt.numopts = 1;
    memcpy(&(cpkt.hdr.id), byteorder_htonl(msg_seq_cnt).u16, sizeof(uint16_t));
    size_t buflen = COAPPING_SIZE;

    coap_build(cbuf, &buflen, &cpkt);

    printf("Sending CoAP ping to %s:%" PRIu16 "\n", ipv6_addr_to_str(addr_str, &addr, IPV6_ADDR_MAX_STR_LEN), dport);
    conn_udp_sendto(cbuf, COAPPING_SIZE, NULL, 0, &addr, sizeof(ipv6_addr_t),
                    AF_INET6, sport, dport);
    msg_seq_cnt++;

    return 0;
}
