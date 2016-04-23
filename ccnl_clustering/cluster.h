#ifndef CLUSTER_H
#define CLUSTER_H

#include "random.h"
#include "timex.h"

#define CLUSTER_PERIOD          (22 * SEC_IN_USEC)
//#define CLUSTER_EVENT_PERIOD    (random_uint32() & 0x00FFFFFF)
#define CLUSTER_EVENT_PERIOD     (10 * SEC_IN_USEC)
#define CLUSTER_STAY_AWAKE_PERIOD   (10 * MS_IN_USEC)

#define CLUSTER_SIZE            (2)

#define CLUSTER_MSG_TAKEOVER    (0x4444)
#define CLUSTER_MSG_NEWDATA     (0x4445)
#define CLUSTER_MSG_ALLDATA     (0x4446)
#define CLUSTER_MSG_BACKTOSLEEP (0x4447)

typedef enum {
    CLUSTER_STATE_DEPUTY,
    CLUSTER_STATE_INACTIVE,
    CLUSTER_STATE_TAKEOVER
} cluster_state_t;

extern cluster_state_t cluster_state;
extern uint16_t cluster_my_id;
extern kernel_pid_t cluster_pid;

void cluster_init(void);
void cluster_takeover(void);
void cluster_wakeup(void);
void cluster_sleep(uint8_t periods);

#endif /* CLUSTER_H */
