#include "sched.h"
#include "net/gnrc/netreg.h"
#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"
#include "log.h"

#include "cluster.h"
#include "ccnlriot.h"

/* buffers for interests and content */
static unsigned char _int_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _cont_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _out[CCNL_MAX_PACKET_SIZE];

struct ccnl_content_s *ccnl_helper_create_cont(struct ccnl_prefix_s *prefix,
                                               unsigned char *value, ssize_t
                                               len, bool cache)
{
    int offs = CCNL_MAX_PACKET_SIZE;

    ssize_t arg_len = ccnl_ndntlv_prependContent(prefix, value, len, NULL, NULL, &offs, _out);

    unsigned char *olddata;
    unsigned char *data = olddata = _out + offs;

    unsigned typ;

    if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
        typ != NDN_TLV_Data) {
        return NULL;
    }

    struct ccnl_content_s *c = 0;
    struct ccnl_pkt_s *pk = ccnl_ndntlv_bytes2pkt(typ, olddata, &data, &arg_len);
    c = ccnl_content_new(&ccnl_relay, &pk);
    if (cache) {
        ccnl_content_add2cache(&ccnl_relay, c);
    }
    c->flags |= CCNL_CONTENT_FLAGS_STATIC;

    return c;
}

struct ccnl_prefix_s* ccnl_prefix_new(int suite, int cnt);

static struct ccnl_prefix_s *_get_pfx_from_store(struct ccnl_prefix_s *pfx)
{
    int i = 0, len = 0;
    struct ccnl_prefix_s *p = ccnl_prefix_new(pfx->suite, pfx->compcnt);
    if (!p) {
        return NULL;
    }

    p->compcnt = pfx->compcnt - 2;
    p->chunknum = pfx->chunknum;

    for (i = 1; i < pfx->compcnt - 1; i++) {
        len += pfx->complen[i];
    }

    p->bytes = (unsigned char*) ccnl_malloc(len);
    if (!p->bytes) {
        free_prefix(p);
        return NULL;
    }

    len = 0;
    for (i = 1; i < pfx->compcnt - 1; i++) {
        p->complen[i-1] = pfx->complen[i];
        p->comp[i-1] = p->bytes + len;
        memcpy(p->bytes + len, pfx->comp[i], p->complen[i-1]);
        len += p->complen[i-1];
    }

    if (pfx->chunknum) {
        p->chunknum = (int*) ccnl_malloc(sizeof(int));
        *p->chunknum = *pfx->chunknum;
    }

    return p;
}

int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    (void) relay;
    (void) from;
    (void) pkt;
    puts("cluster: local producer");

    char store_pfx[] = CCNLRIOT_STORE_PREFIX;
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(store_pfx, CCNL_SUITE_NDNTLV,
                                                    NULL, 0);

    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH))
    {
        /* only deputy sends ACK */
        if (cluster_state == CLUSTER_STATE_DEPUTY) {
            struct ccnl_content_s *c =
            ccnl_helper_create_cont(pkt->pfx, (unsigned char*) CCNLRIOT_CONT_ACK,
                                    strlen(CCNLRIOT_CONT_ACK), false);

            ccnl_face_enqueue(relay, from, c->pkt->buf);
        }
        struct ccnl_prefix_s *new_p = _get_pfx_from_store(pkt->pfx);
        ccnl_helper_create_cont(new_p, pkt->pfx->comp[pkt->pfx->compcnt-1],
                                pkt->pfx->complen[pkt->pfx->compcnt-1], true);
        /* do not do anything else for a store interest */
        return 1;
    }

    return 0;
}

void ccnl_helper_int(unsigned value, size_t len)
{
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[CCNLRIOT_ADDRLEN];
    memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = 0;

    for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
        size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 8 + len;
        if (len > 0) {
            prefix_len += sizeof(CCNLRIOT_STORE_PREFIX);
        }
        char pfx[prefix_len];
        if (len > 0) {
            snprintf(pfx, prefix_len, "%s%s%s%08X/%08X", CCNLRIOT_STORE_PREFIX,
                     CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, cluster_my_id,
                     value);
        }

        printf("cluster: sending interested for %s\n", pfx);
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
    }

}


