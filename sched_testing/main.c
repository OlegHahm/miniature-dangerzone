#include <stdio.h>
#include "thread.h"

char snd_thread_stack[KERNEL_CONF_STACKSIZE_MAIN];

void *snd_thread(void *unused)
{
    (void) unused;
    puts("snd_thread running, going to sleep");
    thread_sleep();
    puts("snd_thread awake again");
    return NULL;
}

int main(void)
{
    puts("Starting Sched testing");
    kernel_pid_t snd_pid = thread_create(snd_thread_stack,
                                         sizeof(snd_thread_stack),
                                         PRIORITY_MAIN, 0,
                                         snd_thread, NULL, "snd");

    puts("yield");
    thread_yield();
    puts("wake up second thread");
    thread_wakeup(snd_pid);
    puts("terminating main thread");

    return 0;
}
