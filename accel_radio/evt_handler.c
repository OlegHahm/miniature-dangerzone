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
 * @file        evt_handler.c
 * @brief       CeBIT 2014 demo application - sensor node
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include "vtimer.h"
#include "board.h"

#include "evt_handler.h"
#include "events.h"

static uint8_t sequ_no = 0;
static uint8_t evt_no = 0;

void send_event(evt_t event);


void evt_handler_ok(void)
{
    puts("EVENT: all good");
    LED_GREEN_ON;
    LED_RED_OFF;
    // send status ok to actuator nodes
    send_event(CONFIRM);
}

void evt_handler_warn(void)
{
    puts("EVENT: warning");
    LED_GREEN_ON;
    LED_RED_ON;
    // send status warning to actuator nodes
    send_event(WARN);
}

void evt_handler_alarm(void)
{
    puts("EVENT: alarm");
    LED_GREEN_OFF;
    LED_RED_ON;
    // send alarm event to actuator nodes
    send_event(ALARM);
}

void send_event(evt_t event)
{
    char cmd[3];
    if (event != CONFIRM) {         // if CONFIRM reuse old evt_no
        ++evt_no;
    }
    cmd[0] = (char)event;      // id
    cmd[1] = (char)evt_no;   // data
    cmd[2] = (char)sequ_no++;  // sequence number
    printf("send id:%i, data:%i, sequ:%i\n", cmd[0], cmd[1], cmd[2]);
}
