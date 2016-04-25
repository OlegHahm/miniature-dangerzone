#include "msg.h"
#include "xtimer.h"
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

/* internal variables */
static xtimer_t _cluster_timer, _sleep_timer;
uint16_t cluster_my_id;
kernel_pid_t cluster_pid = KERNEL_PID_UNDEF;

/* prototypes */
static uint16_t _get_my_pos(void);
static void _send_int(char *val, size_t len);

void *_loop(void *arg)
{
    (void) arg;
    xtimer_t data_timer;
    _cluster_timer.target = _cluster_timer.long_target = data_timer.target =
        data_timer.long_target = _sleep_timer.target = _sleep_timer.long_target = 0;

    msg_t data_msg;
    data_msg.type = CLUSTER_MSG_NEWDATA;

    msg_init_queue(_mq, (sizeof(_mq) / sizeof(msg_t)));

    /* XXX: perform cluster ordering and compute my position */
    uint16_t my_position;

    my_position = _get_my_pos();

    ccnl_set_local_producer(ccnlriot_producer);
    ccnl_set_local_consumer(ccnlriot_consumer);

    /* start data generation timer */
    uint32_t offset = CLUSTER_EVENT_PERIOD;
    LOG_DEBUG("cluster: Next event in %u seconds\n", (offset / 1000000));
    xtimer_set_msg(&data_timer, offset, &data_msg, cluster_pid);

    /* enter correct state and set timer if necessary */
    if (my_position == 0) {
        LOG_INFO("cluster: I'm the first deputy\n");
        /* become deputy now */
        cluster_state = CLUSTER_STATE_DEPUTY;
    }
    else {
        /* go to sleep and set timer */
        cluster_state = CLUSTER_STATE_INACTIVE;
        cluster_sleep(my_position);
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
                /* XXX: implement event */
                unsigned data = xtimer_now();
                /* each byte needs 2 characters to be represented as a hex value */
                /* string representation */
                char val[sizeof(data) * 2];
                snprintf(val, sizeof(val) + 1, "%08X\n", data);

                if ((cluster_state == CLUSTER_STATE_DEPUTY) || (cluster_state == CLUSTER_STATE_TAKEOVER)) {
                    /* for the deputy we put the content directly into the store */
                    size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 9;
                    char pfx[prefix_len];
                    snprintf(pfx, prefix_len, "%s%s/%08X", CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, cluster_my_id);
                    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, CCNL_SUITE_NDNTLV, NULL, 0);
                    ccnl_helper_create_cont(prefix, (unsigned char*) val, sizeof(val), true);
                    free_prefix(prefix);
                }
                else {
                    _send_int(val, (sizeof(data) * 2) + 1);
                }
                /* schedule new data generation */
                offset = CLUSTER_EVENT_PERIOD;
                LOG_DEBUG("Next event in %u seconds\n", (offset / 1000000));
                xtimer_set_msg(&data_timer, offset, &data_msg, cluster_pid);
                break;
            case CLUSTER_MSG_ALLDATA:
                LOG_DEBUG("cluster: received alldata request\n");
                ccnl_helper_send_all_data();
                break;
            case CLUSTER_MSG_BACKTOSLEEP:
                LOG_DEBUG("cluster: received backtosleep request\n");
                netopt_state_t state = NETOPT_STATE_SLEEP;
                if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
                    LOG_WARNING("cluster: error going to sleep\n");
                }
                break;
            default:
                LOG_WARNING("cluster: I don't understand this message\n");
        }
    }
    return NULL;
}

void cluster_init(void)
{
    cpuid_get(&cluster_my_id);
    /* initialize to inactive state */
    cluster_state = CLUSTER_STATE_INACTIVE;

    cluster_pid = thread_create(_cluster_stack, sizeof(_cluster_stack),
                         THREAD_PRIORITY_MAIN-1, THREAD_CREATE_STACKTEST |
                         THREAD_CREATE_WOUT_YIELD, _loop, NULL, "cluster manager");
}

static inline int _addr_cmp(uint8_t *a, uint8_t *b, size_t addr_len)
{
    for (unsigned int i = 0; i < addr_len; i++) {
        if (a[i] != b[i]) {
            return 1;
        }
    }
    return 0;
}


static inline uint16_t _get_pos(uint8_t *addr, size_t addr_len)
{
    for (uint16_t i = 0; i < CCNLRIOT_NUMBER_OF_NODES; i++) {
        if (_addr_cmp(addr, ccnlriot_id[i], addr_len) == 0) {
            return i;
        }
    }
    return -1;
}

static uint16_t _get_my_pos(void)
{
    /* XXX: configure this statically for now */
#ifdef CPU_NATIVE
    return cluster_my_id;
#else
    uint8_t hwaddr[CCNLRIOT_ADDRLEN];
#if USE_LONG && !defined(BOARD_NATIVE)
    int res = gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_ADDRESS_LONG, 0, hwaddr, sizeof(hwaddr));
#else
    int res = gnrc_netapi_get(CCNLRIOT_NETIF, NETOPT_ADDRESS, 0, hwaddr, sizeof(hwaddr));
#endif
    if (res < 0) {
        LOG_WARNING("cluster: critical error, aborting\n");
        return -1;
    }
    return _get_pos(hwaddr, CCNLRIOT_ADDRLEN);
#endif
}

static msg_t _wakeup_msg;
void cluster_sleep(uint8_t periods)
{
    LOG_INFO("cluster: going to sleep\n");
    cluster_state = CLUSTER_STATE_INACTIVE;
    netopt_state_t state = NETOPT_STATE_SLEEP;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        LOG_WARNING("cluster: error going to sleep\n");
    }
    _wakeup_msg.type = CLUSTER_MSG_TAKEOVER;
    LOG_DEBUG("cluster: wakeup in %u seconds\n", ((periods * CLUSTER_PERIOD) / SEC_IN_USEC));
    xtimer_set_msg(&_cluster_timer, periods * CLUSTER_PERIOD, &_wakeup_msg, cluster_pid);
}

void cluster_takeover(void)
{
    cluster_state = CLUSTER_STATE_TAKEOVER;
    cluster_wakeup();
    unsigned char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    ccnl_helper_int(all_pfx, NULL, 0);
}

void cluster_wakeup(void)
{
    netopt_state_t state = NETOPT_STATE_IDLE;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        LOG_WARNING("cluster: error waking up\n");
    }
}

static msg_t _sleep_msg;
static void _send_int(char *val, size_t len)
{
    netopt_state_t state = NETOPT_STATE_IDLE;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        LOG_WARNING("cluster: error waking up\n");
    }
    ccnl_helper_int(NULL, (unsigned char*) val, len);
    _sleep_msg.type = CLUSTER_MSG_BACKTOSLEEP;
    LOG_DEBUG("cluster: going back to sleep in %u microseconds\n", CLUSTER_STAY_AWAKE_PERIOD);
    xtimer_set_msg(&_sleep_timer, CLUSTER_STAY_AWAKE_PERIOD, &_sleep_msg, cluster_pid);
}
