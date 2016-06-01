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
msg_t _mq[64];

/* public variables */
cluster_state_t cluster_state;
bloom_t cluster_neighbors;
kernel_pid_t cluster_pid = KERNEL_PID_UNDEF;
kernel_pid_t ccnl_pid = KERNEL_PID_UNDEF;
uint32_t cluster_my_id;
uint8_t cluster_prevent_sleep = 0;

/* internal variables */
xtimer_t cluster_timer;
uint32_t cluster_period_counter;
msg_t cluster_wakeup_msg = { .type = CLUSTER_MSG_SECOND };

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
static void _radio_sleep(void);

/* data timer variables */
xtimer_t cluster_data_timer = { .target = 0, .long_target = 0 };
msg_t cluster_data_msg;

static inline void cluster_second_timer(void)
{
    if (irq_is_in()) {
        LOG_WARNING("\n\nALAAAAAAAAAAARM\n\n");
    }
    if (__get_PRIMASK()) {
        LOG_WARNING("\n\nALAAAAAAAAAAARM2\n\n");
    }
    LOG_DEBUG("%" PRIu32 " cluster: SET SECOND TIMER\n", xtimer_now());
    xtimer_set_msg(&cluster_timer, SEC_IN_USEC, &cluster_wakeup_msg, cluster_pid);
}

/* main event loop */
void *_loop(void *arg)
{
    (void) arg;

    /* initialize timer variables */
    cluster_timer.target = cluster_timer.long_target =  0;

    cluster_data_msg.type = CLUSTER_MSG_NEWDATA;

    /* initialize other stuff */
    msg_init_queue(_mq, (sizeof(_mq) / sizeof(msg_t)));

#if CLUSTER_DEPUTY
    bloom_init(&cluster_neighbors, BLOOM_BITS, _bf, _hashes, BLOOM_HASHF);
#endif

    /* configure the channel */
    uint16_t chan = CCNLRIOT_CHANNEL;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_CHANNEL, 0, (uint16_t *)&chan, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting channel\n");
    }

#if CLUSTER_DEPUTY
    /* do some beaconing */
    beaconing_start();
#endif

#ifndef BOARD_NATIVE
    unsigned int addr_len = CCNLRIOT_ADDRLEN;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_SRC_LEN, 0, (uint16_t *)&addr_len, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting addressing mode\n");
    }
#endif


    /* initialize and start CCN lite */
    ccnl_core_init();
    ccnl_helper_init();
    extern int debug_level;
    debug_level = LOG_DEBUG;

    ccnl_relay.max_cache_entries = CCNLRIOT_CACHE_SIZE;
    ccnl_pid = ccnl_start();
    /* let CCN start */
    xtimer_usleep(100000);

    if (ccnl_open_netif(CCNLRIOT_NETIF, GNRC_NETTYPE_CCN) < 0) {
        LOG_ERROR("main: critical error, aborting\n");
        return NULL;
    }

    random_init(cluster_my_id);
    /* set the CCN callbacks */
    ccnl_set_local_producer(ccnlriot_producer);
    ccnl_set_local_consumer(ccnlriot_consumer);
    ccnl_set_cache_strategy_remove(cs_oldest_representative);

    /* start data generation timer */
    uint32_t offset = CLUSTER_EVENT_PERIOD;
    LOG_DEBUG("cluster: Next event in %" PRIu32 " seconds (%i)\n", (offset / 1000000), (int) cluster_pid);
    xtimer_set_msg(&cluster_data_timer, offset, &cluster_data_msg, cluster_pid);

#if CLUSTER_DEPUTY
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
#else
    if (CLUSTER_GO_SLEEP) {
        LOG_INFO("\n\ncluster: starting INACTIVE\n\n");
        cluster_state = CLUSTER_STATE_INACTIVE;
        cluster_sleep(CLUSTER_X * CLUSTER_D);
    }
    else {
        LOG_INFO("\n\ncluster: starting as DEPUTY\n\n");
        cluster_state = CLUSTER_STATE_DEPUTY;
        cluster_period_counter = CLUSTER_X * CLUSTER_D;
        cluster_second_timer();
    }
#endif

    while (1) {
        msg_t m;
        msg_receive(&m);
        switch (m.type) {
            case CLUSTER_MSG_SECOND:
                LOG_DEBUG("cluster: SECOND: %u\n", (unsigned) cluster_period_counter);
                xtimer_remove(&cluster_timer);
                if (--cluster_period_counter == 0) {
                    LOG_DEBUG("cluster: time to reconsider my state\n");
#if CLUSTER_DEPUTY
                    cluster_takeover();
#else
                    if (cluster_state == CLUSTER_STATE_INACTIVE) {
                        if (CLUSTER_GO_SLEEP) {
                            LOG_INFO("cluster: stay sleeping\n");
                        }
                        else {
                            LOG_INFO("\n\ncluster: change to state DEPUTY\n\n");
                            cluster_state = CLUSTER_STATE_DEPUTY;
                            cluster_wakeup();
                        }
                        cluster_period_counter = CLUSTER_X * CLUSTER_D;
                        cluster_second_timer();
                    }
                    else {
                        if (CLUSTER_GO_SLEEP) {
                            LOG_INFO("cluster: go sleeping\n");
                            cluster_state = CLUSTER_STATE_INACTIVE;
                            cluster_sleep(CLUSTER_X * CLUSTER_D);
                        }
                        else {
                            LOG_INFO("cluster: stay active\n");
                            cluster_period_counter = CLUSTER_X * CLUSTER_D;
                            cluster_second_timer();
                        }
                    }
#endif
                }
                else {
                    cluster_second_timer();
                }
                break;
#if CLUSTER_DEPUTY
            case CLUSTER_MSG_TAKEOVER:
                LOG_DEBUG("cluster: received takeover msg\n");
                cluster_takeover();
                break;
#endif
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
                _radio_sleep();
                break;
            case CLUSTER_MSG_INACTIVE:
                LOG_DEBUG("cluster: received request to go inactive\n");
                cluster_sleep(cluster_size-1);
                break;
            case CLUSTER_MSG_BEACON:
                LOG_WARNING("cluster: should receive beaconing message now!\n");
                break;
            case CLUSTER_MSG_RECEIVED_INT:
                LOG_DEBUG("cluster: we're not interested in interests right now\n");
                break;
            case CLUSTER_MSG_RECEIVED:
                LOG_DEBUG("cluster: received a content chunk ");
                LOG_DEBUG("cluster: nothing to do\n");
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
    uint8_t cpuid[CPUID_LEN];
    cpuid_get(cpuid);
    cluster_my_id = djb2_hash(cpuid, CPUID_LEN);
#endif
    LOG_INFO("clustering: my ID  is %u\n", (unsigned) cluster_my_id);

    random_init(cluster_my_id);
    /* initialize to inactive state */
    LOG_INFO("\n\ncluster: change to state INACTIVE\n\n");
    cluster_state = CLUSTER_STATE_INACTIVE;

    cluster_pid = thread_create(_cluster_stack, sizeof(_cluster_stack),
                                THREAD_PRIORITY_MAIN-1, THREAD_CREATE_STACKTEST |
                                THREAD_CREATE_WOUT_YIELD, _loop, NULL, "cluster manager");
}

static xtimer_t _sleep_timer = { .target = 0, .long_target = 0 };
static msg_t _sleep_msg = { .type = CLUSTER_MSG_BACKTOSLEEP };
static void _radio_sleep(void)
{
#if CLUSTER_DEPUTY
    if (cluster_prevent_sleep > 0) {
        if (ccnl_relay.pit == NULL) {
            LOG_DEBUG("cluster: no PIT entries, reset pending counter\n");
            cluster_prevent_sleep = 0;
        }
        else {
            LOG_DEBUG("cluster: still waiting for an interest from the deputy, wait some time\n");
            xtimer_remove(&_sleep_timer);
            xtimer_set_msg(&_sleep_timer, CLUSTER_STAY_AWAKE_PERIOD, &_sleep_msg, cluster_pid);
            return;
        }
    }
#endif
    netopt_state_t state;
    if (gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) > 0) {
        if (state == NETOPT_STATE_IDLE) {
            LOG_DEBUG("cluster: sending radio to sleep\n");
            state = NETOPT_STATE_SLEEP;
            if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
                LOG_WARNING("cluster: error sending radio to sleep\n");
            }
        }
        else if (state == NETOPT_STATE_SLEEP) {
            LOG_DEBUG("cluster: radio sleeps already\n");
        }
        else {
            LOG_DEBUG("cluster: radio is in wrong state (%x), try again later\n", state);
            xtimer_remove(&_sleep_timer);
            xtimer_set_msg(&_sleep_timer, CLUSTER_STAY_AWAKE_PERIOD, &_sleep_msg, cluster_pid);
        }
    }
    else {
        LOG_WARNING("cluster: error requesting radio state\n");
    }
}

void cluster_sleep(uint8_t periods)
{
    LOG_INFO("cluster: going to sleep\n");
    gnrc_netapi_set(ccnl_pid, NETOPT_CCN, CCNL_CTX_CLEAR_PIT_BUT_OWN, &ccnl_relay, sizeof(ccnl_relay));
    LOG_INFO("\n\ncluster: change to state INACTIVE\n\n");
    cluster_state = CLUSTER_STATE_INACTIVE;

    _radio_sleep();

#if CLUSTER_DEPUTY
    LOG_DEBUG("cluster: wakeup in %u seconds (%i)\n", ((periods * CLUSTER_PERIOD)), (int) cluster_pid);
    cluster_period_counter = periods * CLUSTER_PERIOD;
#else
    LOG_DEBUG("cluster: wakeup in %u seconds (%i)\n", ((periods)), (int) cluster_pid);
    cluster_period_counter = periods;
#endif
    xtimer_remove(&cluster_timer);
    cluster_second_timer();
}

#if CLUSTER_DEPUTY
void cluster_takeover(void)
{
    LOG_INFO("\n\ncluster: change to state DEPUTY\n\n");
    ccnl_helper_clear_pit_for_own();
    cluster_state = CLUSTER_STATE_DEPUTY;
    cluster_wakeup();
    unsigned cn = 0;
    while (ccnl_helper_int(ccnl_helper_all_pfx, &cn, false) != CCNLRIOT_LAST_CN) {
        cn++;
    }
    LOG_DEBUG("cluster: takeover completed\n");
}
#endif

void cluster_wakeup(void)
{
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
}

void cluster_new_data(void)
{
    /* XXX: save postponed value */
    if (cluster_state == CLUSTER_STATE_HANDOVER) {
        LOG_DEBUG("cluster: currently in handover, let's postpone by a second\n");
        xtimer_set_msg(&cluster_data_timer, SEC_IN_USEC, &cluster_data_msg, cluster_pid);
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
    size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 9 + 9;
    char pfx[prefix_len];
    snprintf(pfx, prefix_len, "%s%s/%08lX/%s", CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, (long unsigned) cluster_my_id, val);
    printf("cluster: NEW DATA: %s\n", pfx);
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("cluster: We're doomed, WE ARE ALL DOOMED!\n");
    }
    else {
        ccnl_helper_create_cont(prefix, (unsigned char*) val, sizeof(val), true, true);
        free_prefix(prefix);
    }

    /* schedule new data generation */
    uint32_t offset = CLUSTER_EVENT_PERIOD;
    LOG_DEBUG("cluster: Next event in %" PRIu32 " seconds (%i)\n", (offset / 1000000), (int) cluster_pid);
    netopt_state_t state;
    if (gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) > 0) {
        LOG_DEBUG("cluster: current radio state is %X\n", state);
    }
    xtimer_set_msg(&cluster_data_timer, offset, &cluster_data_msg, cluster_pid);
}
