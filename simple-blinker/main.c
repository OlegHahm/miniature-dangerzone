#include <stdio.h>

#include "hwtimer.h"
#include "thread.h"
#include "ps.h"
#include "board.h"

char mystack[KERNEL_CONF_STACKSIZE_PRINTF];

void mythread(void) {
    puts("go!");
    thread_sleep();
    puts("second thread: back in business");
    puts("set timer in thread 2");
    hwtimer_wait(1000);
    puts("Going to sleep again");
    thread_sleep();
}

int main(void) {
    int i, pid;

    pid = thread_create(mystack, KERNEL_CONF_STACKSIZE_PRINTF, PRIORITY_MAIN+1, CREATE_STACKTEST, mythread, "second");
    if (pid < 0) {
        puts("error creating thread");
    }
    else { 
        puts("foo");
        for (i = 0; i < 10; i++) {
            puts("set timer in thread 1");
            hwtimer_wait(10 * 1000);
            if (i == 1) {
                thread_wakeup(pid);
            }
        }
        puts("bar");
        thread_print_all();
        thread_sleep();
    }

    while (1);

    return 0;
}
