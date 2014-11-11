/*
 * Copyright (C) 2014 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @file
 * @brief       6LoWPAN example application sensor functions
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 */

#include "board.h"

#if defined MODULE_LSM303DLHC && defined MODULE_LPS331AP \
    && defined MODULE_LPS331AP && defined MODULE_ISL29020

#include "isl29020.h"
#include "l3g4200d.h"
#include "lps331ap.h"
#include "lsm303dlhc.h"

#define ACC_S_RATE  LSM303DLHC_ACC_SAMPLE_RATE_10HZ
#define ACC_SCALE   LSM303DLHC_ACC_SCALE_2G
#define MAG_S_RATE  LSM303DLHC_MAG_SAMPLE_RATE_75HZ
#define MAG_GAIN    LSM303DLHC_MAG_GAIN_400_355_GAUSS

isl29020_t sixlowapp_isl29020_dev;
l3g4200d_t sixlowapp_l3g4200d_dev;
lps331ap_t sixlowapp_lps331ap_dev;
lsm303dlhc_t sixlowapp_lsm303_dev;

int sixlowapp_sensor_init(void)
{
    if (isl29020_init(&sixlowapp_isl29020_dev, ISL29020_I2C, ISL29020_ADDR,
                       ISL29020_RANGE_16K, ISL29020_MODE_AMBIENT)) {
        return -1;
    }
    if (l3g4200d_init(&sixlowapp_l3g4200d_dev, L3G4200D_I2C, L3G4200D_ADDR,
                       L3G4200D_INT, L3G4200D_DRDY, L3G4200D_MODE_100_25,
                       L3G4200D_SCALE_500DPS)) {
        return -1;
    }
    if (lps331ap_init(&sixlowapp_lps331ap_dev, LPS331AP_I2C, LPS331AP_ADDR,
                       LPS331AP_RATE_7HZ)) {
        return -1;
    }
    if (lsm303dlhc_init(&sixlowapp_lsm303_dev, LSM303DLHC_I2C, LSM303DLHC_INT1,
                         LSM303DLHC_DRDY, LSM303DLHC_ACC_ADDR, ACC_S_RATE,
                         ACC_SCALE, LSM303DLHC_MAG_ADDR, MAG_S_RATE, MAG_GAIN)) {
        return -1;
    }
    return 0;
}

#endif
