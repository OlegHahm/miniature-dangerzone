#include "sched.h"
#include "msg.h"
#include "xtimer.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/pktbuf.h"
#include "ccnl-pkt-ndntlv.h"

#include "cluster.h"
#include "ccnlriot.h"
#include "log.h"

/* buffers for interests and content */
static unsigned char _int_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _cont_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _out[CCNL_MAX_PACKET_SIZE];
static char _prefix_str[CCNLRIOT_PFX_LEN];

/* prototypes from CCN-lite */
void free_packet(struct ccnl_pkt_s *pkt);
struct ccnl_prefix_s *ccnl_prefix_dup(struct ccnl_prefix_s *prefix);
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
        /* XXX: always use first (and only IF) */
        uint8_t hwaddr[CCNLRIOT_ADDRLEN];
        gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_ADDRESS, 0, hwaddr, sizeof(hwaddr));
        sockunion dest;
        dest.sa.sa_family = AF_PACKET;
        memcpy(&dest.linklayer.sll_addr, hwaddr, CCNLRIOT_ADDRLEN);
        dest.linklayer.sll_halen = CCNLRIOT_ADDRLEN;
        extern void ccnl_ll_TX(struct ccnl_relay_s *ccnl, struct ccnl_if_s *ifc, sockunion *dest, struct ccnl_buf_s *buf);
        ccnl_ll_TX(&ccnl_relay, &ccnl_relay.ifs[0], &dest, c->pkt->buf);
        free_packet(c->pkt);
        ccnl_free(c);
    }

    return c;
}

struct ccnl_interest_s *ccnl_helper_create_int(struct ccnl_prefix_s *prefix)
{
    int nonce = random_uint32();
    LOG_DEBUG("nonce: %X\n", nonce);

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
    if (c == NULL) {
        return;
    }
    ccnl_face_enqueue(relay, from, ccnl_buf_new(c->pkt->buf->data,
                                                c->pkt->buf->datalen));

    free_packet(c->pkt);
    ccnl_free(c);
}


static bool _cont_is_dup(struct ccnl_pkt_s *pkt)
{
    for (struct ccnl_content_s *c = ccnl_relay.contents; c; c = c->next) {
        if ((c->pkt->buf) && (pkt->buf) &&
            (c->pkt->buf->datalen==pkt->buf->datalen) &&
            !memcmp(c->pkt->buf->data,pkt->buf->data,c->pkt->buf->datalen)) {
            return true; // content is dup, do nothing
        }
    }
    return false;
}

/* local callback to handle incoming content chunks */
int ccnlriot_consumer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    (void) from;
    (void) relay;
    LOG_DEBUG("%" PRIu32 " ccnl_helper: local consumer for prefix: %s\n", xtimer_now(),
              ccnl_prefix_to_path_detailed(_prefix_str, pkt->pfx, 1, 0, 0));
    memset(_prefix_str, 0, CCNLRIOT_PFX_LEN);

    /* XXX: might be unnecessary du to mutex now */
    /* if we're currently transferring our cache to the new deputy, we do not touch the content store */
    if (cluster_state == CLUSTER_STATE_HANDOVER) {
        LOG_DEBUG("ccnl_helper: we're in handover state, cannot touch content store right now\n");
        free_packet(pkt);
        return 1;
    }

    /* check if prefix is for ALL and contains an ACK */
    struct ccnl_prefix_s *prefix;
    char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    prefix = ccnl_URItoPrefix(all_pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("ccnl_helper: We're doomed, WE ARE ALL DOOMED! 666\n");
        return 1;
    }
    if ((ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH) >= 1) &&
        (strncmp((char*) pkt->content, CCNLRIOT_CONT_ACK, strlen(CCNLRIOT_CONT_ACK)) == 0)) {
        LOG_DEBUG("ccnl_helper: received ACK, flag the content\n");
        msg_t m = { .type = CLUSTER_MSG_RECEIVED_ACK };
        msg_try_send(&m, cluster_pid);
        free_prefix(prefix);
        /* TODO: flag the content */
        free_packet(pkt);
        return 1;
    }
    free_prefix(prefix);

    /* TODO: implement not being interested in all content */
    /* create a temporary interest */
    if (_cont_is_dup(pkt)) {
        LOG_DEBUG("ccnl_helper: ignoring duplicate content\n");
    }
    else {
        ccnl_helper_create_int(pkt->pfx);
    }
    /* in case we're waiting for * chunks, try to send a message */
    msg_t m = { .type = CLUSTER_MSG_RECEIVED };
    msg_try_send(&m, cluster_pid);

    return 0;
}

/* local callback to handle incoming interests */
int ccnlriot_producer(struct ccnl_relay_s *relay, struct ccnl_face_s *from,
                      struct ccnl_pkt_s *pkt)
{
    int res = 0;

    LOG_DEBUG("%" PRIu32 " ccnl_helper: local producer for prefix: %s\n",
              xtimer_now(), ccnl_prefix_to_path_detailed(_prefix_str, pkt->pfx, 1, 0, 0));
    memset(_prefix_str, 0, CCNLRIOT_PFX_LEN);

    char all_pfx[] = CCNLRIOT_ALL_PREFIX;

    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(all_pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("ccnl_helper: We're doomed, WE ARE ALL DOOMED! 667\n");
        return 1;
    }
    if (ccnl_prefix_cmp(prefix, NULL, pkt->pfx, CMP_MATCH) >= 1) {
        if ((cluster_state == CLUSTER_STATE_DEPUTY) || (cluster_state == CLUSTER_STATE_HANDOVER)) {
            /* make sure interest contains a chunknumber */
            if ((pkt->pfx->chunknum == NULL) || (*(pkt->pfx->chunknum) == -1)) {
                LOG_WARNING("ccnl_helper: no chunknumber? what a fool!\n");
                free_packet(pkt);
                res = 1;
                goto out;
            }

            /* change state (will be done for each requested chunk, we don't care) */
            LOG_INFO("\n\nccnl_helper: change to state HANDOVER\n\n");
            cluster_state = CLUSTER_STATE_HANDOVER;

            /* find corresponding chunk in store */
            struct ccnl_content_s *cit = relay->contents;
            LOG_DEBUG("ccnl_helper: received %s request for chunk number %i\n",
                      CCNLRIOT_ALL_PREFIX, *(pkt->pfx->chunknum));
            int i;
            for (i = 0; (i < *(pkt->pfx->chunknum)) && (i < CCNLRIOT_CACHE_SIZE) && (cit != NULL); i++) {
                cit = cit->next;
            }

            /* if we reached the end of the store, we send an ACK */
            if ((i >= CCNLRIOT_CACHE_SIZE) || (cit == NULL)) {
                LOG_DEBUG("ccnl_helper: reached end of content store, send ack\n");
                _send_ack(relay, from, prefix);

                /* we can go back to sleep now */
                /* TODO: delay this */
                cluster_sleep(cluster_size-1);
                free_packet(pkt);
                res = 1;
                goto out;
            }

            /* otherwise we rewrite the name before we pass it down */

            /* save old prefix, so we can free its memory */
            struct ccnl_prefix_s *old = pkt->pfx;

            /* now create a new one */
            struct ccnl_prefix_s *new = ccnl_prefix_dup(cit->pkt->pfx);
            pkt->pfx = new;
            ccnl_free(pkt->pfx->chunknum);

            LOG_DEBUG("%" PRIu32 " ccnl_helper: publish content for prefix: %s\n", xtimer_now(),
                      ccnl_prefix_to_path_detailed(_prefix_str, pkt->pfx, 1, 0, 0));
            memset(_prefix_str, 0, CCNLRIOT_PFX_LEN);

            /* free the old prefix */
            free_prefix(old);
            res = 0;
            goto out;
        }
        free_packet(pkt);
        res = 1;
        goto out;
    }

out:
    /* freeing memory */
    free_prefix(prefix);
    return res;
}

void ccnl_helper_publish(unsigned char *prefix, unsigned char *value, size_t len)
{
    size_t prefix_len = len;
    if (prefix == NULL) {
        prefix_len += sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 8 + len;
    }
    unsigned char pfx[prefix_len];
    if (prefix == NULL) {
        snprintf((char*) pfx, prefix_len, "%s%s/%08X/%s", CCNLRIOT_SITE_PREFIX,
                 CCNLRIOT_TYPE_PREFIX, cluster_my_id, (char*) value);
        prefix = pfx;
    }

    struct ccnl_prefix_s *p = ccnl_URItoPrefix((char*) prefix, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("ccnl_helper: We're doomed, WE ARE ALL DOOMED! 668\n");
        return;
    }
    struct ccnl_content_s *c = ccnl_helper_create_cont(p, value, len, false);
    if (c == NULL) {
        free_prefix(p);
        return;
    }
    ccnl_broadcast(&ccnl_relay, c->pkt);
    free_prefix(p);
    free_packet(c->pkt);
    ccnl_free(c);
}

static xtimer_t _wait_timer = { .target = 0, .long_target = 0 };
static msg_t _timeout_msg;
static int _wait_for_chunk(void *buf, size_t buf_len)
{
    int res = (-1);

    int32_t remaining = CCNL_MAX_INTEREST_RETRANSMIT * SEC_IN_USEC;
    uint32_t now = xtimer_now();

    while (1) { /* wait for a content pkt (ignore interests) */
        LOG_DEBUG("  waiting for packet\n");

        _timeout_msg.type = CCNL_MSG_TIMEOUT;
        remaining -= (xtimer_now() - now);
        if (remaining < 0) {
            LOG_WARNING("ccnl_helper: timeout waiting for valid message\n");
            res = -ETIMEDOUT;
            break;
        }
        xtimer_set_msg64(&_wait_timer, remaining, &_timeout_msg, sched_active_pid);
        msg_t m;
        msg_receive(&m);

        if (m.type == CLUSTER_MSG_RECEIVED) {
            LOG_DEBUG("ccnl_helper: received something, that's good enough for me\n");
            res = 1;
            break;
        }
        else if (m.type == CLUSTER_MSG_RECEIVED_ACK) {
            LOG_DEBUG("ccnl_helper: received ack\n");
            memcpy(buf, CCNLRIOT_CONT_ACK, sizeof(CCNLRIOT_CONT_ACK));
            res = sizeof(CCNLRIOT_CONT_ACK);
            break;
        }
        else if (m.type == GNRC_NETAPI_MSG_TYPE_RCV) {
            gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *)m.content.ptr;
            LOG_DEBUG("Type is: %i\n", pkt->type);
            if (pkt->type == GNRC_NETTYPE_CCN_CHUNK) {
                char *c = (char*) pkt->data;
                LOG_INFO("Content is: %s\n", c);
                size_t len = (pkt->size > buf_len) ? buf_len : pkt->size;
                memcpy(buf, pkt->data, len);
                res = (int) len;
                gnrc_pktbuf_release(pkt);
            }
            else {
                LOG_WARNING("Unkown content\n");
                gnrc_pktbuf_release(pkt);
                continue;
            }
            xtimer_remove(&_wait_timer);
            break;
        }
        else if (m.type == CCNL_MSG_TIMEOUT) {
            res = -ETIMEDOUT;
            break;
        }
        else if (m.type == CLUSTER_MSG_NEWDATA) {
            LOG_DEBUG("cluster: received newdata msg while waiting for content\n");
            cluster_new_data();
        }
        else {
            /* TODO: reduce timeout value */
            LOG_DEBUG("Unknow message received, ignore it\n");
        }
    }

    return res;
}

/* build and send an interest packet
 * if prefix is NULL, the value is used to create a store interest */
int ccnl_helper_int(unsigned char *prefix, unsigned *chunknum, bool no_pit)
{
    LOG_DEBUG("ccnl_helper: ccnl_helper_int\n");
    /* initialize address with 0xFF for broadcast */
    uint8_t relay_addr[CCNLRIOT_ADDRLEN];
    memset(relay_addr, UINT8_MAX, CCNLRIOT_ADDRLEN);

    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = CCNLRIOT_TIMEOUT;

    gnrc_netreg_entry_t _ne;

    struct ccnl_interest_s *i;

    for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
        LOG_INFO("ccnl_helper: sending interest for %s\n", prefix);
        /* register for content chunks */
        _ne.demux_ctx =  GNRC_NETREG_DEMUX_CTX_ALL;
        _ne.pid = sched_active_pid;
        gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

        i = ccnl_send_interest(CCNL_SUITE_NDNTLV, (char*) prefix, chunknum, _int_buf, CCNLRIOT_BUF_SIZE);
        if (_wait_for_chunk(_cont_buf, CCNLRIOT_BUF_SIZE) > 0) {
            gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
            LOG_DEBUG("ccnl_helper: Content received: %s\n", _cont_buf);
            success = CCNLRIOT_RECEIVED_CHUNK;
            break;
        }
        gnrc_netreg_unregister(GNRC_NETTYPE_CCN_CHUNK, &_ne);
    }

    if (success == CCNLRIOT_RECEIVED_CHUNK) {
        if (strncmp((char*) _cont_buf, CCNLRIOT_CONT_ACK, strlen(CCNLRIOT_CONT_ACK)) == 0) {
            LOG_DEBUG("ccnl_helper: received ACK, signaling end of takeover\n");
            success = CCNLRIOT_LAST_CN;
        }
        if (no_pit) {
            ccnl_interest_remove(&ccnl_relay, i);
        }
        LOG_WARNING("\n +++ SUCCESS +++\n");
    }
    else {
        LOG_WARNING("\n !!! TIMEOUT while waiting for %s !!!\n", prefix);
    }

    return success;
}
