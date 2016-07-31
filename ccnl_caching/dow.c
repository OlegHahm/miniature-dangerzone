#include "msg.h"
#include "xtimer.h"
#include "bitfield.h"
#include "bloom.h"
#include "hashes.h"
#include "periph/cpuid.h"
#include "net/netopt.h"
#include "net/netstats.h"
#include "net/gnrc/netapi.h"

#include "dow.h"
#include "config.h"
#include "ccnlriot.h"

/* buffers */
char _dow_stack[THREAD_STACKSIZE_MAIN];
msg_t _mq[DOW_MSG_QUEUE_SIZE];

/* public variables */
float DOW_P = DOW_START_P;
dow_state_t dow_state;
bloom_t dow_neighbors;
kernel_pid_t dow_pid = KERNEL_PID_UNDEF;
kernel_pid_t ccnl_pid = KERNEL_PID_UNDEF;
uint32_t dow_my_id;
uint8_t dow_prevent_sleep = 0;
uint32_t dow_time_sleeping = 0;
uint32_t dow_time_active = 0;
bool dow_sleeping = false;
uint32_t dow_ts_wakeup = 0;
uint32_t dow_ts_sleep = 0;
uint8_t dow_prio_cache_cnt = 0;
uint8_t dow_my_prefix_interest_count = 0;

unsigned dow_cache_size = CCNLRIOT_CACHE_SIZE;

/* internal variables */
xtimer_t dow_timer;
uint32_t dow_period_counter;
#if DOW_DEPUTY
uint32_t _sleep_period;
uint32_t _last_deputy_ts;
#endif
#if DOW_APMDMR
int32_t _dow_phase_counter = DOW_FIRST_PHASE;
#endif
msg_t dow_wakeup_msg = { .type = DOW_MSG_SECOND };

#if DOW_REBC_LAST
unsigned _old_data = 0;
bool _send_old_data = false;
#endif

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
#if DOW_INT_INT
static void _populate_data(struct ccnl_prefix_s *pfx);
#endif
static void _radio_sleep(void);

#if (DOW_DEPUTY == 0) && (DOW_PER)
static void _send_interests(void);
#endif

/* data timer variables */
xtimer_t dow_data_timer = { .target = 0, .long_target = 0 };
msg_t dow_data_msg;

static inline void dow_second_timer(void)
{
#ifndef BOARD_NATIVE
    if (irq_is_in()) {
        LOG_WARNING("\n\nALAAAAAAAAAAARM\n\n");
    }
    if (__get_PRIMASK()) {
        LOG_WARNING("\n\nALAAAAAAAAAAARM2\n\n");
    }
#endif
    LOG_DEBUG("%" PRIu32 " dow: SET SECOND TIMER\n", xtimer_now());
    xtimer_set_msg(&dow_timer, SEC_IN_USEC, &dow_wakeup_msg, dow_pid);
}

static void _reset_netstats(void)
{
    netstats_t *stats;
    if (gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_STATS, 0, &stats, sizeof(&stats)) <= 0) {
        LOG_ERROR("dow: could not reset netstats\n");
        return;
    }
    memset(stats, 0, sizeof(netstats_t));
    LOG_INFO("dow: reset netstats\n");
}

/* main event loop */
void *_loop(void *arg)
{
    (void) arg;

    /* initialize timer variables */
    dow_timer.target = dow_timer.long_target =  0;

    dow_data_msg.type = DOW_MSG_NEWDATA;

    /* initialize other stuff */
    msg_init_queue(_mq, (sizeof(_mq) / sizeof(msg_t)));

#if DOW_DEPUTY
    bloom_init(&dow_neighbors, BLOOM_BITS, _bf, _hashes, BLOOM_HASHF);
#endif

    /* configure the channel */
    uint16_t chan = CCNLRIOT_CHANNEL;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_CHANNEL, 0, (uint16_t *)&chan, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting channel\n");
    }

#if DOW_DEPUTY
    /* do some beaconing */
    beaconing_start();
#endif

#ifndef BOARD_NATIVE
    unsigned int addr_len = CCNLRIOT_ADDRLEN;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_SRC_LEN, 0, (uint16_t *)&addr_len, sizeof(uint16_t)) < 0) {
        LOG_ERROR("main: error setting addressing mode\n");
    }
#endif

    _reset_netstats();
    dow_ts_wakeup = xtimer_now();
    /* initialize and start CCN lite */
    ccnl_core_init();
    ccnl_helper_init();
    extern int debug_level;
    debug_level = LOG_DEBUG;

    ccnl_relay.max_cache_entries = dow_cache_size;
    ccnl_pid = ccnl_start();

    if (ccnl_open_netif(CCNLRIOT_NETIF, GNRC_NETTYPE_CCN) < 0) {
        LOG_ERROR("main: critical error, aborting\n");
        return NULL;
    }

    random_init(dow_my_id);
    /* set the CCN callbacks */
    ccnl_set_local_producer(ccnlriot_producer);
    ccnl_set_local_consumer(ccnlriot_consumer);
#if DOW_CACHE_REPL_STRAT
    ccnl_set_cache_strategy_remove(cs_oldest_representative);
#endif

#if !DOW_DEBUG
    /* start data generation timer */
    uint32_t offset = DOW_EVENT_PERIOD_JITTER;
    LOG_DEBUG("dow: Next event in %" PRIu32 " seconds (%i)\n", (offset / 1000000), (int) dow_pid);
    xtimer_set_msg(&dow_data_timer, offset, &dow_data_msg, dow_pid);

#if DOW_DEPUTY
    /* enter correct state and set timer if necessary */
    if (dow_position == 0) {
        LOG_INFO("dow: I'm the first deputy\n");
        /* become deputy now */
        LOG_INFO("\n\ndow: change to state DEPUTY\n\n");
        dow_state = DOW_STATE_DEPUTY;
    }
    else {
        /* go to sleep and set timer */
        LOG_INFO("\n\ndow: change to state INACTIVE (%u)\n\n", (unsigned) dow_position);
        _last_deputy_ts = xtimer_now();
        dow_state = DOW_STATE_INACTIVE;
        dow_sleep(dow_position);
    }
#else
    /* random startup delay */
    xtimer_usleep(DOW_STARTUP_DELAY);
    if (DOW_GO_SLEEP) {
        LOG_INFO("\n\ndow: starting INACTIVE\n\n");
        dow_state = DOW_STATE_INACTIVE;
        dow_sleep(DOW_X * DOW_D);
    }
    else {
        LOG_INFO("\n\ndow: starting as DEPUTY\n\n");
        dow_state = DOW_STATE_DEPUTY;
        dow_period_counter = DOW_X * DOW_D;
        dow_second_timer();
    }
#endif
#endif

    while (dow_state != DOW_STATE_STOPPED) {
        msg_t m;
        msg_receive(&m);
        if (dow_state == DOW_STATE_STOPPED) {
            break;
        }
        switch (m.type) {
            case DOW_MSG_SECOND:
                LOG_DEBUG("dow: SECOND: %u\n", (unsigned) dow_period_counter);
                xtimer_remove(&dow_timer);
                dow_period_counter--;
#if DOW_APMDMR
                if (dow_state != DOW_STATE_INACTIVE) {
                    _dow_phase_counter--;
                    if (_dow_phase_counter == 0) {
                        if (ccnl_helper_prefix_under_represented(dow_registered_prefix[1])) {
                            LOG_WARNING("dow: my prefix is under-represented, do not switch\n");
                        }
                        else {
                            for (unsigned i = 0; i < DOW_MAX_PREFIXES; i++) {
                                if (ccnl_helper_prefix_under_represented(dow_pfx_pres[i].pfx)) {
                                    LOG_WARNING("dow: switch to prefix: %c\n", dow_pfx_pres[i].pfx);
                                    free_prefix(ccnl_helper_my_pfx);
                                    ccnl_helper_subsribe(dow_pfx_pres[i].pfx);
                                    break;
                                }
                            }
                        }
                        _send_interests();
                    }
                }
#endif

#if DOW_PSR
                if (ccnl_helper_flagged_cache >= (CCNLRIOT_CACHE_SIZE - 1)) {
                    LOG_DEBUG("dow: publish some old date\n");
                    ccnl_helper_publish_content();
                }
                LOG_DEBUG("dow: currently flagged in cache: %u\n", (unsigned) ccnl_helper_flagged_cache);
#endif

#if DOW_DEPUTY
                if (xtimer_now() >= (_last_deputy_ts + (_sleep_period * SEC_IN_USEC))) {
                    if (dow_period_counter > 0) {
                        LOG_WARNING("dow: we are late - apparently missed a second timer\n");
                        dow_period_counter = 0;
                    }
                }
#endif
                if (dow_period_counter == 0) {
                    LOG_DEBUG("dow: time to reconsider my state\n");
#if DOW_DEPUTY
                    dow_takeover();
#else
                    if (dow_state == DOW_STATE_INACTIVE) {
                        if (DOW_GO_SLEEP) {
                            LOG_DEBUG("dow: stay sleeping\n");
                        }
                        else {
                            LOG_INFO("\n\ndow: change to state DEPUTY\n\n");
                            dow_state = DOW_STATE_DEPUTY;
                            ccnl_helper_reset();
                            dow_wakeup();
                        }
                        dow_period_counter = DOW_X * DOW_D;
#if DOW_APMDMR
                        _dow_phase_counter = DOW_FIRST_PHASE;
                        memset(dow_pfx_pres, 0, sizeof(dow_pfx_pres));
#endif
                        dow_second_timer();
#if DOW_KEEP_ALIVE_PFX
                        dow_my_prefix_interest_count = 0;
#endif
                    }
                    else {
                        bool force_awake = false;
                        uint32_t active_period_duration = (DOW_X * DOW_D);
#if DOW_KEEP_ALIVE_PFX
                        if (dow_my_prefix_interest_count < DOW_KEEP_ALIVE_MIN_THR) {
                            LOG_INFO("dow: didn't see enough interests (%i < %i) for my prefix, stay awake\n", (int) dow_my_prefix_interest_count, DOW_KEEP_ALIVE_MIN_THR);
                            force_awake = true;
                            active_period_duration = DOW_D;
                        }
                        dow_my_prefix_interest_count = 0;
#endif

                        if ((!force_awake) && DOW_GO_SLEEP) {
                            LOG_DEBUG("dow: go sleeping\n");
                            dow_state = DOW_STATE_INACTIVE;
                            dow_sleep(DOW_X * DOW_D);
                        }
                        else {
                            LOG_DEBUG("dow: stay active\n");
                            dow_period_counter = active_period_duration;
                            dow_second_timer();
                        }
                    }
#endif
                }
                else {
                    dow_second_timer();
                }
                break;
#if DOW_DEPUTY
            case DOW_MSG_TAKEOVER:
                LOG_DEBUG("dow: received takeover msg\n");
                dow_takeover();
                break;
#endif
            case DOW_MSG_NEWDATA:
                LOG_DEBUG("dow: received newdata msg\n");
                if (dow_is_source) {
                    dow_new_data();
                }
#if !DOW_INT_INT
                /* give the transceiver some time to finish its job */
                if (dow_state == DOW_STATE_INACTIVE) {
                    xtimer_usleep(5000);
                    _radio_sleep();
                }
#endif
                break;
            case DOW_MSG_BACKTOSLEEP:
                LOG_DEBUG("dow: received backtosleep request\n");
                if (dow_state != DOW_STATE_INACTIVE) {
                    LOG_WARNING("dow: currently not in inactive state, sleeping would be bad idea\n");
                    break;
                }
                _radio_sleep();
                break;
            case DOW_MSG_INACTIVE:
                LOG_DEBUG("dow: received request to go inactive\n");
                dow_sleep(dow_size-1);
                break;
            case DOW_MSG_BEACON:
                LOG_WARNING("dow: should receive beaconing message now!\n");
                break;
            case DOW_MSG_RECEIVED_INT:
                LOG_DEBUG("dow: we're not interested in interests right now\n");
                break;
            case DOW_MSG_RECEIVED:
                LOG_DEBUG("dow: received a content chunk ");
#if DOW_INT_INT
                static char _prefix_str[CCNLRIOT_PFX_LEN];
                struct ccnl_prefix_s *pfx = (struct ccnl_prefix_s*)m.content.ptr;
                ccnl_prefix_to_path_detailed(_prefix_str, pfx, 1, 0, 0);

                /* if we're not deputy or becoming one, send an interest for our own data */
                if (dow_state != DOW_STATE_DEPUTY) {
                    LOG_DEBUG("assume that is from us and generate an interest\n");
                    dow_prevent_sleep++;
                    LOG_DEBUG("dow: call _populate_data, %u pending interests\n",
                              (unsigned) dow_prevent_sleep);
                    _populate_data(pfx);
                }
                else {
                    LOG_DEBUG("- since we're DEPUTY, there's nothing to do\n");
                }
                free_prefix(pfx);
#else
                LOG_DEBUG("dow: nothing to do\n");
#endif
                break;
            default:
                LOG_WARNING("dow: I don't understand this message: %X\n", m.type);
        }
    }
    return NULL;
}

void dow_init(void)
{
    uint32_t tmp_id;
#ifdef CPU_NATIVE
        cpuid_get(&tmp_id);
#else
        uint8_t cpuid[CPUID_LEN];
        cpuid_get(cpuid);
        tmp_id = djb2_hash(cpuid, CPUID_LEN);
#endif
    if (dow_my_id == 0) {
        dow_my_id = tmp_id;
    }
    LOG_INFO("dow: my ID  is %08lX\n", (unsigned long) dow_my_id);

    random_init(dow_my_id);
    /* initialize to inactive state */
    LOG_INFO("\n\ndow: change to state INACTIVE\n\n");
    dow_state = DOW_STATE_INACTIVE;

    dow_pid = thread_create(_dow_stack, sizeof(_dow_stack),
                                THREAD_PRIORITY_MAIN-1, THREAD_CREATE_STACKTEST |
                                THREAD_CREATE_WOUT_YIELD, _loop, NULL, "caching manager");
}

static xtimer_t _sleep_timer = { .target = 0, .long_target = 0 };
static msg_t _sleep_msg = { .type = DOW_MSG_BACKTOSLEEP };
static void _radio_sleep(void)
{
#if DOW_INT_INT
    if (dow_prevent_sleep > 0) {
        if (ccnl_relay.pit == NULL) {
            LOG_DEBUG("dow: no PIT entries, reset pending counter\n");
            dow_prevent_sleep = 0;
        }
        else {
            LOG_DEBUG("dow: still waiting for an interest from the deputy, wait some time\n");
            xtimer_remove(&_sleep_timer);
            xtimer_set_msg(&_sleep_timer, DOW_KEEP_ALIVE_PERIOD, &_sleep_msg, dow_pid);
            return;
        }
    }
#endif
    if (!dow_sleeping) {
        dow_ts_sleep = xtimer_now();
        dow_time_active += (dow_ts_sleep - dow_ts_wakeup);
        LOG_DEBUG("dow: add to active time: %u\n", (unsigned) (dow_ts_sleep - dow_ts_wakeup));
        dow_sleeping = true;
    }
    netopt_state_t state;
    if (gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) > 0) {
        if (state == NETOPT_STATE_IDLE) {
            LOG_DEBUG("dow: sending radio to sleep\n");
            state = NETOPT_STATE_SLEEP;
            if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
                LOG_WARNING("dow: error sending radio to sleep\n");
            }
        }
        else if (state == NETOPT_STATE_SLEEP) {
            LOG_DEBUG("dow: radio sleeps already\n");
        }
        else {
            LOG_DEBUG("dow: radio is in wrong state (%x), try again later\n", state);
            xtimer_remove(&_sleep_timer);
            xtimer_set_msg(&_sleep_timer, DOW_KEEP_ALIVE_PERIOD, &_sleep_msg, dow_pid);
        }
    }
    else {
        LOG_WARNING("dow: error requesting radio state\n");
    }
}

void dow_sleep(uint8_t periods)
{
    LOG_DEBUG("dow: going to sleep\n");
    gnrc_netapi_set(ccnl_pid, NETOPT_CCN, CCNL_CTX_CLEAR_PIT_BUT_OWN, &ccnl_relay, sizeof(ccnl_relay));
    LOG_INFO("\n\ndow: change to state INACTIVE\n\n");
    dow_state = DOW_STATE_INACTIVE;

    _radio_sleep();

#if DOW_DEPUTY
    LOG_DEBUG("dow: wakeup in %u seconds (%i)\n", ((periods * DOW_Y)), (int) dow_pid);
    dow_period_counter = periods * DOW_Y;
    _sleep_period = dow_period_counter;
#else
    LOG_DEBUG("dow: wakeup in %u seconds (%i)\n", ((periods)), (int) dow_pid);
    dow_period_counter = periods;
#endif
    xtimer_remove(&dow_timer);
    dow_second_timer();
}

#if DOW_DEPUTY
void dow_takeover(void)
{
    _last_deputy_ts = xtimer_now();
    LOG_INFO("\n\ndow: change to state DEPUTY\n\n");
#if DOW_INT_INT
    ccnl_helper_clear_pit_for_own();
#endif
    dow_state = DOW_STATE_DEPUTY;
    dow_wakeup();
    unsigned cn = 0;
    while ((ccnl_helper_int(ccnl_helper_all_pfx, &cn, false) != CCNLRIOT_LAST_CN)
           && (cn <= CCNLRIOT_CACHE_SIZE)) {
        cn++;
    }
    LOG_DEBUG("dow: takeover completed\n");
}
#endif

#if (DOW_DEPUTY == 0) && (DOW_PER)
static void _send_interests(void)
{
    unsigned cn;
    struct ccnl_prefix_s *tmp;
    if (dow_is_registered) {
        tmp = ccnl_helper_my_pfx;
    }
    else {
        tmp = ccnl_helper_all_pfx;
    }
    for (cn = 0; (cn < DOW_PER) && (ccnl_helper_int(tmp, &cn, false) != CCNLRIOT_LAST_CN); cn++) {
        if (cn > CCNLRIOT_CACHE_SIZE) {
            LOG_WARNING("dow: asking too much! FAIL!\n");
        }
    }
}
#endif

void dow_wakeup(void)
{
    if (dow_sleeping) {
        dow_ts_wakeup = xtimer_now();
        dow_time_sleeping += (dow_ts_wakeup - dow_ts_sleep);
        LOG_DEBUG("dow: add to sleeping time: %u\n", (unsigned) (dow_ts_wakeup - dow_ts_sleep));
        dow_sleeping = false;
    }
    netopt_state_t state;
    if (gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) > 0) {
        if (state == NETOPT_STATE_SLEEP) {
            LOG_DEBUG("dow: waking up radio\n");
            state = NETOPT_STATE_IDLE;
            if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
                LOG_WARNING("dow: error waking up\n");
            }
        }
        else {
            LOG_DEBUG("dow: radio is already on\n");
        }
#if (DOW_DEPUTY == 0) && (DOW_PER)
        /* try to update the cache a bit */
        _send_interests();
#endif
    }
    else {
        LOG_WARNING("dow: error requesting radio state\n");
    }
}

void dow_new_data(void)
{
    /* XXX: save postponed value */
    if (dow_state == DOW_STATE_HANDOVER) {
        LOG_INFO("dow: currently in handover, let's postpone by a second\n");
        xtimer_set_msg(&dow_data_timer, SEC_IN_USEC, &dow_data_msg, dow_pid);
        return;
    }

    /* XXX: implement event */
    unsigned data = xtimer_now();

#if DOW_REBC_LAST
    bool _sent_old = false;
    if ((_old_data != 0) && (_send_old_data)) {
        data = _old_data;
        _sent_old = true;
        _send_old_data = false;
    }
    else {
        _old_data = data;
        _send_old_data = true;
    }
#endif
    /* each byte needs 2 characters to be represented as a hex value */
    /* string representation */
    char val[(sizeof(data) * 2) + 1];
    snprintf(val, sizeof(val) + 1, "%08X", data);

    /* get data into cache by sending to loopback */
    LOG_DEBUG("dow: put data into cache via loopback\n");
    size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 9 + 9;
    char pfx[prefix_len];
    if (dow_sensor_nr > 0) {
        uint8_t current_sensor = (random_uint32() % dow_sensor_nr);
        snprintf(pfx, prefix_len, "%s%s/%08lX/%s", CCNLRIOT_TYPE_PREFIX,
                 dow_sensors[current_sensor], (long unsigned) dow_my_id,
                 val);
    }
    else if (dow_is_registered) {
        snprintf(pfx, prefix_len, "%s%s/%08lX/%s", CCNLRIOT_TYPE_PREFIX,
                 dow_registered_prefix, (long unsigned) dow_my_id,
                 val);
    }
    else {
        snprintf(pfx, prefix_len, "%s%s/%08lX/%s", CCNLRIOT_SITE_PREFIX,
                 CCNLRIOT_TYPE_PREFIX, (long unsigned) dow_my_id, val);
    }
#if DOW_REBC_LAST
    if (!_sent_old) {
#endif
    printf("NEW DATA: %s\n", pfx);
#if DOW_REBC_LAST
    }
#endif
    /* schedule new data generation */
    uint32_t offset = DOW_EVENT_PERIOD_JITTER;
#ifdef DOW_REBC_LAST
    offset /= 2;
#endif
    LOG_DEBUG("dow: Next event in %" PRIu32 " seconds (%i)\n", (offset / 1000000), (int) dow_pid);
    xtimer_set_msg(&dow_data_timer, offset, &dow_data_msg, dow_pid);
    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, CCNL_SUITE_NDNTLV, NULL, 0);
    if (prefix == NULL) {
        LOG_ERROR("dow: We're doomed, WE ARE ALL DOOMED!\n");
    }
    else {
#if DOW_INT_INT
        ccnl_helper_create_cont(prefix, (unsigned char*) val, sizeof(val), true, false);
#else
        xtimer_usleep(500 * 1000);
        ccnl_helper_create_cont(prefix, (unsigned char*) val, sizeof(val), true, true);
#endif
        free_prefix(prefix);
    }
}

#if DOW_INT_INT
static void _populate_data(struct ccnl_prefix_s *pfx)
{
    /* first wake up radio (if necessary) */
    LOG_DEBUG("dow: entering _populate_data\n");
    dow_wakeup();
    /* populate the content now */
    ccnl_helper_int(pfx, NULL, true);
}
#endif
