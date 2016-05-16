#include <stdio.h>

#include "thread.h"
#include "msg.h"

#define MSG_TYPE1       (0x1701)
#define MSG_TYPE2       (0x4711)
#define MSG_TYPE_WRONG  (0x8888)

char _test_stack[THREAD_STACKSIZE_MAIN];

void *_test(void *unused)
{
    (void) unused;
    puts("second thread started");
    msg_t m, reply;
    reply.type = MSG_TYPE2;
    msg_receive(&m);
    msg_reply(&m, &reply);

    return NULL;
}

int main(void)
{
    puts("msg_snd_rcv application running");
    kernel_pid_t test_pid = thread_create(_test_stack, sizeof(_test_stack),
                                          THREAD_PRIORITY_MAIN + 1, 0, _test,
                                          NULL, "test");
    msg_t m, reply;
    m.type = MSG_TYPE1;
    reply.type = MSG_TYPE_WRONG;
    printf("call msg_send_receive() with type %X\n", (unsigned) MSG_TYPE1);
    msg_send_receive(&m, &reply, test_pid);
    printf("received type %X (should have been %X)\n", (unsigned) reply.type, (unsigned) MSG_TYPE2);
    return 0;
}
