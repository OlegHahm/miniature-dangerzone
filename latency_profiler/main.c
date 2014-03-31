#include <stdio.h>

#include "hwtimer.h"
#include "board.h"
#include "cpu.h"
#include "gpioint.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "latency.h"

static my_loop_delay(uint32_t delay)
{
    volatile uint32_t i, j;

    for (i = 1; i < delay; i++) {
            asm volatile(" nop ");
    }
}


void pin_toggle_isr(void)
{
    LED_GREEN_TOGGLE;
    //puts("TOGGLE");
}

void timer_cb(void *unused)
{
    DEBUG_PIN_TOGGLE;
    LED_RED_TOGGLE;
}

int main(void)
{
    puts("Starting latency profiler");

    INIT_TRIGGER_PIN;

    TRIGGER_PIN_OFF;
    DEBUG_PIN_OFF;
    /*
   
    TOGGLE_TRIGGER_PIN;
    for (int i = 0; i < 10; i++) {
        DEBUG_PIN_TOGGLE;
    }
    TOGGLE_TRIGGER_PIN;
*/
    gpioint_set(0, BIT10, GPIOINT_RISING_EDGE, pin_toggle_isr);
    while (1) {
        TOGGLE_TRIGGER_PIN;
        DEBUG_PIN_TOGGLE;
        hwtimer_set(HWTIMER_TICKS(10), timer_cb, NULL);
        my_loop_delay(320000);
    }

    return 0;
}
