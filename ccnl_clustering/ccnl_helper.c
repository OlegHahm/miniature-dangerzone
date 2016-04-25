#include "sched.h"
#include "msg.h"
#include "xtimer.h"
#include "net/gnrc/netreg.h"
#include "ccnl-pkt-ndntlv.h"

#include "cluster.h"
#include "ccnlriot.h"
#include "log.h"

/* buffers for interests and content */
static unsigned char _int_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _cont_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _out[CCNL_MAX_PACKET_SIZE];

void free_packet(struct ccnl_pkt_s *pkt);

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

static void _send_ack(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_prefix_s *pfx)
{
            struct ccnl_content_s *c =
                ccnl_helper_create_cont(pfx, (unsigned char*)
                                        CCNLRIOT_CONT_ACK,
                                        strlen(CCNLRIOT_CONT_ACK), false);
            ccnl_face_enqueue(relay, from, ccnl_buf_new(c->pkt->buf->data,
                                                        c->pkt->buf->datalen));

            free_packet(c->pkt);
            ccnl_free(c);
}

int ccnlriot_consumer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    (void) from;
    char *pfx_str = ccnl_prefix_to_path_detailed(pkt->pfx, 1, 0, 0);
    LOG_DEBUG("%u cluster: local consumer for prefix: %s\n", xtimer_now(),
             pfx_str);
    ccnl_free(pfx_str);

    struct ccnl_prefix_s *prefix;

    char store_pfx[] = CCNLRIOT_STORE_PREFIX;
    char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    char done_pfx[] = CCNLRIOT_DONE_PREFIX;
    char *ignore_prefixes[3];
    ignore_prefixes[0] = store_pfx;
    ignore_prefixes[1] = all_pfx;
    ignore_prefixes[2] = done_pfx;

    for (unsigned i = 0; i < 3; i++) {
        prefix = ccnl_URItoPrefix(ignore_prefixes[i], CCNL_SUITE_NDNTLV, NULL, 0);

        if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH))
        {
            LOG_DEBUG("cluster: ignoring this content\n");
            struct ccnl_pkt_s *tmp = pkt;
            struct ccnl_content_s *c = ccnl_content_new(relay, &pkt);
            ccnl_content_serve_pending(relay, c);
            free_packet(tmp);
            ccnl_free(c);
            free_prefix(prefix);
            return 1;
        }
        free_prefix(prefix);
    }
    return 0;
}
int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    int res = 0;

    char *pfx_str = ccnl_prefix_to_path_detailed(pkt->pfx, 1, 0, 0);
    LOG_DEBUG("%u cluster: local producer for prefix: %s\n", xtimer_now(), pfx_str);

    char store_pfx[] = CCNLRIOT_STORE_PREFIX;
    char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    char done_pfx[] = CCNLRIOT_DONE_PREFIX;
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(store_pfx, CCNL_SUITE_NDNTLV,
                                                    NULL, 0);

    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH))
    {
        /* only deputy sends ACK */
        if ((cluster_state == CLUSTER_STATE_DEPUTY) || (cluster_state == CLUSTER_STATE_TAKEOVER)) {
            LOG_INFO("cluster: acknowledging store request for %s\n", pfx_str);
            _send_ack(relay, from, pkt->pfx);
        }
        /* strip store prefix and cache it */
        struct ccnl_prefix_s *new_p = _get_pfx_from_store(pkt->pfx);
        ccnl_helper_create_cont(new_p, pkt->pfx->comp[pkt->pfx->compcnt-1],
                                pkt->pfx->complen[pkt->pfx->compcnt-1], true);
        /* do not do anything else for a store interest */
        free_prefix(new_p);
        free_packet(pkt);
        res = 1;
        goto out;
    }
    free_prefix(prefix);

    /* check for someone requesting all the data */
    prefix = ccnl_URItoPrefix(all_pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH)) {
        if (cluster_state == CLUSTER_STATE_DEPUTY) {
            LOG_INFO("cluster: acknowledging interest for handover\n");
            _send_ack(relay, from, pkt->pfx);
            /* schedule transmitting content store */
            msg_t m;
            m.type = CLUSTER_MSG_ALLDATA;
            msg_send(&m, cluster_pid);
        }
        free_packet(pkt);
        res = 1;
        goto out;
    }
    free_prefix(prefix);

    /* check if handover is done */
    prefix = ccnl_URItoPrefix(done_pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH)) {
        if (cluster_state == CLUSTER_STATE_TAKEOVER) {
            _send_ack(relay, from, pkt->pfx);
            LOG_INFO("cluster: I'm the new deputy\n");
            cluster_state = CLUSTER_STATE_DEPUTY;
        }
        else {
            LOG_WARNING("!!! cluster: received unexpected \"done\"\n");
        }
        free_packet(pkt);
        res = 1;
        goto out;
    }

out:
    /* freeing memory */
    ccnl_free(pfx_str);
    free_prefix(prefix);
    return res;
}

void ccnl_helper_send_all_data(void)
{
    struct ccnl_content_s *cit;
    for (cit = ccnl_relay.contents; cit; cit = cit->next) {
        /* send store interest for each object in cache */
        char *p_str = ccnl_prefix_to_path_detailed(cit->pkt->pfx, 1, 0, 0);
        ccnl_helper_int((unsigned char*) p_str, cit->pkt->content, cit->pkt->contlen);
        ccnl_free(p_str);
    }
    /* send done message */
    char done_pfx[] = CCNLRIOT_DONE_PREFIX;
    if (ccnl_helper_int((unsigned char*) done_pfx, NULL, 0)) {
        cluster_sleep(CLUSTER_SIZE-1);
    }
}

int ccnl_helper_int(unsigned char *prefix, unsigned char *value, size_t len)
{
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[CCNLRIOT_ADDRLEN];
    memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = 0;

    char *tmp_pfx;

    for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
        size_t prefix_len = len;
        if (prefix == NULL) {
            prefix_len += sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 8 + len;
        }
        else {
            /* add 1 for the \0 terminator */
            prefix_len += strlen((char*) prefix) + 1;
        }

        if (len > 0) {
            prefix_len += sizeof(CCNLRIOT_STORE_PREFIX);
        }
        unsigned char pfx[prefix_len];
        if (len > 0) {
            if (prefix == NULL) {
                snprintf((char*) pfx, prefix_len, "%s%s%s/%08X/%s",
                         CCNLRIOT_STORE_PREFIX, CCNLRIOT_SITE_PREFIX,
                         CCNLRIOT_TYPE_PREFIX, cluster_my_id, (char*) value);
            }
            else if (len > 0) {
                snprintf((char*) pfx, prefix_len, "%s%s/%s", CCNLRIOT_STORE_PREFIX, prefix, (char*) value);
            }
            prefix = pfx;
        }

        tmp_pfx = strndup((char*) prefix, strlen((char*) prefix));
        LOG_INFO("cluster: sending interest for %s\n", prefix);
        gnrc_netreg_entry_t _ne;
        /* register for content chunks */
        _ne.demux_ctx =  GNRC_NETREG_DEMUX_CTX_ALL;
        _ne.pid = sched_active_pid;
        gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

        ccnl_send_interest(CCNL_SUITE_NDNTLV, (char*) prefix, 0, _int_buf, CCNLRIOT_BUF_SIZE);
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
        LOG_WARNING("\n !!! TIMEOUT while waiting for %s !!!\n", tmp_pfx);
    }
    free(tmp_pfx);

    return success;
}
