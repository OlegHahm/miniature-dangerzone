#include "xtimer.h"

#include "net/netopt.h"
#include "net/gnrc/netapi.h"

#include "ccnlriot.h"

xtimer_t _cluster_timer;

void cluster_sleep(void)
{
    netopt_state_t state = NETOPT_STATE_SLEEP;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        puts("cluster: error going to sleep");
    }
}

void cluster_takeover(void)
{
}

void cluster_handover(void)
{
}

void cluster_wakeup(void)
{
    netopt_state_t state = NETOPT_STATE_IDLE;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        puts("cluster: error waking up");
    }
}
