#include <stdio.h>
#include <sys/time.h>
#include "xtimer.h"

int main(void)
{
    int r;
    struct timeval tp;
    for (int i = 0; i < 100; i++) {
        r = gettimeofday(&tp, NULL);
        printf("s: %li, us: %li (%i)\n", tp.tv_sec, tp.tv_usec, r);
        xtimer_sleep(1);
    }
    return 0;
}
