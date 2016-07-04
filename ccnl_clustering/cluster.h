#ifndef CLUSTER_H
#define CLUSTER_H

#include "random.h"
#include "timex.h"
#include "bloom.h"
#include "ccnlriot.h"

/**
 * @brief configuration parameters
 * @{
 */

/** enable round-robin deputy mode */
#define CLUSTER_DEPUTY              (0)

/** start automatically (instead of requiring a shell command */
#define CLUSTER_AUTOSTART           (0)

/** mostly to test caching strategies via content generation via shell command */
#ifndef CLUSTER_DEBUG
#   define CLUSTER_DEBUG               (0)
#endif

/** Data generation interval D */
#define CLUSTER_D                   (3)

/**
 * @brief values for non-round robin mode
 * @{ */
/** multiplier for D for sleep-/awake-cycles in non round-robin mode */
#define CLUSTER_X                   (4)
/** sleep probability */
#define CLUSTER_P                   (0.95)
/** @} */

/**
 * @brief values round-robin mode
 * @{
 */
/** interval length for one deputy shift */
#define CLUSTER_PERIOD              (17)
/** @} */

/**
 * @brief Caching strategy
 * @{ */
/** caching probability for non-prioritized content */
#define CLUSTER_CACHE_PROB          (.5)

/** replacement strategy:
 * 0 means LRU, 1 means "our" strategy (oldest representative)
 */
#define CLUSTER_CACHE_RM_STRATEGY   (1)

/** autoconfiguration of prioritized content based on the first names that
 * arrive - this variable sets the threshold for this */
#if CLUSTER_CACHE_RM_STRATEGY
//#   define CLUSTER_PRIO_CACHE       (CCNLRIOT_CACHE_SIZE / 2)
#   define CLUSTER_PRIO_CACHE       (0)
#else
#   define CLUSTER_PRIO_CACHE       (0)
#endif
/** @} */

/** use interest-interest for dissemination of unsolicited content */
#define CLUSTER_INT_INT             (0)

/** how many interests a node should send after waking up (either for * or its
 * preferred prefix */
#define CLUSTER_UPDATE_INTERESTS    (5)
/** how many chunks for oldest content a node should sent after detecting that
 * its cache has been completely refreshed */
#define CLUSTER_PUBLISH_OLD         (3)

/** if node is about to respond to an interest for * or its prefix, it will
 * select a random chunk from its cache for the response if this is set to 1*/
#define CLUSTER_RAND_UPDATES        (0)

/** a node stays active if a certain threshold of interests for its prefix was
 * not observed during last cycle */
#if CLUSTER_UPDATE_INTERESTS
#   define CLUSTER_STAY_AWAKE_PFX       (1)
#   define CLUSTER_STAY_AWAKE_MIN_THR   (2)
#else
#   define CLUSTER_STAY_AWAKE_PFX   (0)
#endif

/** the maximum number of sensors a node can have */
#define CLUSTER_SENSOR_MAX_NR       (5)

/**
 * @}
 */

/**
 * @brief values derived from configuration settings
 * @{
 */
#define CLUSTER_EVENT_PERIOD        (CLUSTER_D * SEC_IN_USEC)
#define CLUSTER_EVENT_PERIOD_JITTER (CLUSTER_D * SEC_IN_USEC) + (random_uint32() & 0x000FFFFF)

#define CLUSTER_GO_SLEEP            (random_uint32() < (UINT32_MAX * CLUSTER_P))

#define CLUSTER_DO_CACHE            (random_uint32() < (UINT32_MAX * CLUSTER_CACHE_PROB))

#define CLUSTER_RANDOM_CHUNK        (random_uint32() % ccnl_relay.contentcnt)

#define CLUSTER_STAY_AWAKE_PERIOD   (100 * MS_IN_USEC)

#define CLUSTER_STARTUP_DELAY       (CLUSTER_D * (random_uint32() & 0x000001FF) * MS_IN_USEC)
/**
 * @}
 */

/**
 * @brief beaconing parameters
 * @{
 */
/* random interval between 50ms and ~1s */
#define CLUSTER_BEACONING_PERIOD    ((random_uint32() & 0x000FFFFF) + 50000)
#define CLUSTER_BEACONING_COUNT     (3)
#define CLUSTER_BEACONING_WAIT      ((CLUSTER_BEACONING_COUNT + 1) * SEC_IN_USEC)
/** @} */

#define CLUSTER_CONT_LEN            (8)

/**
 * @brief state machine's message types
 * @{
 */
#define CLUSTER_MSG_SECOND          (0x4443)
#define CLUSTER_MSG_TAKEOVER        (0x4444)
#define CLUSTER_MSG_NEWDATA         (0x4445)
#define CLUSTER_MSG_INACTIVE        (0x4446)
#define CLUSTER_MSG_BACKTOSLEEP     (0x4447)
#define CLUSTER_MSG_BEACON          (0x4448)
#define CLUSTER_MSG_BEACON_END      (0x4449)
#define CLUSTER_MSG_RECEIVED        (0x4450)
#define CLUSTER_MSG_RECEIVED_ACK    (0x4451)
#define CLUSTER_MSG_RECEIVED_INT    (0x4452)
/** @} */

#define CLUSTER_MSG_QUEUE_SIZE      (64)

/** current state */
typedef enum {
    CLUSTER_STATE_DEPUTY,
    CLUSTER_STATE_HANDOVER,
    CLUSTER_STATE_INACTIVE,
} cluster_state_t;

/** data type for the used contend */
typedef struct __attribute__((packed)) {
    char value[CLUSTER_CONT_LEN + 1];
    int num;
} cluster_content_t;

/** set if registered for a certain prefix
 * 3 bytes = / + PREFIX + \0 */
extern char cluster_registered_prefix[3];
extern bool cluster_is_registered;

extern char cluster_sensors[CLUSTER_SENSOR_MAX_NR][3];
extern uint8_t cluster_sensor_nr;

extern uint8_t cluster_my_prefix_interest_count;

/* state */
extern cluster_state_t cluster_state;

/* id and PIDs */
extern uint32_t cluster_my_id;
extern kernel_pid_t ccnl_pid;
extern kernel_pid_t cluster_pid;

/* beaconing information */
extern uint16_t cluster_position;
extern bloom_t cluster_neighbors;
extern uint16_t cluster_size;

/* state control */
extern uint8_t cluster_prevent_sleep;
extern xtimer_t cluster_data_timer;
extern msg_t cluster_data_msg;
extern uint32_t cluster_period_counter;
extern xtimer_t cluster_timer;
extern msg_t cluster_wakeup_msg;
extern uint8_t cluster_prio_cache_cnt;

/* time measurement */
extern uint32_t cluster_time_sleeping;
extern uint32_t cluster_time_active;
extern bool cluster_sleeping;
extern uint32_t cluster_ts_wakeup;
extern uint32_t cluster_ts_sleep;

void cluster_init(void);
void cluster_takeover(void);
void cluster_wakeup(void);
void cluster_sleep(uint8_t periods);
void cluster_new_data(void);

void beaconing_start(void);
void beaconing_send(void);

#endif /* CLUSTER_H */
