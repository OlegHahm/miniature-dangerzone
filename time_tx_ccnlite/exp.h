/*
 * Copyright (C) 2016 Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief   Experiment definitions
 *
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 */
#ifndef EXP_H_
#define EXP_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EXP_ADDR
#define EXP_ADDR        { { 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                            0x00, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde } }
#endif

#ifndef EXP_ADDR_L2
#define EXP_ADDR_L2     { 0x02, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde }
#endif

#ifndef EXP_SRC_PORT
#define EXP_SRC_PORT    (1234U)
#endif

#ifndef EXP_DST_PORT
#define EXP_DST_PORT    (EXP_SRC_PORT)
#endif

#ifndef EXP_PREFIX
#define EXP_PREFIX      { { 0xab, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }
#endif

#ifndef EXP_PREFIX_LEN
#define EXP_PREFIX_LEN      (16U)
#endif

#ifndef EXP_PACKET_DELAY
#define EXP_PACKET_DELAY    (0U)
#endif

#ifndef NHC_ENABLED
#define NHC_ENABLED         (1U)
#endif

#ifndef EXP_FRAGMENT_DELAY
#define EXP_FRAGMENT_DELAY  (0U)
#endif

#define EXP_NAME_COMP   "ABC"

#ifndef EXP_MIN_PAYLOAD
#define EXP_MIN_PAYLOAD (8U)
#elif EXP_MIN_PAYLOAD < 5
#error "EXP_MAX_PAYLOAD needs to be at least 5"
#endif

#ifndef EXP_MAX_PAYLOAD
#define EXP_MAX_PAYLOAD (1232U)
#elif EXP_MAX_PAYLOAD < 5
#error "EXP_MAX_PAYLOAD needs to be at least 5"
#endif

#ifndef EXP_PAYLOAD_STEP
#define EXP_PAYLOAD_STEP (8U)
#elif EXP_PAYLOAD_STEP < 5
#error "EXP_MAX_PAYLOAD needs to be at least 5"
#endif

#ifndef EXP_RUNS
#define EXP_RUNS        (1000U)
#endif

#ifndef EXP_POWER_MEASURE_DELAY
#define EXP_POWER_MEASURE_DELAY (140 * 32)
#endif

#ifndef EXP_PAYLOAD_STEP_DELAY
#define EXP_PAYLOAD_STEP_DELAY  (64 * 4060)
#endif
void exp_run(void);

#ifdef __cplusplus
}
#endif

#endif /* EXP_H_ */
/** @} */
