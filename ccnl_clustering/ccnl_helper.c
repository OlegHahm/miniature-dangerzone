#include "sched.h"
#include "msg.h"
#include "xtimer.h"
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

static void _send_ack(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_prefix_s *pfx)
{
            struct ccnl_content_s *c =
            ccnl_helper_create_cont(pfx, (unsigned char*) CCNLRIOT_CONT_ACK,
                                    strlen(CCNLRIOT_CONT_ACK), false);
            ccnl_face_enqueue(relay, from, c->pkt->buf);
}

int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    (void) relay;
    (void) from;
    (void) pkt;
    printf("%u cluster: local producer for prefix: %s\n", xtimer_now(), ccnl_prefix_to_path_detailed(pkt->pfx, 1, 0, 0));

    char store_pfx[] = CCNLRIOT_STORE_PREFIX;
    char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    char done_pfx[] = CCNLRIOT_DONE_PREFIX;
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(store_pfx, CCNL_SUITE_NDNTLV,
                                                    NULL, 0);

    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH))
    {
        /* only deputy sends ACK */
        if ((cluster_state == CLUSTER_STATE_DEPUTY) || (cluster_state == CLUSTER_STATE_TAKEOVER)) {
            _send_ack(relay, from, pkt->pfx);
        }
        /* strip store prefix and cache it */
        struct ccnl_prefix_s *new_p = _get_pfx_from_store(pkt->pfx);
        ccnl_helper_create_cont(new_p, pkt->pfx->comp[pkt->pfx->compcnt-1],
                                pkt->pfx->complen[pkt->pfx->compcnt-1], true);
        /* do not do anything else for a store interest */
        free_prefix(prefix);
        return 1;
    }
    free_prefix(prefix);

    /* check for someone requesting all the data */
    prefix = ccnl_URItoPrefix(all_pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH)) {
        if (cluster_state == CLUSTER_STATE_DEPUTY) {
            _send_ack(relay, from, pkt->pfx);
            /* schedule transmitting content store */
            msg_t m;
            m.type = CLUSTER_MSG_ALLDATA;
            msg_send(&m, cluster_pid);
        }
        free_prefix(prefix);
        return 1;
    }
    free_prefix(prefix);

    /* check if handover is done */
    prefix = ccnl_URItoPrefix(done_pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH)) {
        if (cluster_state == CLUSTER_STATE_TAKEOVER) {
            _send_ack(relay, from, pkt->pfx);
            puts("cluster: I'm the new deputy");
            cluster_state = CLUSTER_STATE_DEPUTY;
        }
        else {
            puts("!!! cluster: received unexpected \"done\"");
        }
        free_prefix(prefix);
        return 1;
    }

    free_prefix(prefix);
    return 0;
}

void ccnl_helper_send_all_data(void)
{
    struct ccnl_content_s *cit;
    for (cit = ccnl_relay.contents; cit; cit = cit->next) {
        /* send store interest for each object in cache */
        char *p_str = ccnl_prefix_to_path_detailed(cit->pkt->pfx, 1, 0, 0);
        ccnl_helper_int((unsigned char*) p_str, cit->pkt->content, cit->pkt->contlen);
    }
    /* send done message */
    char done_pfx[] = CCNLRIOT_DONE_PREFIX;
    if (ccnl_helper_int((unsigned char*) done_pfx, NULL, 0)) {
        cluster_sleep(CLUSTER_SIZE);
    }
}

static int _wait_for_chunk(void *buf, size_t buf_len, uint64_t timeout)
{
    int res = (-1);

    if (timeout == 0) {
        timeout = CCNL_MAX_INTEREST_RETRANSMIT * SEC_IN_USEC;
    }

    while (1) { /* wait for a content pkt (ignore interests) */
        DEBUGMSG(DEBUG, "  waiting for packet\n");

        /* TODO: receive from socket or interface */
        msg_t m;
        if (xtimer_msg_receive_timeout(&m, timeout) >= 0) {
            DEBUGMSG(DEBUG, "received something\n");
        }
        else {
            /* TODO: handle timeout reasonably */
            DEBUGMSG(WARNING, "timeout\n");
            res = -ETIMEDOUT;
            break;
        }

        if (m.type == GNRC_NETAPI_MSG_TYPE_RCV) {
            DEBUGMSG(TRACE, "It's from the stack!\n");
            gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *)m.content.ptr;
            DEBUGMSG(DEBUG, "Type is: %i\n", pkt->type);
            if (pkt->type == GNRC_NETTYPE_CCN_CHUNK) {
                char *c = (char*) pkt->data;
                DEBUGMSG(INFO, "Content is: %s\n", c);
                size_t len = (pkt->size > buf_len) ? buf_len : pkt->size;
                memcpy(buf, pkt->data, len);
                res = (int) len;
                gnrc_pktbuf_release(pkt);
            }
            else {
                DEBUGMSG(WARNING, "Unkown content\n");
                gnrc_pktbuf_release(pkt);
                continue;
            }
            break;
        }
        else {
            /* TODO: reduce timeout value */
            DEBUGMSG(DEBUG, "Unknow message received, ignore it\n");
        }
    }

    return res;
}


int ccnl_helper_int(unsigned char *prefix, unsigned char *value, size_t len)
{
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[CCNLRIOT_ADDRLEN];
    memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = 0;

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

        printf("cluster: sending interest for %s\n", prefix);
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

   
            struct ccnl_prefix_s *pfx = ccnl_URItoPrefix((char*) prefix, CCNL_SUITE_NDNTLV,
                                                    NULL, 0);
            struct ccnl_content_s *c =
                ccnl_helper_create_cont(pfx, (unsigned char*)
                                        CCNLRIOT_CONT_ACK,
                                        strlen(CCNLRIOT_CONT_ACK), false);
            ccnl_content_remove(&ccnl_relay, c);
            free_prefix(pfx);
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

    return success;
}


