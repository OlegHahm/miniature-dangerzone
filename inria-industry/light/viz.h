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
 * @file        viz.h
 * @brief       INRIA-Industry 2014 demo application - light node
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#ifndef __VIZ_H
#define __VIZ_H

#include <stdint.h>

void viz_udp_pkt(uint8_t src);

void viz_parent_select(uint8_t parent);
void viz_parent_deselect(uint8_t parent);
void viz_dio(uint8_t src);


#endif /* __VIZ_H */