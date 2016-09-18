/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#include <stdio.h>

#include "tlsf-malloc.h"
#include "msg.h"
#include "xtimer.h"

#include "netdev.h"
#include "stack.h"
#include "exp.h"

#define MAIN_MSG_QUEUE_SIZE (8)

static msg_t main_msg_queue[MAIN_MSG_QUEUE_SIZE];

/* 10kB buffer for the heap should be enough for everyone */
#define TLSF_BUFFER     (12240 / sizeof(uint32_t))
static uint32_t _tlsf_heap[TLSF_BUFFER];

int main(void) {
    tlsf_create_with_pool(_tlsf_heap, sizeof(_tlsf_heap));

    printf("%s started\n", APPLICATION_NAME);
    xtimer_init();
    msg_init_queue(main_msg_queue, MAIN_MSG_QUEUE_SIZE);
    netdev_init();
    stack_init();
    exp_run();
    printf("%s stopped\n", APPLICATION_NAME);
    return 0;
}

/** @} */
