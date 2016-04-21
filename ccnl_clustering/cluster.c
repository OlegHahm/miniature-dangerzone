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
static xtimer_t _cluster_timer;
uint16_t cluster_my_id;
kernel_pid_t cluster_pid = KERNEL_PID_UNDEF;

/* prototypes */
static uint16_t _get_my_pos(void);

void *_loop(void *arg)
{
    (void) arg;
    xtimer_t data_timer;
    msg_t data_msg;
    data_msg.type = CLUSTER_MSG_NEWDATA;

    msg_init_queue(_mq, (sizeof(_mq) / sizeof(msg_t)));

    /* XXX: perform cluster ordering and compute my position */
    uint16_t my_position;

    my_position = _get_my_pos();

    ccnl_set_local_producer(ccnlriot_producer);

    /* start data generation timer */
    uint32_t offset = CLUSTER_EVENT_PERIOD;
    printf("cluster: Next event in %u seconds\n", (offset / 1000000));
    xtimer_set_msg(&data_timer, offset, &data_msg, cluster_pid);

    /* enter correct state and set timer if necessary */
    if (my_position == 0) {
        puts("cluster: I'm the first deputy");
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
                puts("cluster: received takeover msg");
                cluster_takeover();
                break;
            case CLUSTER_MSG_NEWDATA:
                puts("cluster: received newdata msg");
                /* XXX: implement event */
                unsigned data = random_uint32();
                /* XXX: send new data or put it into content store */
                /* each byte needs 2 characters to be represented as a hex value */
                if (cluster_state == CLUSTER_STATE_DEPUTY) {
                    size_t prefix_len = sizeof(CCNLRIOT_SITE_PREFIX) + sizeof(CCNLRIOT_TYPE_PREFIX) + 8;
                    char pfx[prefix_len];
                    char val[sizeof(data) * 2];
                    snprintf(pfx, prefix_len, "%s%s/%08X", CCNLRIOT_SITE_PREFIX, CCNLRIOT_TYPE_PREFIX, cluster_my_id);
                    snprintf(val, sizeof(val), "%08X\n", data);
                    struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(pfx, CCNL_SUITE_NDNTLV, NULL, 0);
                    ccnl_helper_create_cont(prefix, (unsigned char*) val, sizeof(val), true);
                }
                else {
                    ccnl_helper_int(NULL, (unsigned char*) &data, (sizeof(data) * 2));
                }
                /* schedule new data generation */
                offset = CLUSTER_EVENT_PERIOD;
                printf("Next event in %u seconds\n", (offset / 1000000));
                xtimer_set_msg(&data_timer, offset, &data_msg, cluster_pid);
                break;
            case CLUSTER_MSG_ALLDATA:
                puts("cluster: received alldata request");
                ccnl_helper_send_all_data();
                break;
            default:
                puts("cluster: I don't understand this message");
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
        puts("cluster: critical error, aborting");
        return -1;
    }
    return _get_pos(hwaddr, CCNLRIOT_ADDRLEN);
#endif
}

static msg_t _wakeup_msg;
void cluster_sleep(uint8_t periods)
{
    cluster_state = CLUSTER_STATE_INACTIVE;
    netopt_state_t state = NETOPT_STATE_SLEEP;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        puts("cluster: error going to sleep");
    }
    _wakeup_msg.type = CLUSTER_MSG_TAKEOVER;
    printf("cluster: wakeup in %u seconds\n", ((periods * CLUSTER_PERIOD) / SEC_IN_USEC));
    xtimer_set_msg(&_cluster_timer, periods * CLUSTER_PERIOD, &_wakeup_msg, cluster_pid);
}

void cluster_takeover(void)
{
    cluster_state = CLUSTER_STATE_TAKEOVER;
    cluster_wakeup();
    unsigned char all_pfx[] = CCNLRIOT_ALL_PREFIX;
    ccnl_helper_int(all_pfx, NULL, 0);
}

void cluster_handover(void)
{
}

void cluster_wakeup(void)
{
    netopt_state_t state = NETOPT_STATE_IDLE;
    if (gnrc_netapi_set(CCNLRIOT_NETIF, NETOPT_STATE, 0, &state, sizeof(netopt_state_t)) < 0) {
        puts("cluster: error waking up");
    }
}
