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

/* prototypes from CCN-lite */
void free_packet(struct ccnl_pkt_s *pkt);
struct ccnl_prefix_s* ccnl_prefix_new(int suite, int cnt);

/* create a content struct */
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
    if (pk == NULL) {
        LOG_ERROR("ccnl_helper: something went terribly wrong!\n");
        return NULL;
    }
    c = ccnl_content_new(&ccnl_relay, &pk);
    if (cache) {
        ccnl_content_add2cache(&ccnl_relay, c);
    }

    return c;
}

struct ccnl_interest_s *ccnl_helper_create_int(struct ccnl_prefix_s *prefix)
{
    int nonce = random_uint32();
    LOG_DEBUG("nonce: %i\n", nonce);

    extern int ndntlv_mkInterest(struct ccnl_prefix_s *name, int *nonce, unsigned char *out, int outlen);
    int len = ndntlv_mkInterest(prefix, &nonce, _int_buf, CCNLRIOT_BUF_SIZE);

    unsigned char *start = _int_buf;
    unsigned char *data = _int_buf;
    struct ccnl_pkt_s *pkt;

    int typ;
    int int_len;

    /* TODO: support other suites */
    if (ccnl_ndntlv_dehead(&data, &len, (int*) &typ, &int_len) || (int) int_len > len) {
        LOG_WARNING("  invalid packet format\n");
        return NULL;
    }
    pkt = ccnl_ndntlv_bytes2pkt(NDN_TLV_Interest, start, &data, &len);

    struct ccnl_face_s *loopback_face = ccnl_get_face_or_create(&ccnl_relay, -1, NULL, 0);
    return ccnl_interest_new(&ccnl_relay, loopback_face, &pkt);
}

/* send an acknowledgement */
static void _send_ack(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_prefix_s *pfx)
{
    struct ccnl_content_s *c =
        ccnl_helper_create_cont(pfx, (unsigned char*)
                                CCNLRIOT_CONT_ACK,
                                strlen(CCNLRIOT_CONT_ACK) + 1, false);
    ccnl_face_enqueue(relay, from, ccnl_buf_new(c->pkt->buf->data,
                                                c->pkt->buf->datalen));

    free_packet(c->pkt);
    ccnl_free(c);
}

/* local callback to handle incoming content chunks */
int ccnlriot_consumer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    (void) from;
    char *pfx_str = ccnl_prefix_to_path_detailed(pkt->pfx, 1, 0, 0);
    LOG_DEBUG("%" PRIu32 " ccnl_helper: local consumer for prefix: %s\n", xtimer_now(),
             pfx_str);
    ccnl_free(pfx_str);

    /* a list of prefixes that should be ignored */
    struct ccnl_prefix_s *prefix;

    char all_pfx[] = CCNLRIOT_ALL_PREFIX;

    prefix = ccnl_URItoPrefix(all_pfx, CCNL_SUITE_NDNTLV, NULL, 0);

    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH)) {
        LOG_DEBUG("ccnl_helper: ignoring this content\n");
        struct ccnl_pkt_s *tmp = pkt;
        struct ccnl_content_s *c = ccnl_content_new(relay, &pkt);
        ccnl_content_serve_pending(relay, c);
        free_packet(tmp);
        ccnl_free(c);
        free_prefix(prefix);
        return 1;
    }
    free_prefix(prefix);

    /* XXX: might be unnecessary du to mutex now */
    /* if we're currently transferring our cache to the new deputy, we do not touch the content store */
    if (cluster_state == CLUSTER_STATE_HANDOVER) {
        LOG_DEBUG("ccnl_helper: we're in handover state, cannot touch content store right now\n");
        free_packet(pkt);
        return 1;
    }

    if (strncmp((char*) pkt->content, CCNLRIOT_CONT_ACK, strlen(CCNLRIOT_CONT_ACK)) == 0) {
        LOG_DEBUG("ccnl_helper: received ACK, flag the content\n");
        /* TODO: flag the content */
        free_packet(pkt);
        return 1;
    }
    /* TODO: implement not being interested in all content */
    /* create a temporary interest */
    ccnl_helper_create_int(pkt->pfx);
    _send_ack(relay, from, pkt->pfx);

    return 0;
}

/* local callback to handle incoming interests */
int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    int res = 0;

    char *pfx_str = ccnl_prefix_to_path_detailed(pkt->pfx, 1, 0, 0);
    LOG_DEBUG("%" PRIu32 " ccnl_helper: local producer for prefix: %s\n", xtimer_now(), pfx_str);

    char all_pfx[] = CCNLRIOT_ALL_PREFIX;

    /* first handle store interests */
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(all_pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH)) {
        if (cluster_state == CLUSTER_STATE_DEPUTY) {
            LOG_INFO("ccnl_helper: acknowledging interest for handover\n");
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

out:
    /* freeing memory */
    ccnl_free(pfx_str);
    free_prefix(prefix);
    return res;
}

/* iterating over content store and transmit everything */
void ccnl_helper_send_all_data(void)
{
    LOG_INFO("\n\nccnl_helper: change to state HANDOVER\n\n");
    cluster_state = CLUSTER_STATE_HANDOVER;
    struct ccnl_content_s *cit;
    for (cit = ccnl_relay.contents; cit; cit = cit->next) {
        printf("###### %p\n", (void*) cit);
        printf("######### %p\n", (void*) cit->pkt);
        printf("############ %p\n", (void*) cit->pkt->pfx);
        if (!cit->pkt) {
            LOG_ERROR("ccnl_helper: cache entry without content!?\n");
        }
        if (!cit->pkt->pfx) {
            LOG_ERROR("ccnl_helper: cache entry without a name!?\n");
        }
        /* send store interest for each object in cache */
        char *p_str = ccnl_prefix_to_path_detailed(cit->pkt->pfx, 1, 0, 0);
        ccnl_helper_publish((unsigned char*) p_str, cit->pkt->content, cit->pkt->contlen);
        ccnl_free(p_str);
    }
    /* TODO: make sure data was received */
    cluster_sleep(cluster_size-1);
}

void ccnl_helper_publish(unsigned char *prefix, unsigned char *value, size_t len)
{
    size_t prefix_len = len;
    if (prefix == NULL) {
        prefix_len += sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 8 + len;
    }
    unsigned char pfx[prefix_len];
    if (prefix == NULL) {
        snprintf((char*) pfx, prefix_len, "%s%s/%08X", CCNLRIOT_SITE_PREFIX,
                 CCNLRIOT_TYPE_PREFIX, cluster_my_id);
        prefix = pfx;
    }

    struct ccnl_prefix_s *p = ccnl_URItoPrefix((char*) prefix, CCNL_SUITE_NDNTLV, NULL, 0);
    struct ccnl_content_s *c = ccnl_helper_create_cont(p, value, len, false);
    ccnl_broadcast(&ccnl_relay, c->pkt);
    free_prefix(p);
    free_packet(c->pkt);
    ccnl_free(c);
}

/* build and send an interest packet
 * if prefix is NULL, the value is used to create a store interest */
int ccnl_helper_int(unsigned char *prefix)
{
    LOG_DEBUG("ccnl_helper: ccnl_helper_int\n");
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[CCNLRIOT_ADDRLEN];
    memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = 0;

    gnrc_netreg_entry_t _ne;

    for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
        LOG_INFO("ccnl_helper: sending interest for %s\n", prefix);
        /* register for content chunks */
        _ne.demux_ctx =  GNRC_NETREG_DEMUX_CTX_ALL;
        _ne.pid = sched_active_pid;
        gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

        ccnl_send_interest(CCNL_SUITE_NDNTLV, (char*) prefix, 0, _int_buf, CCNLRIOT_BUF_SIZE);
        if (ccnl_wait_for_chunk(_cont_buf, CCNLRIOT_BUF_SIZE, 0) > 0) {
            gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
            LOG_DEBUG("ccnl_helper: Content received: %s\n", _cont_buf);
            success++;
            break;
        }
        gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
    }

    if (success == 1) {
        LOG_WARNING("\n +++ SUCCESS +++\n");
    }
    else {
        LOG_WARNING("\n !!! TIMEOUT while waiting for %s !!!\n", prefix);
    }

    return success;
}
