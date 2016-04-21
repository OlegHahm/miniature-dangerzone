#include "sched.h"
#include "net/gnrc/netreg.h"
#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"

#include "ccnlriot.h"

/* buffers for interests and content */
static unsigned char _int_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _cont_buf[CCNLRIOT_BUF_SIZE];

void ccnl_helper_int(void)
{
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[CCNLRIOT_ADDRLEN];
    memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = 0;

    for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
        char pfx[] = CCNLRIOT_PREFIX1;

        gnrc_netreg_entry_t _ne;
        /* register for content chunks */
        _ne.demux_ctx =  GNRC_NETREG_DEMUX_CTX_ALL;
        _ne.pid = sched_active_pid;
        gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

        ccnl_send_interest(CCNL_SUITE_NDNTLV, pfx, 0, _int_buf, CCNLRIOT_BUF_SIZE);
        if (ccnl_wait_for_chunk(_cont_buf, CCNLRIOT_BUF_SIZE, 0) > 0) {
            gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
            LOG_DEBUG("Content received: %s\n", _cont_buf);
            success++;
            break;
        }
        gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
    }

    if (success == 1) {
        LOG_WARNING("\n +++ SUCCESS +++\n");
    }
    else {
        LOG_WARNING("\n !!! TIMEOUT !!!\n");
        LOG_DEBUG("Timeout! No content received in response to the Interest for %s.\n", ccnlriot_prefix1);
    }

}


