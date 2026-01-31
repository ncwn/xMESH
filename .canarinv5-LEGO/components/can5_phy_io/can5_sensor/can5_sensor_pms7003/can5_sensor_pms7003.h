/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_PMS7003_H
#define CAN5_APP_CAN5_SENSOR_PMS7003_H

#include "can5_sensordriv.h"

typedef enum can5_sensor_pms7003_driverctl_t {
    PM_NONE,
} can5_sensor_pms7003_driverctl_t;

typedef enum can5_sensor_pms7003_type_s {
    PMS7003_TYPE_1 = 0,
    PMS7003_TYPE_2_5,
    PMS7003_TYPE_10,

    PMS7003_TYPE_COUNT,
} can5_sensor_pms7003_type_t;

typedef struct can5_sensor_pms7003_data_s {
    bool read;
    struct {
        int cf1;
        int atm_env;
    } pm[PMS7003_TYPE_COUNT];
} can5_sensor_pms7003_data_t;

extern const can5_sensordriv_t sensordriv_pms7003;

void parse_pm_read_data(const can5_sensordriv_t *driv, const can5_sensor_pms7003_data_t *pm_data, void *pout, size_t *plen);

#endif //CAN5_APP_CAN5_SENSOR_PMS7003_H
