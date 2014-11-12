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
#include <stdlib.h>

#include "thread.h"
#include "vtimer.h"
#include "kernel.h"
#include "board.h"

#include "sense.h"
#include "l3g4200d.h"
#include "evt_handler.h"


#define THREAD_PRIO         (10U)

#define AXIS_THRESHOLD      (230)
#define SAMPLING_PERIOD     (50000U)
#define REP_LIMIT           (1U)

#define STATE_NORMAL        (0U)
#define STATE_DOWN1         (2U)
#define STATE_DOWN2         (1U)

static char _stack[KERNEL_CONF_STACKSIZE_MAIN];
static int sensepid;


static int state = -1;
static int rep_count = 0;

void *sensethread(void *unused);
void check_state(l3g4200d_data_t *ad);

l3g4200d_t _l3g4200d_dev;

extern void _transceiver_send_handler(char *pkt);


void *sensethread(void *unused)
{
    (void) unused;

    timex_t delay = timex_set(0, SAMPLING_PERIOD);
    for (;;) {
        l3g4200d_data_t ad;
        l3g4200d_read(&_l3g4200d_dev, &ad);
        //printf("%" PRIi16 "-%" PRIi16 "-%" PRIi16 "\n", ad.acc_x, ad.acc_y, ad.acc_z);
        check_state(&ad);
        vtimer_sleep(delay);
    }
    return NULL;
}

void check_state(l3g4200d_data_t *ad)
{
    unsigned dir = 0;
    if ((ad->acc_x > AXIS_THRESHOLD) || (ad->acc_y > AXIS_THRESHOLD) || ad->acc_z > AXIS_THRESHOLD) {
        puts("Movement detected");
    }
    if (abs(ad->acc_x) > AXIS_THRESHOLD) {
        dir = 1;
    }
    else if (abs(ad->acc_y) > AXIS_THRESHOLD) {
        dir = 2;
    }
    else if (abs(ad->acc_z) > AXIS_THRESHOLD) {
        dir = 3;
    }
    if (state == dir) {
        if (rep_count < REP_LIMIT) {
            ++rep_count;
        }
        //printf("inc %i\n", dir);
    } else {
        state = dir;
        rep_count = 0;
        printf("new dir: %i\n", dir);
    }
    
    if (rep_count == REP_LIMIT) {
        rep_count = REP_LIMIT + 1;
        switch (state - 1) {
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
}


void sense_init(void)
{
    if (l3g4200d_init(&_l3g4200d_dev, L3G4200D_I2C, L3G4200D_ADDR,
                       L3G4200D_INT, L3G4200D_DRDY, L3G4200D_MODE_100_25,
                       L3G4200D_SCALE_500DPS)) {
        puts("! Failed to initialze L3G4300D.");
        return;
    }
    puts("L3G4300D initialized.");

    // setup and start sense thread
    sensepid = thread_create(_stack, sizeof(_stack), THREAD_PRIO, CREATE_STACKTEST, sensethread, NULL, "sense");
    puts("Sense thread created.");
}

