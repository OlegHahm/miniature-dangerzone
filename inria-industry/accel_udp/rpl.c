/*
 * Copyright (C) 2013 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     inria_industry_demo
 * @{
 *
 * @file        rpl.c
 * @brief       INRIA-Industry 2014 demo application - sensor node
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include "vtimer.h"
#include "thread.h"
#include "net_if.h"
#include "sixlowpan.h"
#include "socket_base/socket.h"
#include "rpl.h"
#include "rpl/rpl_dodag.h"

#include "demo.h"

char monitor_stack_buffer[MONITOR_STACK_SIZE];
radio_address_t id = NODE_ADDRESS;
ipv6_addr_t std_addr;
char addr_str[IPV6_MAX_ADDR_STR_LEN];

uint8_t is_root = 0;

void rpl_udp_init(int argc, char **argv)
{
    transceiver_command_t tcmd;
    msg_t m;
    uint8_t chan = RADIO_CHANNEL;

    if (argc != 2) {
        printf("Usage: %s (r|n)\n", argv[0]);
        printf("\tr\tinitialize as root\n");
        printf("\tn\tinitialize as node router\n");
        return;
    }


    char command = argv[1][0];
    if ((command == 'n') || (command == 'r')) {
        uint8_t state;
        printf("INFO: Initialize as %s on address %d (0x%X)\n", ((command == 'n') ? "node" : "root"), id, id);

        if (!id || (id > 255)) {
            printf("ERROR: address not a valid 8 bit integer\n");
            return;
        }

        net_if_set_hardware_address(0, id);

        state = rpl_init(0);

        if (state != SIXLOWERROR_SUCCESS) {
            printf("Error initializing RPL\n");
        }
        else {
            puts("6LoWPAN and RPL initialized.");
        }

        if (command == 'r') {
            rpl_init_root();
            is_root = 1;
        }
        else {
            ipv6_iface_set_routing_provider(rpl_get_next_hop);
        }

        kernel_pid_t monitor_pid = thread_create(monitor_stack_buffer, MONITOR_STACK_SIZE, PRIORITY_MAIN - 2, CREATE_STACKTEST, rpl_udp_monitor, NULL, "monitor");
        transceiver_register(TRANSCEIVER_DEFAULT, monitor_pid);
        ipv6_register_packet_handler(monitor_pid);
        //sixlowpan_lowpan_register(monitor_pid);
    }
    else {
        printf("ERROR: Unknown command '%c'\n", command);
        return;
    }

    puts("initializing interface");
    puts("initializing interface");
    puts("initializing interface");
    puts("initializing interface");
    puts("initializing interface");
    sixlowpan_lowpan_init_interface(IF_ID);
    /* TODO: check if this works as intended */
    puts("initializing as router");
    ipv6_init_as_router();

    puts("Setting channel");
    /* set channel to 10 */
    tcmd.transceivers = TRANSCEIVER_DEFAULT;
    tcmd.data = &chan;
    m.type = SET_CHANNEL;
    m.content.ptr = (void *) &tcmd;

    msg_send_receive(&m, &m, transceiver_pid);
    printf("Channel set to %u\n", RADIO_CHANNEL);

    /* start transceiver watchdog */
}

void rpl_udp_dodag(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    printf("---------------------------\n");
    rpl_dodag_t *mydodag = rpl_get_my_dodag();

    if (mydodag == NULL) {
        printf("Not part of a dodag\n");
        printf("---------------------------\n");
        return;
    }

    printf("Part of Dodag:\n");
    printf("%s\n", ipv6_addr_to_str(addr_str, IPV6_MAX_ADDR_STR_LEN,
                                    (&mydodag->dodag_id)));
    printf("my rank: %d\n", mydodag->my_rank);

    if (!is_root) {
        printf("my preferred parent:\n");
        printf("%s\n", ipv6_addr_to_str(addr_str, IPV6_MAX_ADDR_STR_LEN,
                                        (&mydodag->my_preferred_parent->addr)));
    }

    printf("---------------------------\n");
}
