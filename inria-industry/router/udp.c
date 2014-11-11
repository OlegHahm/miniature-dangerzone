/*
 * Copyright (C) 2014 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "thread.h"

#include "socket_base/socket.h"

#include "net_help.h"

#include "../ccn_lite/ccnl-riot-compat.h"
#include "ccn_lite/ccnl-riot.h"

#include "demo.h"

#define UDP_BUFFER_SIZE     (128)
#define SERVER_PORT     (0xFF01)

char udp_server_stack_buffer[KERNEL_CONF_STACKSIZE_MAIN];
char addr_str[IPV6_MAX_ADDR_STR_LEN];
char buffer_main[UDP_BUFFER_SIZE];

extern int appserver_pid;

static void *init_udp_server(void *);

/* UDP server thread */
void udp_server(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    int udp_server_thread_pid = thread_create(
            udp_server_stack_buffer, sizeof(udp_server_stack_buffer),
            PRIORITY_MAIN, CREATE_STACKTEST,
            init_udp_server, NULL, "init_udp_server");
    printf("UDP SERVER ON PORT %d (THREAD PID: %d)\n", HTONS(SERVER_PORT), udp_server_thread_pid);
}

msg_t m;
riot_ccnl_msg_t rmsg;

static void *init_udp_server(void *arg)
{
    (void) arg;

    sockaddr6_t sa;
    int32_t recsize;
    uint32_t fromlen;
    int sock = socket_base_socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    memset(&sa, 0, sizeof(sa));

    sa.sin6_family = AF_INET;
    sa.sin6_port = HTONS(SERVER_PORT);

    fromlen = sizeof(sa);

    if (-1 == socket_base_bind(sock, &sa, sizeof(sa))) {
        printf("Error bind failed!\n");
        socket_base_close(sock);
    }

    while (1) {
        recsize = socket_base_recvfrom(sock, (void *)buffer_main, UDP_BUFFER_SIZE, 0,
                                          &sa, &fromlen);

        if (recsize < 0) {
            printf("ERROR: recsize < 0!\n");
        }

        printf("UDP packet of size %" PRIi32 " received, payload: %s\n", recsize, buffer_main);
        printf("relaying to appserver at PID %i\n", appserver_pid);
        m.type = UPPER_LAYER_4;
        rmsg.size = recsize;
        rmsg.payload = buffer_main;
        m.content.ptr = (char *) &rmsg;
        msg_send(&m, appserver_pid);
    }

    socket_base_close(sock);

    return NULL;
}

/* UDP send command */
void udp_send(int argc, char **argv)
{
    int sock;
    sockaddr6_t sa;
    ipv6_addr_t ipaddr;
    int bytes_sent;
    int address;
    char text[5];

    if (argc != 3) {
        printf("usage: send <addr> <text>\n");
        return;
    }

    address = atoi(argv[1]);

    strncpy(text, argv[2], sizeof(text));
    text[sizeof(text) - 1] = 0;

    sock = socket_base_socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    if (-1 == sock) {
        printf("Error Creating Socket!");
        return;
    }

    memset(&sa, 0, sizeof(sa));

    ipv6_addr_init(&ipaddr, 0xfe80, 0x0, 0x0, 0x0, 0x0, 0x00ff, 0xfe00, (uint16_t)address);

    sa.sin6_family = AF_INET;
    memcpy(&sa.sin6_addr, &ipaddr, 16);
    sa.sin6_port = HTONS(SERVER_PORT);

    bytes_sent = socket_base_sendto(sock, (char *)text,
                                       strlen(text) + 1, 0, &sa,
                                       sizeof(sa));

    if (bytes_sent < 0) {
        printf("Error sending packet!\n");
    }
    else {
        printf("Successful deliverd %i bytes over UDP to %s to 6LoWPAN\n",
               bytes_sent, ipv6_addr_to_str(addr_str, IPV6_MAX_ADDR_STR_LEN,
                                            &ipaddr));
    }

    socket_base_close(sock);
}
