#include "msg.h"
#include "xtimer.h"
#include "bloom.h"
#include "bitfield.h"
#include "hashes.h"
#include "periph/cpuid.h"
#include "net/netopt.h"
#include "net/gnrc/netapi.h"

#include "cluster.h"
#include "config.h"
#include "ccnlriot.h"

/* buffers */
char _cluster_stack[THREAD_STACKSIZE_MAIN];
msg_t _mq[8];

/* public variables */
cluster_state_t cluster_state;
bloom_t cluster_neighbors;
kernel_pid_t cluster_pid = KERNEL_PID_UNDEF;
uint16_t cluster_my_id;

/* internal variables */
static xtimer_t _cluster_timer;

/* bloom filter for beaconing */
#define BLOOM_BITS (1 << 12)
#define BLOOM_HASHF (8)

BITFIELD(_bf, BLOOM_BITS);
hashfp_t _hashes[BLOOM_HASHF] = {
    (hashfp_t) fnv_hash, (hashfp_t) sax_hash, (hashfp_t) sdbm_hash,
    (hashfp_t) djb2_hash, (hashfp_t) kr_hash, (hashfp_t) dek_hash,
    (hashfp_t) rotating_hash, (hashfp_t) one_at_a_time_hash,
};

/* prototypes */
static void _populate_data(char *pfx);

/* data timer variables */
static xtimer_t _data_timer;
static msg_t _data_msg;

/* main event loop */
void *_loop(void *arg)
{
    (void) arg;

    /* initialize timer variables */
    _cluster_timer.target = _cluster_timer.long_target = _data_timer.target =
        _data_timer.long_target =  0;

    _data_msg.type = CLUSTER_MSG_NEWDATA;

    /* initialize other stuff */
    msg_init_queue(_mq, (sizeof(_mq) / sizeof(msg_t)));

    bloom_init(&cluster_neighbors, BLOOM_BITS, _bf, _hashes, BLOOM_HASHF);

    /* configure the channel */
    uint16_t chan = CCNLRIOT_CHANNEL;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_CHANNEL, 0, (uint16_t *)&chan, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting channel\n");
    }

    /* do some beaconing */
    beaconing_start();

#ifndef BOARD_NATIVE
    unsigned int addr_len = CCNLRIOT_ADDRLEN;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_SRC_LEN, 0, (uint16_t *)&addr_len, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting addressing mode\n");
    }
#endif


    /* initialize and start CCN lite */
    ccnl_core_init();
    extern int debug_level;
    debug_level = CCNLRIOT_LOGLEVEL;
    ccnl_relay.max_cache_entries = CCNLRIOT_CACHE_SIZE;
    ccnl_start();

    if (ccnl_open_netif(CCNLRIOT_NETIF, GNRC_NETTYPE_CCN) < 0) {
        LOG_ERROR("main: critical error, aborting\n");
        return NULL;
    }

    random_init(cluster_my_id);
    /* set the CCN callbacks */
    ccnl_set_local_producer(ccnlriot_producer);
    ccnl_set_local_consumer(ccnlriot_consumer);

    /* start data generation timer */
    uint32_t offset = CLUSTER_EVENT_PERIOD;
    LOG_DEBUG("cluster: Next event in %" PRIu32 " seconds (%i)\n", (offset / 1000000), (int) cluster_pid);
    xtimer_set_msg(&_data_timer, offset, &_data_msg, cluster_pid);

    /* enter correct state and set timer if necessary */
    if (cluster_position == 0) {
        LOG_INFO("cluster: I'm the first deputy\n");
        /* become deputy now */
        LOG_INFO("\n\ncluster: change to state DEPUTY\n\n");
        cluster_state = CLUSTER_STATE_DEPUTY;
    }
    else {
        /* go to sleep and set timer */
        LOG_INFO("\n\ncluster: change to state INACTIVE\n\n");
        cluster_state = CLUSTER_STATE_INACTIVE;
        cluster_sleep(cluster_position);
    }

    while (1) {
        msg_t m;
        msg_receive(&m);
        switch (m.type) {
            case CLUSTER_MSG_TAKEOVER:
                LOG_DEBUG("cluster: received takeover msg\n");
                cluster_takeover();
                break;
            case CLUSTER_MSG_NEWDATA:
                LOG_DEBUG("cluster: received newdata msg\n");
                cluster_new_data();
                break;
            case CLUSTER_MSG_BACKTOSLEEP:
                LOG_DEBUG("cluster: received backtosleep request\n");
                if (cluster_state != CLUSTER_STATE_INACTIVE) {
                    LOG_WARNING("cluster: currently not in inactive state, sleeping would be bad idea\n");
                    break;
                }
                netopt_state_t state = NETOPT_STATE_SLEEP;
                if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
                    LOG_WARNING("cluster: error going to sleep\n");
                }
                break;
            case CLUSTER_MSG_BEACON:
                LOG_WARNING("cluster: should receive beaconing message now!\n");
                break;
            case CLUSTER_MSG_RECEIVED:
                LOG_DEBUG("cluster: received content, no special treatment necessary\n");
                break;
            default:
                LOG_WARNING("cluster: I don't understand this message: %X\n", m.type);
        }
    }
    return NULL;
}

void cluster_init(void)
{
#ifdef CPU_NATIVE
    cpuid_get(&cluster_my_id);
#else
    /* XXX: limited to addresses of max. 8 chars */
    uint16_t hwaddr[4];
    if (gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_ADDRESS, 0, (uint8_t*) hwaddr, sizeof(hwaddr)) <= 0) {
        LOG_ERROR("cluster: I'm doomed!\n");
    }
    cluster_my_id = (uint16_t) *((uint16_t*) &hwaddr);
#endif
    LOG_DEBUG("clustering: my ID  is %u\n", (unsigned) cluster_my_id);

    random_init(cluster_my_id);
    /* initialize to inactive state */
    LOG_INFO("\n\ncluster: change to state INACTIVE\n\n");
    cluster_state = CLUSTER_STATE_INACTIVE;

    cluster_pid = thread_create(_cluster_stack, sizeof(_cluster_stack),
                         THREAD_PRIORITY_MAIN-1, THREAD_CREATE_STACKTEST |
                         THREAD_CREATE_WOUT_YIELD, _loop, NULL, "cluster manager");
}

static msg_t _wakeup_msg;
void cluster_sleep(uint8_t periods)
{
    LOG_INFO("cluster: going to sleep\n");
    LOG_INFO("\n\ncluster: change to state INACTIVE\n\n");
    cluster_state = CLUSTER_STATE_INACTIVE;
    netopt_state_t state = NETOPT_STATE_SLEEP;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        LOG_WARNING("cluster: error going to sleep\n");
    }
    _wakeup_msg.type = CLUSTER_MSG_TAKEOVER;
    LOG_DEBUG("cluster: wakeup in %u seconds (%i)\n", ((periods * CLUSTER_PERIOD) / SEC_IN_USEC), (int) cluster_pid);
    xtimer_set_msg(&_cluster_timer, periods * CLUSTER_PERIOD, &_wakeup_msg, cluster_pid);
}

void cluster_takeover(void)
{
    LOG_INFO("\n\ncluster: change to state DEPUTY\n\n");
    cluster_state = CLUSTER_STATE_DEPUTY;
    cluster_wakeup();
    unsigned char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    for (unsigned cn = 0; cn <= CCNLRIOT_CACHE_SIZE; cn++) {
        if (ccnl_helper_int(all_pfx, &cn, true, false) == CCNLRIOT_LAST_CN) {
            LOG_DEBUG("cluster: received final chunk\n");
            break;
        }
    }
    LOG_DEBUG("cluster: takeover completed\n");
}

void cluster_wakeup(void)
{
    netopt_state_t state = NETOPT_STATE_IDLE;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        LOG_WARNING("cluster: error waking up\n");
    }
}

void cluster_new_data(void)
{
    /* XXX: save postponed value */
    if (cluster_state == CLUSTER_STATE_HANDOVER) {
        LOG_DEBUG("cluster: currently in handover, let's postpone by a second\n");
        xtimer_set_msg(&_data_timer, SEC_IN_USEC, &_data_msg, cluster_pid);
        return;
    }

    /* XXX: implement event */
    unsigned data = xtimer_now();
    /* each byte needs 2 characters to be represented as a hex value */
    /* string representation */
    char val[sizeof(data) * 2];
    snprintf(val, sizeof(val) + 1, "%08X", data);

    /* get data into cache by sending to loopback */
    LOG_DEBUG("cluster: put data into cache via loopback\n");
    /* for the deputy we put the content directly into the store */
    size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 9 + 9;
    char pfx[prefix_len];
    snprintf(pfx, prefix_len, "%s%s/%08X/%s", CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, cluster_my_id, val);
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("cluster: We're doomed, WE ARE ALL DOOMED!\n");
    }
    else {
        ccnl_helper_create_cont(prefix, (unsigned char*) val, sizeof(val), true);
        free_prefix(prefix);
    }

    /* if we're not deputy or becoming one, send an interest for our own data */
    if ((cluster_state != CLUSTER_STATE_DEPUTY) &&
        (cluster_state != CLUSTER_STATE_TAKEOVER)) {
        LOG_DEBUG("cluster: call _populate_data\n");
        snprintf(pfx, prefix_len, "%s%s/%08X/%s", CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, cluster_my_id, val);
        _populate_data(pfx);
    }
    /* schedule new data generation */
    uint32_t offset = CLUSTER_EVENT_PERIOD;
    LOG_DEBUG("Next event in %" PRIu32 " seconds (%i)\n", (offset / 1000000), (int) cluster_pid);
    xtimer_set_msg(&_data_timer, offset, &_data_msg, cluster_pid);
}

static void _populate_data(char *pfx)
{
    /* first wake up radio (if necessary) */
    LOG_DEBUG("cluster: entering _populate_data\n");
    netopt_state_t state;
    if (gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) > 0) {
        if (state == NETOPT_STATE_SLEEP) {
            LOG_DEBUG("cluster: waking up radio\n");
            state = NETOPT_STATE_IDLE;
            if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
                LOG_WARNING("cluster: error waking up\n");
            }
        }
        else {
            LOG_DEBUG("cluster: radio is already on\n");
        }
    }
    else {
        LOG_WARNING("cluster: error requesting radio state\n");
    }
    /* populate the content now */
    ccnl_helper_int((unsigned char*) pfx, NULL, false, true);
}
