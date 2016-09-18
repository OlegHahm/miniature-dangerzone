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

#include <errno.h>
#include <string.h>

#include "net/netopt.h"
#include "net/netdev2/ieee802154.h"

#include "netdev.h"

netdev2_test_t netdevs[NETDEV_NUMOF];

static int _get_src_len(netdev2_t *dev, void *value, size_t max_len)
{
    uint16_t *v = value;

    (void)dev;
    if (max_len != sizeof(uint16_t)) {
        return -EOVERFLOW;
    }

    *v = sizeof(uint64_t);

    return sizeof(uint16_t);
}

static int _get_max_pkt_size(netdev2_t *dev, void *value, size_t max_len)
{
    uint16_t *v = value;

    (void)dev;
    if (max_len != sizeof(uint16_t)) {
        return -EOVERFLOW;
    }

    *v = 104;   /* 127 - maximum MAC overhead of IEEE 802.15.4 (with PAN compression) */

    return sizeof(uint16_t);
}

static int _get_addr(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_get((netdev2_ieee802154_t *)dev, NETOPT_ADDRESS, value, max_len);
}

static int _get_addr_long(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_get((netdev2_ieee802154_t *)dev, NETOPT_ADDRESS_LONG, value, max_len);
}

static int _get_addr_len(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_get((netdev2_ieee802154_t *)dev, NETOPT_ADDR_LEN, value, max_len);
}

static int _set_addr_len(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_set((netdev2_ieee802154_t *)dev, NETOPT_ADDR_LEN, value, max_len);
}

static int _get_nid(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_get((netdev2_ieee802154_t *)dev, NETOPT_NID, value, max_len);
}

static int _get_mtu(netdev2_t *dev, void *value, size_t max_len)
{
    (void) dev;
    (void) max_len;
    *((uint16_t *)value) = 104;
    return sizeof(uint16_t);
}

static int _get_proto(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_get((netdev2_ieee802154_t *)dev, NETOPT_PROTO, value, max_len);
}

static int _get_device_type(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_get((netdev2_ieee802154_t *)dev, NETOPT_DEVICE_TYPE, value, max_len);
}

static int _get_ipv6_iid(netdev2_t *dev, void *value, size_t max_len)
{
    return netdev2_ieee802154_get((netdev2_ieee802154_t *)dev, NETOPT_IPV6_IID, value, max_len);
}

void netdev_init(void)
{
    uint8_t addr[] = NETDEV_ADDR_PREFIX;
    for (uint8_t i = 0; i < NETDEV_NUMOF; i++) {
        addr[7] = i;
        netdev2_test_setup(&netdevs[i], NULL);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_SRC_LEN, _get_src_len);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_MAX_PACKET_SIZE,
                                _get_max_pkt_size);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_ADDRESS, _get_addr);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_ADDRESS_LONG,
                                _get_addr_long);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_ADDR_LEN,
                                _get_addr_len);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_SRC_LEN,
                                _get_addr_len);
        netdev2_test_set_set_cb(&netdevs[i], NETOPT_SRC_LEN,
                                _set_addr_len);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_MAX_PACKET_SIZE,
                                _get_mtu);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_NID,
                                _get_nid);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_PROTO,
                                _get_proto);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_DEVICE_TYPE,
                                _get_device_type);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_IPV6_IID,
                                _get_ipv6_iid);
        netdevs[i].netdev.proto = GNRC_NETTYPE_CCN;
        netdevs[i].netdev.pan = NETDEV_PAN_ID;
        memcpy(netdevs[i].netdev.short_addr, &addr[6], sizeof(uint16_t));
        memcpy(netdevs[i].netdev.long_addr, &addr[0], sizeof(uint64_t));
        netdevs[i].netdev.seq = 0;
        netdevs[i].netdev.chan = 26;
        netdevs[i].netdev.flags = (NETDEV2_IEEE802154_ACK_REQ |
                                   NETDEV2_IEEE802154_SRC_MODE_LONG |
                                   NETDEV2_IEEE802154_PAN_COMP);
    }
}

/** @} */
