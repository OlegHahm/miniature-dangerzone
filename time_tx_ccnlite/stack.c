/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#include <assert.h>

#include "msg.h"
#include "net/gnrc/netdev2/ieee802154.h"
#include "ccn-lite-riot.h"
#include "net/gnrc/netif.h"
#include "thread.h"

#include "netdev.h"

#include "stack.h"


#define _MAC_STACKSIZE      (THREAD_STACKSIZE_DEFAULT)
#define _MAC_PRIO           (THREAD_PRIORITY_MAIN - 4)

static char _mac_stacks[_MAC_STACKSIZE][NETDEV_NUMOF];
static gnrc_netdev2_t _gnrc_adapters[NETDEV_NUMOF];
static kernel_pid_t _pids[NETDEV_NUMOF];

void stack_init(void)
{
    /* netdev needs to be set-up */
    assert(netdevs[0].netdev.netdev.driver);
    gnrc_pktbuf_init();

    for (uint8_t i = 0; i < NETDEV_NUMOF; i++) {
        if (gnrc_netdev2_ieee802154_init(&_gnrc_adapters[i],
                                         (netdev2_ieee802154_t *)&netdevs[i])) {
            DEBUG("Could not initialize netdev2<->IEEE802154 glue code\n");
            return;
        }
        _pids[i] = gnrc_netdev2_init(_mac_stacks[i], _MAC_STACKSIZE, _MAC_PRIO,
                                     "netdev2", &_gnrc_adapters[i]);
    }

    ccnl_core_init();

    ccnl_start();

    /* get the default interface */
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    size_t ifnum = gnrc_netif_get(ifs);

    /* set the relay's PID, configure the interface to use CCN nettype */
    if ((ifnum <= 0) || (ccnl_open_netif(ifs[0], GNRC_NETTYPE_CCN) < 0)) {
        puts("Error registering at network interface!");
        return;
    }
}

/** @} */
