#include "thread.h"
#include "xtimer.h"
#include "random.h"

char _dg_stack[THREAD_STACKSIZE_MAIN];

void *_loop(void *arg)
{
    (void) arg;
    while (1) {
        xtimer_usleep((random_uint32() & 0x0FFFFFFF));
        puts("generate event");
        /* XXX: implement event */
        /* XXX: send new data or put it into content store */
    }
    return NULL;
}

void data_generator_init(void)
{
    thread_create(_dg_stack, sizeof(_dg_stack), THREAD_PRIORITY_MAIN-1,
                  THREAD_CREATE_STACKTEST, _loop, NULL, "data generator");
}
