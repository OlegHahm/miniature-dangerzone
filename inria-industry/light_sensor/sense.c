/*
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     cebit_sensor
 * @{
 *
 * @file        sense.c
 * @brief       CeBIT 2014 demo application - sensor node
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <math.h>

#include "thread.h"
#include "isl29020.h"
#include "vtimer.h"
#include "kernel.h"
#include "board.h"

#include "sense.h"
#include "evt_handler.h"


#define THREAD_PRIO         (10U)

#define LUX_THRESHOLD      (230)
#define SAMPLING_PERIOD     (50000U)

#define STATE_NORMAL        (0U)
#define STATE_DOWN1         (2U)
#define STATE_DOWN2         (1U)

static char _stack[KERNEL_CONF_STACKSIZE_MAIN];
static int sensepid;


static int16_t acc_data[6];
static int state = -1;


void *sensethread(void *unused);
void check_state(int lux);

isl29020_t _isl29020_dev;

extern void _transceiver_send_handler(char *pkt);


void *sensethread(void *unused)
{
    (void) unused;

    timex_t delay = timex_set(0, SAMPLING_PERIOD);
    while (1) {
        isl29020_read(&_isl29020_dev);
        check_state();
        vtimer_sleep(delay);
    }
}

void check_state(int lux)
{
   
        switch (state) {
            case STATE_NORMAL:
                evt_handler_ok();
                break;
            case STATE_DOWN1:
                evt_handler_warn();
                break;
            case STATE_DOWN2:
                evt_handler_alarm();
                break;
        }
}


void sense_init(void)
{
    if (isl29020_init(&sixlowapp_isl29020_dev, ISL29020_I2C, ISL29020_ADDR,
                       ISL29020_RANGE_16K, ISL29020_MODE_AMBIENT)) {
        puts("! Initializing light sensor failed");
        return;
    }
    puts("Light sensor initialized.");

    // setup and start sense thread
    sensepid = thread_create(_stack, sizeof(_stack), THREAD_PRIO, CREATE_STACKTEST, sensethread, NULL, "sense");
    puts("Sense thread created.");
}
