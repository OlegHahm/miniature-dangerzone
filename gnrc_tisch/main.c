/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "shell.h"
#include "msg.h"

#include "opendefs.h"
#include "schedule.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

void icn_addToFixedSchedule(open_addr_t *addr, int16_t rx_cell, int16_t tx_cell, int16_t shared) {
    if (tx_cell >= 0) {
        schedule_addActiveSlot(tx_cell,
                CELLTYPE_TX,
                FALSE,
                SCHEDULE_MINIMAL_6TISCH_CHANNELOFFSET,
                addr
                );
    }
    if (rx_cell >= 0) {
        schedule_addActiveSlot(rx_cell,
                CELLTYPE_RX,
                FALSE,
                SCHEDULE_MINIMAL_6TISCH_CHANNELOFFSET,
                addr
                );
    }
    if (shared >= 0) {
        open_addr_t     temp_neighbor;
        memset(&temp_neighbor,0,sizeof(temp_neighbor));
        temp_neighbor.type             = ADDR_ANYCAST;
        schedule_addActiveSlot(shared,
                CELLTYPE_TXRX,
                TRUE,
                SCHEDULE_MINIMAL_6TISCH_CHANNELOFFSET,
                &temp_neighbor
                );
    }
}

int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");

    icn_addToFixedSchedule(NULL, 4, -1, -1);

    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should be never reached */
    return 0;
}
