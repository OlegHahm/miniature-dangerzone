#include <stdio.h>
#include <stdint.h>
#include "xtimer.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/netif/hdr.h"

#define SILENT_INTERVAL     (50)
#define NOISE_CHAN          (26)
#define NOISE_PAN           (44)
#define PAYLOAD     "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

static void _send(void)
{
    gnrc_pktsnip_t *pkt;
    gnrc_netif_hdr_t *nethdr;
    uint8_t flags = 0x00;

    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    size_t numof = gnrc_netif_get(ifs);

    unsigned res = NOISE_CHAN;
    if (gnrc_netapi_set(ifs[0], NETOPT_CHANNEL, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        puts("main: error setting channel");
    }
    res = NOISE_PAN;
    if (gnrc_netapi_set(ifs[0], NETOPT_NID, 0, (uint16_t *)&res, sizeof(uint16_t)) < 0) {
        puts("main: error setting pan");
    }
    if (gnrc_netapi_set(ifs[0], NETOPT_CSMA, 0, NETOPT_DISABLE, sizeof(netopt_enable_t)) < 0) {
        puts("failed to disable CSMA");
    }
    for (size_t i = 0; i < numof && i < GNRC_NETIF_NUMOF; i++) {

        flags |= GNRC_NETIF_HDR_FLAGS_BROADCAST;

        /* put packet together */
        pkt = gnrc_pktbuf_add(NULL, PAYLOAD, strlen(PAYLOAD), GNRC_NETTYPE_UNDEF);
        pkt = gnrc_pktbuf_add(pkt, NULL, sizeof(gnrc_netif_hdr_t), GNRC_NETTYPE_NETIF);
        nethdr = (gnrc_netif_hdr_t *)pkt->data;
        nethdr->flags = flags;
        /* and send it */
        if (gnrc_netapi_send(ifs[i], pkt) < 1) {
            puts("error: unable to send\n");
            gnrc_pktbuf_release(pkt);
        }
    }
}

void *noise_start(void *unused)
{
    (void) unused;
    puts("Let's make some NOOOOOOOOOIIIIIIISE!!!!");

    while (1) {
        xtimer_usleep(SILENT_INTERVAL);
        _send();
    }
    return 0;
}
