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
 * @file        light.h
 * @brief       INRIA-Industry 2014 demo application - light node
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#ifndef __LIGHT_H
#define __LIGHT_H


void light_init(void);

void light_recv_cmd(int src, char id, char data, char sequ);

void light_set_shell(int argc, char **argv);

void light_off_shell(int argc, char **argv);

void light_ok_shell(int argc, char **);

void light_warn_shell(int argc, char **);

void light_alarm_shell(int argc, char **);

void light_set_rgb(uint8_t r, uint8_t g, uint8_t b);

void light_alarm(void);

void light_ok(void);

void light_warn(void);

void light_off(void);

void light_on_data(uint16_t src, char *data, int length);


#endif /* __LIGHT_H */
/** @} */