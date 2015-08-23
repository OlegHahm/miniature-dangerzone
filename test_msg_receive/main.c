#include <stdio.h>
#include "thread.h"

static char second_stack[THREAD_STACKSIZE_MAIN];

void *second(void *arg)
{
    (void) arg;
    msg_t m;
    puts("second thread starts and calls msg_receive()");
    msg_receive(&m);
    puts("Message received");
    return NULL;
}

int main(void)
{
    msg_t m;
    puts("test_msg_receive app");
    kernel_pid_t t2 = thread_create(second_stack, sizeof(second_stack),
                                    THREAD_PRIORITY_MAIN,
                                    CREATE_STACKTEST | CREATE_WOUT_YIELD,
                                    second, NULL, "second");

    puts("sending message to second thread");
    msg_send_receive(&m, &m, t2);
    puts("received message from seconed thread");
    return 0;
}
