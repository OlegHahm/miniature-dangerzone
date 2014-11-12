/*
 * Copyright (C) 2013 INRIA
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup examples
 * @{
 *
 * @file        main.c
 * @brief       CeBIT 2014 demo application - router node
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

#include "ps.h"

void _stat(int argc, char **argv)
{
    thread_print_all();

    extern void native_print_stats(void);
    native_print_stats();
}

const shell_command_t shell_commands[] = {
    {"init", "Initialize network", rpl_udp_init},
    {"set", "Set ID", rpl_udp_set_id},
    {"table", "Shows the routing table", rpl_udp_table},
    {"dodag", "Shows the dodag", rpl_udp_dodag},
    {"loop", "", rpl_udp_loop},
    {"server", "Starts a UDP server", udpif_shell_server},
    {"send", "Send a UDP datagram", udpif_shell_send},
    {"ign", "ignore node", rpl_udp_ignore_cmd},
    {"stat", "print statistics", _stat},
    {NULL, NULL, NULL}
};


int main(void)
{
    puts("CeBIT demo -router node v"APP_VERSION);

    // fill neighbor cache
    //fill_nc();

    /* init RPL */
//    char *init[] = {"init", NODE_MODE};
//    rpl_udp_init(2, init);

    extern void default_data_handler(uint16_t src, char *data, int length);
    udpif_start_server(APPLICATION_PORT, default_data_handler);

    /* start shell */
    posix_open(uart0_handler_pid, 0);
    shell_t shell;
    shell_init(&shell, shell_commands, UART0_BUFSIZE, uart0_readc, uart0_putc);
    shell_run(&shell);
    return 0;
}
