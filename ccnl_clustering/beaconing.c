#include "net/gnrc.h"
#include "xtimer.h"
#include "bloom.h"

#include "ccnlriot.h"
#include "cluster.h"

/* data format of beacons */
#define BEACONING_MK    (0x1701)

typedef struct __attribute__((packed))  {
    uint16_t magic_key;
    uint32_t id;
} beacon_t;

/* initialize some global variables */
uint16_t cluster_position = 0;

uint16_t cluster_size = 1;

/* handle a received beacon */
static void _handle_beacon(gnrc_pktsnip_t *pkt)
{
    if (pkt->size != sizeof(beacon_t)) {
        LOG_WARNING("beaconing: received packet doesn't seem to be a beacon - wrong size\n");
        gnrc_pktbuf_release(pkt);
        return;
    }
    beacon_t b = *((beacon_t*) pkt->data);
    if (b.magic_key != BEACONING_MK) {
        LOG_WARNING("beaconing: received packet doesn't seem to be a beacon - wrong magic key\n");
        gnrc_pktbuf_release(pkt);
        return;
    }
    LOG_DEBUG("beaconing: received a beacon, id is %" PRIu32 "\n", b.id);
    if (bloom_check(&cluster_neighbors, (uint8_t*) &(b.id), sizeof(b.id))) {
        LOG_DEBUG("beaconing: already know this neighbor\n");
    }
    /* if we don't know the neighbor we analyze its ID */
    else {
        bloom_add(&cluster_neighbors, (uint8_t*) &(b.id), sizeof(b.id));
        cluster_size++;
        if (b.id < cluster_my_id) {
            cluster_position++;
        }
    }
    gnrc_pktbuf_release(pkt);
}

/* start sending beacons */
void beaconing_start(void)
{
    uint8_t remaining_beacons = CLUSTER_BEACONING_COUNT;
    bool end_beaconing = false;

    /* register for RX */
    gnrc_netreg_entry_t _ne;
    _ne.pid = sched_active_pid;
    _ne.demux_ctx = GNRC_NETREG_DEMUX_CTX_ALL;
    gnrc_netreg_register(GNRC_NETTYPE_UNDEF, &_ne);
    /* schedule beacon */
    msg_t beacon_msg;
    beacon_msg.type = CLUSTER_MSG_BEACON;
    xtimer_t beacon_timer;
    beacon_timer.target = beacon_timer.long_target = 0;
    /* let's delay the first beacon by CLUSTER_BEACONING_WAIT */
    uint32_t tmp = CLUSTER_BEACONING_PERIOD + CLUSTER_BEACONING_WAIT;
    xtimer_set_msg(&beacon_timer, tmp, &beacon_msg, sched_active_pid);
    tmp -= CLUSTER_BEACONING_WAIT;
    uint32_t start = xtimer_now();
    while (1) {
        msg_t m;
        msg_receive(&m);
        switch (m.type) {
            case CLUSTER_MSG_BEACON:
                LOG_DEBUG("beaconing: ready to send next beacon\n");
                beaconing_send();
                tmp = CLUSTER_BEACONING_PERIOD;
                /* check if we need to do further beaconing */
                if (--remaining_beacons > 0) {
                    xtimer_set_msg(&beacon_timer, tmp, &beacon_msg, sched_active_pid);
                }
                else {
                    beacon_msg.type = CLUSTER_MSG_BEACON_END;
                    tmp = 2 * CLUSTER_BEACONING_WAIT - (xtimer_now() - start);
                    LOG_INFO("beaconing: end beaconing period in %" PRIu32 \
                              " seconds\n", (tmp / SEC_IN_USEC));

                    xtimer_set_msg(&beacon_timer, tmp, &beacon_msg, sched_active_pid);
                }
                break;
            case CLUSTER_MSG_BEACON_END:
                LOG_INFO("beaconing: end beaconing\n");
                end_beaconing = true;
                break;
            case GNRC_NETAPI_MSG_TYPE_RCV:
                LOG_DEBUG("beaconing: received packet, assume that it is a beacon\n");
                _handle_beacon((gnrc_pktsnip_t *)m.content.ptr);
                break;
            default:
                LOG_WARNING("beaconing: didn't expect message type %X\n", m.type);
                break;
        }
        if (end_beaconing) {
            break;
        }
    }
    gnrc_netreg_unregister(GNRC_NETTYPE_UNDEF, &_ne);
}

/* send a beacon */
void beaconing_send(void)
{
    gnrc_pktsnip_t *pkt, *hdr;
    gnrc_netif_hdr_t *nethdr;

    /* put packet together */
    beacon_t b = { .magic_key = BEACONING_MK, .id = cluster_my_id };
    pkt = gnrc_pktbuf_add(NULL, &b, sizeof(b), GNRC_NETTYPE_UNDEF);
    if (pkt == NULL) {
        puts("error: packet buffer full");
        return;
    }
    hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);
    if (hdr == NULL) {
        puts("error: packet buffer full");
        gnrc_pktbuf_release(pkt);
        return;
    }
    LL_PREPEND(pkt, hdr);
    nethdr = (gnrc_netif_hdr_t *)hdr->data;
    nethdr->flags |= GNRC_NETIF_HDR_FLAGS_BROADCAST;
    /* and send it */
    LOG_DEBUG("beaconing: send beacon\n");
    if (gnrc_netapi_send(CCNLRIOT_NETIF, pkt) < 1) {
        puts("error: unable to send");
        gnrc_pktbuf_release(pkt);
        return;
    }
}
