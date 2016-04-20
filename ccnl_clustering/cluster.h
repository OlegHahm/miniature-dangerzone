#ifndef CLUSTER_H
#define CLUSTER_H

#define PERIOD      (100)

typedef enum {
    CLUSTER_STATE_DEPUTY,
    CLUSTER_STATE_INACTIVE
} cluster_state_t;

cluster_state_t cluster_state;

#endif /* CLUSTER_H */
