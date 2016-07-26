#ifndef DOW_H
#define DOW_H

#include "random.h"
#include "timex.h"
#include "bloom.h"
#include "ccnlriot.h"

/**
 * @brief configuration parameters
 * @{
 */

/** enable round-robin deputy mode */
#define DOW_DEPUTY              (0)

/** start automatically (instead of requiring a shell command */
#define DOW_AUTOSTART           (0)

/** mostly to test caching strategies via content generation via shell command */
#ifndef DOW_DEBUG
#   define DOW_DEBUG               (0)
#endif

/** Data generation interval D */
#define DOW_D                   (50)

/**
 * @brief values for non-round robin mode
 * @{ */
/** multiplier for D for sleep-/awake-cycles in non round-robin mode */
#define DOW_X                   (.02)
/** sleep probability */
#define DOW_P                   (0.95)
/** @} */

/**
 * @brief values round-robin mode
 * @{
 */
/** interval length for one deputy shift */
#define DOW_Y                   (24)
/** @} */

/**
 * @brief Caching strategy
 * @{ */
/** caching probability for non-prioritized content */
#define DOW_Q                   (.5)

/** replacement strategy:
 * 0 means LRU, 1 means "our" strategy (oldest representative)
 */
#define DOW_CACHE_REPL_STRAT          (1)

/** autoconfiguration of prioritized content based on the first names that
 * arrive - this variable sets the threshold for this */
#if DOW_CACHE_REPL_STRAT
//#   define DOW_PRIO_CACHE       (CCNLRIOT_CACHE_SIZE / 2)
#   define DOW_PRIO_CACHE       (0)
#else
#   define DOW_PRIO_CACHE       (0)
#endif
/** @} */

/** use interest-interest for dissemination of unsolicited content */
#define DOW_INT_INT             (0)

/** how many interests a node should send after waking up (either for * or its
 * preferred prefix */
#define DOW_PER                 (0)

/** adaptively choose prioritized prefix */
#define DOW_APMDMR              (0)

#if !DOW_PER
#undef DOW_APMDMR
#define DOW_APMDMR              (0)
#endif

#if DOW_APMDMR
#   define DOW_FIRST_PHASE      (3)
#   define DOW_APMDMR_MIN_INT   (3)
#endif

/** how many chunks for oldest content a node should sent after detecting that
 * its cache has been completely refreshed */
#define DOW_PSR                 (0)

/** if node is about to respond to an interest for * or its prefix, it will
 * select a random chunk from its cache for the response if this is set to 1*/
#define DOW_RAND_SEEDING        (0)

/** a node stays active if a certain threshold of interests for its prefix was
 * not observed during last cycle */
#if DOW_PER
#   define DOW_KEEP_ALIVE_PFX       (0)
#   define DOW_KEEP_ALIVE_MIN_THR   (0)
#else
#   define DOW_KEEP_ALIVE_PFX       (0)
#endif

/** the maximum number of sensors a node can have */
#define DOW_SENSOR_MAX_NR           (5)

/** maximum number of prefixes */
#define DOW_MAX_PREFIXES            (10)

/** how often broadcasts of unsolicited are repeated */
#ifdef CPU_NATIVE
#   define DOW_BC_COUNT                (1)
#else
#   define DOW_BC_COUNT                (3)
#endif

/**
 * @}
 */

/**
 * @brief values derived from configuration settings
 * @{
 */
#define DOW_EVENT_PERIOD        (DOW_D * SEC_IN_USEC)
//#define DOW_EVENT_PERIOD_JITTER (DOW_D * SEC_IN_USEC) + (random_uint32() & 0x000FFFFF)
#define DOW_EVENT_PERIOD_JITTER (random_uint32() & 0x5f5e100)

#define DOW_GO_SLEEP            (random_uint32() < (UINT32_MAX * DOW_P))

#define DOW_DO_CACHE            (random_uint32() < (UINT32_MAX * DOW_Q))

#define DOW_RANDOM_CHUNK        (random_uint32() % ccnl_relay.contentcnt)

#define DOW_KEEP_ALIVE_PERIOD   (100 * MS_IN_USEC)

//#define DOW_STARTUP_DELAY       (DOW_D * (random_uint32() & 0x000001FF) * MS_IN_USEC)
#define DOW_STARTUP_DELAY       (0)
/**
 * @}
 */

/**
 * @brief beaconing parameters
 * @{
 */
/* random interval between 50ms and ~1s */
#define DOW_BEACONING_PERIOD    ((random_uint32() & 0x000FFFFF) + 50000)
#define DOW_BEACONING_COUNT     (3)
#define DOW_BEACONING_WAIT      ((DOW_BEACONING_COUNT + 1) * SEC_IN_USEC)
/** @} */

#define DOW_CONT_LEN            (8)

/**
 * @brief state machine's message types
 * @{
 */
#define DOW_MSG_SECOND          (0x4443)
#define DOW_MSG_TAKEOVER        (0x4444)
#define DOW_MSG_NEWDATA         (0x4445)
#define DOW_MSG_INACTIVE        (0x4446)
#define DOW_MSG_BACKTOSLEEP     (0x4447)
#define DOW_MSG_BEACON          (0x4448)
#define DOW_MSG_BEACON_END      (0x4449)
#define DOW_MSG_RECEIVED        (0x4450)
#define DOW_MSG_RECEIVED_ACK    (0x4451)
#define DOW_MSG_RECEIVED_INT    (0x4452)
/** @} */

#define DOW_MSG_QUEUE_SIZE      (64)

/** current state */
typedef enum {
    DOW_STATE_DEPUTY,
    DOW_STATE_HANDOVER,
    DOW_STATE_INACTIVE,
} dow_state_t;

/** data type for the used contend */
typedef struct __attribute__((packed)) {
    char value[DOW_CONT_LEN + 1];
    int num;
} dow_content_t;

typedef struct {
    char pfx;
    unsigned count;
} dow_pfx_presence_t;

extern dow_pfx_presence_t dow_pfx_pres[DOW_MAX_PREFIXES];

/** set if registered for a certain prefix
 * 3 bytes = / + PREFIX + \0 */
extern char dow_registered_prefix[3];
extern bool dow_is_registered;

extern char dow_sensors[DOW_SENSOR_MAX_NR][3];
extern uint8_t dow_sensor_nr;

extern uint8_t dow_my_prefix_interest_count;

/* state */
extern dow_state_t dow_state;

/* id and PIDs */
extern uint32_t dow_my_id;
extern kernel_pid_t ccnl_pid;
extern kernel_pid_t dow_pid;
extern bool dow_manual_id;
extern uint16_t dow_num_src;

/* beaconing information */
extern uint16_t dow_position;
extern bloom_t dow_neighbors;
extern uint16_t dow_size;

/* state control */
extern uint8_t dow_prevent_sleep;
extern xtimer_t dow_data_timer;
extern msg_t dow_data_msg;
extern uint32_t dow_period_counter;
extern xtimer_t dow_timer;
extern msg_t dow_wakeup_msg;
extern uint8_t dow_prio_cache_cnt;

/* time measurement */
extern uint32_t dow_time_sleeping;
extern uint32_t dow_time_active;
extern bool dow_sleeping;
extern uint32_t dow_ts_wakeup;
extern uint32_t dow_ts_sleep;

void dow_init(void);
void dow_takeover(void);
void dow_wakeup(void);
void dow_sleep(uint8_t periods);
void dow_new_data(void);

void beaconing_start(void);
void beaconing_send(void);

#endif /* DOW_H */
