#include "thread.h"
#include "msg.h"
#include "xtimer.h"

static char _stack1[THREAD_STACKSIZE_MAIN];
static char _stack2[THREAD_STACKSIZE_MAIN];

static msg_t _mq1[8];
static msg_t _mq2[8];

static xtimer_t _xt1 = { .target = 0, .long_target = 0 };
static xtimer_t _xt2 = { .target = 0, .long_target = 0 };

static msg_t _m1;
static msg_t _m2;

void *_t1(void *unused)
{
    (void) unused;
    puts("T1 starting");
    msg_init_queue(_mq1, 8);
    while (1) {
        xtimer_remove(&_xt1);
        xtimer_set_msg(&_xt1, SEC_IN_USEC, &_m1, sched_active_pid);

        msg_t m;
        msg_receive(&m);
        puts("T1 msg received");
    }
    return NULL;
}

void *_t2(void *unused)
{
    puts("T2 starting");
    (void) unused;
    msg_init_queue(_mq2, 8);
    while (1) {
        xtimer_remove(&_xt2);
        xtimer_set_msg(&_xt2, SEC_IN_USEC, &_m2, sched_active_pid);

        msg_t m;
        msg_receive(&m);
        puts("T2 msg received");
    }
    return NULL;
}

void periodic_start(void)
{
    thread_create(_stack1, sizeof(_stack1), THREAD_PRIORITY_MAIN - 2, THREAD_CREATE_WOUT_YIELD, _t1, NULL, "t1");
    thread_create(_stack2, sizeof(_stack2), THREAD_PRIORITY_MAIN - 3, THREAD_CREATE_WOUT_YIELD, _t2, NULL, "t2");
    thread_yield_higher();
}
