/*
 * Copyright (C) 2013 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     cebit_demo
 * @{
 *
 * @file        main.c
 * @brief       CeBIT 2014 demo application - light node
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "posix_io.h"
#include "shell.h"
#include "shell_commands.h"
#include "board_uart0.h"
#include "tl_socket.h"
#include "kernel.h"

#include "demo.h"
#include "udpif.h"
#include "light.h"

const shell_command_t shell_commands[] = {
    {"init", "Initialize network", rpl_udp_init},
    {"set", "Set ID", rpl_udp_set_id},
    {"table", "Shows the routing table", rpl_udp_table},
    {"dodag", "Shows the dodag", rpl_udp_dodag},
    {"loop", "", rpl_udp_loop},
    {"server", "Starts a UDP server", udpif_shell_server},
    {"send", "Send a UDP datagram", udpif_shell_send},
    {"ign", "ignore node", rpl_udp_ignore_cmd},
    {"color", "Set a color [r] [g] [b]", light_set_shell},
    {"light-off", "Turn off the light", light_off_shell},
    {"confirm", "Confirm event", light_ok_shell},
    {"warn", "Warn event", light_warn_shell},
    {"alarm", "Alarm event", light_alarm_shell},
    {NULL, NULL, NULL}
};


void fill_nc(void)
{
// #ifdef ALL_NEIGHBORS
//     int numne = 5;
//     int numig = 0;
//     uint16_t neighbors[] = {23, 31, 32, 33, 41, 51};
//     uint16_t ignore[] = {};
// #elif defined LEVEL1
//     int numne = 4;
//     int numig = 2;
//     uint16_t neighbors[] = {23, 31, 32, 33};
//     uint16_t ignore[] = {41, 51};
// #elif defined LEVEL2
//     int numne = 5;
//     int numig = 1;
//     uint16_t neighbors[] = {31, 32, 33, 41, 51};
//     uint16_t ignore[] = {23};
// #elif defined LEVEL3
//     int numne = 3;
//     int numig = 3;
//     uint16_t neighbors[] = {32, 33, 51};
//     uint16_t ignore[] = {23, 31, 41};
// #endif

    int numne = 2;
    int numig = 4;
    uint16_t neighbors[] = {33, 51};
    uint16_t ignore[] = {23, 31, 32, 41};

    ipv6_addr_t r_addr;
    uint16_t l_addr;

    for (int i = 0; i < numne; i++) {
        printf("Adding %u as neighbor\n", neighbors[i]);
        udpif_get_ipv6_address(&r_addr, neighbors[i]);
        l_addr = HTONS(neighbors[i]);
        ndp_neighbor_cache_add(0, &r_addr, &l_addr, 2, 0,
                               NDP_NCE_STATUS_REACHABLE, 
                               NDP_NCE_TYPE_TENTATIVE, 
                               0xffff);
    }
    for (int i = 0; i < numig; i++) {
        printf("Ignoring %u\n", ignore[i]);
        rpl_udp_ignore(ignore[i]);
    }
}

int main(void)
{
    puts("CeBIT demo - light node v" APP_VERSION);

    // fill neighbor cache
    fill_nc();

    /* start the node as normal RPL node */
    char *init[] = {"init", NODE_MODE};
    rpl_udp_init(2, init);

    /* start the UDP server */
    udpif_start_server(APPLICATION_PORT, light_on_data);

    /* initialize the light */
    light_init();

    /* start shell */
    posix_open(uart0_handler_pid, 0);
    shell_t shell;
    shell_init(&shell, shell_commands, UART0_BUFSIZE, uart0_readc, uart0_putc);
    shell_run(&shell);
    return 0;
}
