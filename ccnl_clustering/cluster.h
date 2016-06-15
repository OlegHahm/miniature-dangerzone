#ifndef CLUSTER_H
#define CLUSTER_H

#include "random.h"
#include "timex.h"
#include "bloom.h"
#include "ccnlriot.h"

#define CLUSTER_DEPUTY              (0)
#define CLUSTER_AUTOSTART           (0)
#ifndef CLUSTER_DEBUG
#   define CLUSTER_DEBUG               (0)
#endif

#define CLUSTER_D                   (3)
#define CLUSTER_X                   (2)
#define CLUSTER_P                   (0.95)
#define CLUSTER_PERIOD              (17)

#define CLUSTER_CACHE_PROB          (.5)
/* 0 means LRU, 1 means "our" strategy */
#define CLUSTER_CACHE_RM_STRATEGY   (1)

#if CLUSTER_CACHE_RM_STRATEGY
//#   define CLUSTER_PRIO_CACHE       (CCNLRIOT_CACHE_SIZE / 3)
#   define CLUSTER_PRIO_CACHE       (0)
#else
#   define CLUSTER_PRIO_CACHE       (0)
#endif

#define CLUSTER_SENSOR_MAX_NR       (5)

#define CLUSTER_EVENT_PERIOD        (CLUSTER_D * SEC_IN_USEC)
#define CLUSTER_EVENT_PERIOD_JITTER (CLUSTER_D * SEC_IN_USEC) + (random_uint32() & 0x000FFFFF)

#define CLUSTER_GO_SLEEP            (random_uint32() < (UINT32_MAX * CLUSTER_P))

#define CLUSTER_DO_CACHE            (random_uint32() < (UINT32_MAX * CLUSTER_CACHE_PROB))

#define CLUSTER_STAY_AWAKE_PERIOD   (100 * MS_IN_USEC)

#define CLUSTER_STARTUP_DELAY       (CLUSTER_D * (random_uint32() & 0x000001FF) * SEC_IN_USEC)

/* random interval between 50ms and ~1s */
#define CLUSTER_BEACONING_PERIOD    ((random_uint32() & 0x000FFFFF) + 50000)
#define CLUSTER_BEACONING_COUNT     (3)
#define CLUSTER_BEACONING_WAIT      ((CLUSTER_BEACONING_COUNT + 1) * SEC_IN_USEC)

#define CLUSTER_CONT_LEN            (8)

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

typedef enum {
    CLUSTER_STATE_DEPUTY,
    CLUSTER_STATE_HANDOVER,
    CLUSTER_STATE_INACTIVE,
} cluster_state_t;

typedef struct __attribute__((packed)) {
    char value[CLUSTER_CONT_LEN + 1];
    int num;
} cluster_content_t;

/* set if registered for a certain prefix
 * 3 bytes = / + PREFIX + \0 */
extern char cluster_registered_prefix[3];
extern bool cluster_is_registered;

extern char cluster_sensors[CLUSTER_SENSOR_MAX_NR][3];
extern uint8_t cluster_sensor_nr;

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
