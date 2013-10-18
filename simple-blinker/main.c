#include <stdio.h>

#include "hwtimer.h"

int main(void) {

    puts("foo");
    hwtimer_wait(1000);
    puts("bar");

    while (1);

    return 0;
}
