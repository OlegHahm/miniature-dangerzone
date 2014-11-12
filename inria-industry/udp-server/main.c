/*
 * Copyright (C) 2014 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "msg.h"
#include "thread.h"
#include "posix_io.h"
#include "shell.h"
#include "board_uart0.h"
#include "transceiver.h"
#include "vtimer.h"
#include "ps.h"
#include "ltc4150.h"
#include "socket_base.h"
#include "net_if.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

#include "demo.h"

#define SHELL_MSG_BUFFER_SIZE (64)
msg_t msg_buffer_shell[SHELL_MSG_BUFFER_SIZE];

shell_t shell;

static const shell_command_t sc[] = {
    { "init", "Initialize network", rpl_udp_init},
    { "set", "Set ID", rpl_udp_set_id},
    { "server", "Starts a UDP server", udp_server},
    { "send", "Send a UDP datagram", udp_send},
    { "ign", "ignore node", rpl_udp_ignore},
    { "dodag", "Shows the dodag", rpl_udp_dodag},
    { NULL, NULL, NULL }
};

void fill_nc(void)
{
    ipv6_addr_t r_addr;
    uint16_t l_addr;

    printf("Adding %u as neighbor\n", id - 1);
    ipv6_addr_init(&r_addr, 0xfe80, 0x0, 0x0, 0x0, 0x0, 0x00ff, 0xfe00, id - 1);
    l_addr = HTONS(id - 1);
    ndp_neighbor_cache_add(0, &r_addr, &l_addr, 2, 0, NDP_NCE_STATUS_REACHABLE,
                           NDP_NCE_TYPE_TENTATIVE, 0xffff);

    printf("Adding %u as neighbor\n", id + 1);
    ipv6_addr_init(&r_addr, 0xfe80, 0x0, 0x0, 0x0, 0x0, 0x00ff, 0xfe00, id + 1);
    l_addr = HTONS(id + 1);
    ndp_neighbor_cache_add(0, &r_addr, &l_addr, 2, 0, NDP_NCE_STATUS_REACHABLE,
                           NDP_NCE_TYPE_TENTATIVE, 0xffff);

}

int main(void)
{
    puts("IETF90 - BnB - UDP server");

    if (msg_init_queue(msg_buffer_shell, SHELL_MSG_BUFFER_SIZE) != 0) {
        DEBUG("msg init queue failed...abording\n");
        return -1;
    }
    
    id = 1;

    /* fill neighbor cache */
    fill_nc();

    helper_ignore(3);
    rpl_ex_init('r');
    udp_server(1, NULL);
    posix_open(uart0_handler_pid, 0);
    net_if_set_src_address_mode(0, NET_IF_TRANS_ADDR_M_SHORT);
    shell_init(&shell, sc, UART0_BUFSIZE, uart0_readc, uart0_putc);
    shell_run(&shell);

    return 0;
}
