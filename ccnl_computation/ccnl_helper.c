#include "sched.h"
#include "msg.h"
#include "xtimer.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/pktbuf.h"
#include "ccnl-pkt-ndntlv.h"

#include "dow.h"
#include "ccnlriot.h"
#include "log.h"
#include "ccnl_helper.h"

/* public variables */
struct ccnl_prefix_s *ccnl_helper_all_pfx;
struct ccnl_prefix_s *ccnl_helper_my_pfx;
uint8_t ccnl_helper_flagged_cache = 0;
dow_pfx_presence_t dow_pfx_pres[DOW_MAX_PREFIXES];

/* buffers for interests and content */
static unsigned char _int_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _cont_buf[CCNLRIOT_BUF_SIZE];
static unsigned char _out[CCNL_MAX_PACKET_SIZE];

/* internal variables */
#if DOW_INT_INT
static xtimer_t _sleep_timer = { .target = 0, .long_target = 0 };
static msg_t _sleep_msg = { .type = DOW_MSG_BACKTOSLEEP };
#endif

void ccnl_helper_init(void)
{
    unsigned cn = 0;
    char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    ccnl_helper_all_pfx = ccnl_URItoPrefix(all_pfx, CCNL_SUITE_NDNTLV, NULL, &cn);
    ccnl_helper_reset();
    memset(dow_pfx_pres, 0, sizeof(dow_pfx_pres));
}

void ccnl_helper_reset(void)
{
    struct ccnl_content_s *cit;
    for (cit = ccnl_relay.contents; cit; cit = cit->next) {
        cit->flags &= ~CCNL_CONTENT_FLAGS_USER2;
    }
}

void ccnl_helper_subsribe(char c) {
    dow_registered_prefix[0] = '/';
    dow_registered_prefix[1] = c;
    dow_registered_prefix[2] = '\0';
    dow_is_registered = true;

    unsigned cn = 0;
    char tmp[5];
    snprintf(tmp, 5, "%s/%c", CCNLRIOT_TYPE_PREFIX, c);
    ccnl_helper_my_pfx = ccnl_URItoPrefix(tmp, CCNL_SUITE_NDNTLV, NULL, &cn);
    printf("dow_registered_prefix: %s\n", dow_registered_prefix);
}

#if DOW_APMDMR
bool ccnl_helper_prefix_under_represented(char p)
{
    for (unsigned i = 0; i < DOW_MAX_PREFIXES; i++) {
        if (dow_pfx_pres[i].pfx == 0) {
            return false;
        }
        if ((dow_pfx_pres[i].pfx == p) && (dow_pfx_pres[i].count < DOW_APMDMR_MIN_INT)) {
            return true;
        }
    }
    return false;
}

static void _prefix_seen_interest(char p)
{
    LOG_DEBUG("ccnl_helper: seen prefix %c\n", p);
    for (unsigned i = 0; i < DOW_MAX_PREFIXES; i++) {
        if ((dow_pfx_pres[i].pfx == p) || (dow_pfx_pres[i].pfx == 0)) {
            dow_pfx_pres[i].pfx = p;
            dow_pfx_pres[i].count++;
            return;
        }
    }
}
#endif

/**
 * @brief create a content struct
 *
 * @param[in] prefix    name of the content
 * @param[in] value     content itself (will get filled into a
 *                      @ref dow_content_t struct
 * @param[in] len       length of the content
 * @param[in] cache     if true, sends data via loopback to self for caching
 * @param[in] send      if true, sends data via broadcast
 *
 * @returns pointer to new content chunk
 * */
struct ccnl_content_s *ccnl_helper_create_cont(struct ccnl_prefix_s *prefix,
                                               unsigned char *value,
                                               ssize_t len, bool cache,
                                               bool send)
{
    if (len > (DOW_CONT_LEN + 1)) {
        LOG_ERROR("ccnl_helper: Too long content. This is not acceptable!!!\n");
        return NULL;
    }
    int offs = CCNL_MAX_PACKET_SIZE;

    dow_content_t my_cont;
    memset(&my_cont.value, 0, DOW_CONT_LEN + 1);
    memcpy(my_cont.value, value, len);
    my_cont.num = -1;
    len = sizeof(dow_content_t);

    ssize_t arg_len = ccnl_ndntlv_prependContent(prefix, (unsigned char*) &my_cont, len, NULL, NULL, &offs, _out);

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
        extern void ccnl_count_faces(struct ccnl_relay_s *ccnl);
        ccnl_count_faces(&ccnl_relay);
        extern void ccnl_pit_size(struct ccnl_relay_s *ccnl);
        ccnl_pit_size(&ccnl_relay);
        return NULL;
    }
    c = ccnl_content_new(&ccnl_relay, &pk);
    if (send) {
        for (int i = 0; i < DOW_BC_COUNT; i++) {
            ccnl_broadcast(&ccnl_relay, c->pkt);
#if (DOW_BC_COUNT > 1)
            xtimer_usleep(20000);
#endif
        }
    }
    if (cache) {
        /* XXX: always use first (and only IF) */
        uint8_t hwaddr[CCNLRIOT_ADDRLEN];
#if (CCNLRIOT_ADDRLEN == 8)
        gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_ADDRESS_LONG, 0, hwaddr, sizeof(hwaddr));
#else
        gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_ADDRESS, 0, hwaddr, sizeof(hwaddr));
#endif
        sockunion dest;
        dest.sa.sa_family = AF_PACKET;
        memcpy(dest.linklayer.sll_addr, hwaddr, CCNLRIOT_ADDRLEN);
        dest.linklayer.sll_halen = CCNLRIOT_ADDRLEN;
        dest.linklayer.sll_protocol = htons(ETHERTYPE_NDN);
        extern void ccnl_ll_TX(struct ccnl_relay_s *ccnl, struct ccnl_if_s *ifc, sockunion *dest, struct ccnl_buf_s *buf);
        ccnl_ll_TX(&ccnl_relay, &ccnl_relay.ifs[0], &dest, c->pkt->buf);
        free_packet(c->pkt);
        ccnl_free(c);
    }

    return c;
}

/**
 * @brief creates an interestest for a given name
 */
struct ccnl_interest_s *ccnl_helper_create_int(struct ccnl_prefix_s *prefix)
{
    int nonce = random_uint32();
    LOG_DEBUG("ccnl_helper: nonce: %X\n", nonce);

    extern int ndntlv_mkInterest(struct ccnl_prefix_s *name, int *nonce, unsigned char *out, int outlen);
    int len = ndntlv_mkInterest(prefix, &nonce, _int_buf, CCNLRIOT_BUF_SIZE);

    unsigned char *start = _int_buf;
    unsigned char *data = _int_buf;
    struct ccnl_pkt_s *pkt;

    int typ;
    int int_len;

    if (ccnl_ndntlv_dehead(&data, &len, (int*) &typ, &int_len) || (int) int_len > len) {
        LOG_WARNING("ccnl_helper: invalid packet format\n");
        return NULL;
    }
    pkt = ccnl_ndntlv_bytes2pkt(NDN_TLV_Interest, start, &data, &len);

    struct ccnl_face_s *loopback_face = ccnl_get_face_or_create(&ccnl_relay, -1, NULL, 0);
    return ccnl_interest_new(&ccnl_relay, loopback_face, &pkt);
}

#if DOW_INT_INT
static bool _cont_is_dup(struct ccnl_pkt_s *pkt)
{
    assert(pkt->content != NULL);
    /* save old value */
    dow_content_t *cc = (dow_content_t*) pkt->content;
    int old = cc->num;
    /* set to -1 for comparison */
    cc->num = -1;

    for (struct ccnl_content_s *c = ccnl_relay.contents; c; c = c->next) {
        dow_content_t *ccc = (dow_content_t*) c->pkt->content;
        int cold = ccc->num;
        ccc->num = -1;
        if ((c->pkt->buf) && (pkt->buf) &&
            (c->pkt->buf->datalen==pkt->buf->datalen) &&
            !memcmp(c->pkt->buf->data,pkt->buf->data,c->pkt->buf->datalen)) {
            cc->num = old;
            ccc->num = cold;
            return true; /* content is dup, do nothing */
        }
        ccc->num = cold;
    }
    cc->num = old;
    return false;
}
#endif

#if DOW_INT_INT
void ccnl_helper_clear_pit_for_own(void)
{
    /* check if we have a PIT entry for our own content and remove it */
    LOG_DEBUG("ccnl_helper: clear PIT entries for own content\n");
    gnrc_netapi_set(ccnl_pid, NETOPT_CCN, CCNL_CTX_CLEAR_PIT_OWN, &ccnl_relay, sizeof(ccnl_relay));
}
#endif

#if DOW_PSR || DOW_PSR2
void ccnl_helper_publish_content(void)
{
#if DOW_PSR
    uint8_t i = 0, start_pos = (ccnl_relay.contentcnt - DOW_PSR);
#elif DOW_PSR2
    uint8_t i = 0, start_pos = (ccnl_relay.contentcnt - DOW_PSR2);
#endif
    struct ccnl_content_s *cit;

    /* get to start position */
    for (cit = ccnl_relay.contents; (cit && (i < start_pos)); cit = cit->next) {
        i++;
    }

    /* now start publishing */
    for (; cit; cit = cit->next) {
//        if (!dow_is_registered || (ccnl_prefix_cmp(ccnl_helper_my_pfx, NULL, cit->pkt->pfx, CMP_MATCH) >= 2)) {
        ccnl_broadcast(&ccnl_relay, cit->pkt);
        LOG_DEBUG("ccnl_helper: try to publish chunk #%i\n", i);
    }
}
#endif

static xtimer_t _wait_timer = { .target = 0, .long_target = 0 };
static msg_t _timeout_msg;
static int _wait_for_chunk(void *buf, size_t buf_len, bool wait_for_int)
{
    int res = (-1);

    int32_t remaining = (CCNL_MAX_INTEREST_RETRANSMIT ? CCNL_MAX_INTEREST_RETRANSMIT : 1) * CCNLRIOT_INT_TIMEOUT;
    uint32_t now = xtimer_now().ticks32;

    while (1) { /* wait for a content pkt (ignore interests) */
        LOG_DEBUG("ccnl_helper:  waiting for packet\n");

        /* check if there's time left to wait for the content */
        _timeout_msg.type = CCNL_MSG_TIMEOUT;
        remaining -= (xtimer_now().ticks32 - now);
        if (remaining < 0) {
            LOG_WARNING("ccnl_helper: timeout waiting for valid message\n");
            res = -ETIMEDOUT;
            break;
        }
        xtimer_remove(&_wait_timer);
        LOG_DEBUG("remaining time: %u\n", (unsigned) remaining);
        xtimer_set_msg(&_wait_timer, remaining, &_timeout_msg, sched_active_pid);

        msg_t m;
        msg_receive(&m);

        /* we received something from local_consumer */
        if (m.type == DOW_MSG_RECEIVED) {
            if (wait_for_int) {
                LOG_DEBUG("ccnl_helper: that was an chunk - we're currently not interested in\n");
                continue;
            }
            LOG_DEBUG("ccnl_helper: received something, that's good enough for me\n");
            res = CCNLRIOT_RECEIVED_CHUNK;
            xtimer_remove(&_wait_timer);
            break;
        }
        else if (m.type == DOW_MSG_RECEIVED_ACK) {
            if (wait_for_int) {
                LOG_DEBUG("ccnl_helper: that was an chunk - we're currently not interested in\n");
                continue;
            }
            LOG_INFO("ccnl_helper: received ack\n");
            memcpy(buf, CCNLRIOT_CONT_ACK, sizeof(CCNLRIOT_CONT_ACK));
            res = sizeof(CCNLRIOT_CONT_ACK);
            xtimer_remove(&_wait_timer);
            break;
        }
        else if (m.type == DOW_MSG_RECEIVED_INT) {
            if (wait_for_int) {
                LOG_DEBUG("ccnl_helper: received an interest - we're waiting for that\n");
                res = CCNLRIOT_RECEIVED_CHUNK;
                xtimer_remove(&_wait_timer);
                break;
            }
            else {
                LOG_DEBUG("ccnl_helper: we were not waiting for an interest\n");
                continue;
            }
        }
        /* we received a chunk from CCN-Lite */
        else if (m.type == GNRC_NETAPI_MSG_TYPE_RCV) {
            gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *)m.content.ptr;
            LOG_DEBUG("ccnl_helper: Type is: %i\n", pkt->type);
            if (pkt->type == GNRC_NETTYPE_CCN_CHUNK) {
                char *c = (char*) pkt->data;
                LOG_INFO("ccnl_helper: Content is: %s\n", c);
                size_t len = (pkt->size > buf_len) ? buf_len : pkt->size;
                memcpy(buf, pkt->data, len);
                res = (int) len;
                gnrc_pktbuf_release(pkt);
            }
            else {
                LOG_WARNING("ccnl_helper: Unkown content\n");
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
        else if (m.type == DOW_MSG_NEWDATA) {
            LOG_INFO("ccnl_helper: received newdata msg while waiting for content, postpone it\n");
            xtimer_set_msg(&dow_data_timer, US_PER_SEC, &dow_data_msg, dow_pid);
        }
        else if (m.type == DOW_MSG_SECOND) {
            //LOG_DEBUG("ccnl_helper: SECOND: %u\n", (unsigned) dow_period_counter);
            xtimer_remove(&dow_timer);
            if (dow_period_counter == 1) {
                LOG_WARNING("ccnl_helper: we're late!\n");
            }
            else {
                dow_period_counter--;
            }
            xtimer_set_msg(&dow_timer, US_PER_SEC, &dow_wakeup_msg, dow_pid);
        }
        else {
            LOG_DEBUG("ccnl_helper: Unknown message received, ignore it\n");
        }
    }

    return res;
}

/**
 * @brief build and send an interest packet
 *
 * @param[in] wait_for_int if true, waits for interest instead of chunk
 *
 * @returns CCNLRIOT_RECEIVED_CHUNK     if a chunk was received
 * @returns CCNLRIOT_LAST_CN            if an ACK was received
 * @returns CCNLRIOT_TIMEOUT            if nothing was received within the
 *                                      given timeframe
 **/
int ccnl_helper_int(struct ccnl_prefix_s *prefix, unsigned *chunknum, bool wait_for_int)
{
    char _prefix_str[CCNLRIOT_PFX_LEN];
    LOG_DEBUG("ccnl_helper: ccnl_helper_int\n");

    /* clear interest and content buffer */
    memset(_int_buf, '\0', CCNLRIOT_BUF_SIZE);
    memset(_cont_buf, '\0', CCNLRIOT_BUF_SIZE);

    unsigned success = CCNLRIOT_TIMEOUT;

    gnrc_netreg_entry_t _ne;

    /* actual sending of the content */
    for (int cnt = 0; cnt < CCNLRIOT_INT_RETRIES; cnt++) {
        LOG_DEBUG("ccnl_helper: sending interest #%u for %s (%i)\n", (unsigned) cnt,
                 ccnl_prefix_to_path_detailed(_prefix_str, prefix, 1, 0, 0),
                 ((chunknum != NULL) ? (int) *chunknum : -1));
        /* register for content chunks */
        _ne.demux_ctx =  GNRC_NETREG_DEMUX_CTX_ALL;
        _ne.target.pid = sched_active_pid;
        gnrc_netreg_register(GNRC_NETTYPE_CCN_CHUNK, &_ne);

        if (chunknum != NULL) {
            *(prefix->chunknum) = *chunknum;
        }
        ccnl_interest_t i = { .prefix = prefix, .buf = _int_buf, .buflen = CCNLRIOT_BUF_SIZE };
        gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, &i, sizeof(i), GNRC_NETTYPE_CCN);
        gnrc_netapi_send(ccnl_pid, pkt);
        if (_wait_for_chunk(_cont_buf, CCNLRIOT_BUF_SIZE, wait_for_int) > 0) {
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
        LOG_WARNING("\nccnl_helper: +++ SUCCESS +++\n");
    }
    else {
        LOG_WARNING("\nccnl_helper: !!! TIMEOUT while waiting for %s number %i!!!\n",
                    (wait_for_int ? "interest" : "chunk"),
                    ((chunknum != NULL) ? (int) *chunknum : -1));
    }

    return success;
}
