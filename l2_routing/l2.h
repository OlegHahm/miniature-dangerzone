#ifndef L2_H
#define L2_H

#include "vtimer.h"
#include "net/ng_ieee802154.h"

extern kernel_pid_t *if_id;
extern timex_t interval;
extern vtimer_t vt;
extern eui64_t myId;
extern char l2addr_str[3 * 8];

void *_eventloop(void *arg);

#endif /* L2_H */
