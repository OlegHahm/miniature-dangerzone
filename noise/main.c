#include <stdio.h>
#include <stdint.h>
#include "xtimer.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/netif/hdr.h"

#define SILENT_INTERVAL     (500000)

#define PAYLOAD     "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

static void _send(void)
{
    gnrc_pktsnip_t *pkt;
    gnrc_netif_hdr_t *nethdr;
    uint8_t flags = 0x00;

    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    size_t numof = gnrc_netif_get(ifs);

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

int main(void)
{
    puts("Let's make some NOOOOOOOOOIIIIIIISE!!!!");

    while (1) {
        xtimer_usleep(SILENT_INTERVAL);
        _send();
        puts(".");
    }
    return 0;
}
