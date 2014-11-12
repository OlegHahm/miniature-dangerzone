/*
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     inria_industry_demo
 * @{
 *
 * @file        light.c
 * @brief       INRIA-Industry 2014 demo application - light node
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "rgbled.h"
#include "periph/pwm.h"

#include "light.h"
#include "demo.h"
#include "events.h"


static rgbled_t led;


void light_init(void)
{
    rgbled_init(&led, PWM_0, LIGHT_CH_R, LIGHT_CH_G, LIGHT_CH_B);
}

void light_set_shell(int argc, char **argv)
{
    rgb_t col;
    if (argc >= 4) {
        col.r = (uint8_t)atoi(argv[1]);
        col.g = (uint8_t)atoi(argv[2]);
        col.b = (uint8_t)atoi(argv[3]);
        printf("setting color to r: %i, g: %i, b: %i\n", col.r, col.g, col.b);
        rgbled_set(&led, &col);
    }
}

void light_off_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    light_off();
}

void light_ok_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char data[3];
    data[0] = CONFIRM;
    data[1] = 12;
    data[2] = 33;
    light_on_data(99, data, 3);
}

void light_warn_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char data[3];
    data[0] = WARN;
    data[1] = 12;
    data[2] = 33;
    light_on_data(99, data, 3);
}

void light_alarm_shell(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char data[3];
    data[0] = ALARM;
    data[1] = 12;
    data[2] = 33;
    light_on_data(99, data, 3);
}

void light_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    rgb_t col = {r, g, b};
    rgbled_set(&led, &col);
}

void light_ok(void)
{
    rgb_t col = LIGHT_COLOR_OK;
    rgbled_set(&led, &col);
    printf("LIGHT: confirm\n");

}

void light_warn(void)
{
    rgb_t col = LIGHT_COLOR_WARN;
    rgbled_set(&led, &col);
    printf("LIGHT: warning\n");
}

void light_alarm(void)
{
    rgb_t col = LIGHT_COLOR_ALARM;
    rgbled_set(&led, &col);
    printf("LIGHT: alarm\n");
}

void light_off(void)
{
    rgb_t col = {0, 0, 0};
    rgbled_set(&led, &col);
}

void light_on_data(uint16_t src, char *data, int length)
{
    if (length == 3) {
        switch(data[0]) {
            case CONFIRM:
                light_ok();
                break;
            case WARN:
                light_warn();
                break;
            case ALARM:
                light_alarm();
                break;
        }
    }
}