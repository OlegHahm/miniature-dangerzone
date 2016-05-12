#ifndef CLUSTER_H
#define CLUSTER_H

#include "random.h"
#include "timex.h"
#include "bloom.h"

#define CLUSTER_DEPUTY              (0)

#define CLUSTER_D                   (3)
#define CLUSTER_X                   (2)
#define CLUSTER_Y                   (2)
#define CLUSTER_P                   (0.6)

#define CLUSTER_PERIOD              (23)
//#define CLUSTER_EVENT_PERIOD      (random_uint32() & 0x00FFFFFF)
#define CLUSTER_EVENT_PERIOD        (CLUSTER_D * SEC_IN_USEC) + (random_uint32() & 0x000FFFFF)
//#define CLUSTER_EVENT_PERIOD      (4 * SEC_IN_USEC)

#define CLUSTER_GO_SLEEP            (random_uint32() > (UINT32_MAX * CLUSTER_P))

#define CLUSTER_STAY_AWAKE_PERIOD   (100 * MS_IN_USEC)
/* random interval between 50ms and ~1s */
#define CLUSTER_BEACONING_PERIOD    ((random_uint32() & 0x000FFFFF) + 50000)
#define CLUSTER_BEACONING_COUNT     (1)
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

typedef enum {
    CLUSTER_STATE_DEPUTY,
    CLUSTER_STATE_HANDOVER,
    CLUSTER_STATE_INACTIVE,
    CLUSTER_STATE_TAKEOVER
} cluster_state_t;

typedef struct __attribute__((packed)) {
    char value[CLUSTER_CONT_LEN + 1];
    int num;
} cluster_content_t;

extern cluster_state_t cluster_state;
extern uint16_t cluster_my_id;
extern kernel_pid_t ccnl_pid;
extern kernel_pid_t cluster_pid;
extern uint16_t cluster_position;
extern bloom_t cluster_neighbors;
extern uint16_t cluster_size;
extern uint8_t cluster_prevent_sleep;
extern xtimer_t cluster_data_timer;
extern msg_t cluster_data_msg;

void cluster_init(void);
void cluster_takeover(void);
void cluster_wakeup(void);
void cluster_sleep(uint8_t periods);
void cluster_new_data(void);

void beaconing_start(void);
void beaconing_send(void);

#endif /* CLUSTER_H */
