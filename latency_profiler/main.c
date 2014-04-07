#include <stdio.h>

#include "hwtimer.h"
#include "board.h"
#include "cpu.h"
#include "gpioint.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#include "latency.h"

static void my_loop_delay(uint32_t delay)
{
    volatile uint32_t i;

    for (i = 1; i < delay; i++) {
            asm volatile(" nop ");
    }
}


void pin_toggle_isr(void)
{
    LED_GREEN_TOGGLE;
    DEBUG_PIN_OFF;
    //puts("TOGGLE");
}

void timer_cb(void *unused)
{
    LED_GREEN_TOGGLE;
}

int main(void)
{
    puts("Starting latency profiler");

    TRIGGER_PIN_INIT;

    TRIGGER_PIN_OFF;
    DEBUG_PIN_OFF;
#ifdef GPIOINT_TEST
    gpioint_set(0, BIT10, GPIOINT_RISING_EDGE, pin_toggle_isr);
    while (1) {
        my_loop_delay(320000);
        TRIGGER_PIN_TOGGLE;
    }
#endif
#ifdef BITARITHM_TEST
    my_loop_delay(256);
    DEBUG_PIN_ON;
    TRIGGER_PIN_ON;
    volatile unsigned v;
    for (unsigned i = 1; i < (1ul << 16) - 1; ++i) {
        v = number_of_lowest_bit(i);
    }
    (void) v;
    DEBUG_PIN_OFF;
    TRIGGER_PIN_OFF;
    while(1);
#endif
#ifdef HWTIMER_TEST
    while (1) {
        hwtimer_set(HWTIMER_TICKS(100), timer_cb, NULL);
        my_loop_delay(3200000);
    }
#endif
#ifdef HWTIMER_WAIT_TEST
    while (1) {
        TRIGGER_PIN_TOGGLE;
        hwtimer_wait(HWTIMER_TICKS(10000));
        DEBUG_PIN_OFF;
        my_loop_delay(3200000);
    }
#endif

    return 0;
}
