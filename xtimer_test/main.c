/*
 * Copyright (C) 2015 Oliver Hahm <oleg@hobbykeller.org>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include "xtimer.h"
#include "thread.h"

static char dummy_stack[THREAD_STACKSIZE_MAIN];

#if MORE_TIMERS
    xtimer_t xts[MORE_TIMERS];
    int i = 0;
#endif

void *dummy(void *arg)
{
    (void) arg;
    xtimer_t xt;
    msg_t m;
    while (1) {
        xtimer_set_msg(&xt, 1000, &m, sched_active_pid);
#ifdef MORE_TIMERS
        if (i >= MORE_TIMERS) {
            i = 0;
        }
        xtimer_set_msg64(&(xts[i++]), 3600 * SEC_IN_USEC, &m, sched_active_pid);
#endif
        msg_receive(&m);
    }

    return NULL;
}

int main(void)
{
    thread_create(dummy_stack, sizeof(dummy_stack), THREAD_PRIORITY_MAIN + 1,
                  CREATE_STACKTEST, dummy, NULL, "dummy"); 

    uint32_t last_wakeup = xtimer_now();

    while(1) {
        xtimer_usleep_until(&last_wakeup, SEC_IN_USEC);
        printf("slept until %"PRIu32"\n", xtimer_now());
    }

    return 0;
}
