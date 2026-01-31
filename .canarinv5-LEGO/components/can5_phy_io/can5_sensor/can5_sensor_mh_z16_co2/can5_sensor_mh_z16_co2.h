/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_MHZ16_H
#define CAN5_APP_CAN5_SENSOR_MHZ16_H

#include "can5_sensordriv.h"

typedef enum can5_sensor_mh_z16_co2_driverctl_t {
    CO2_NONE,
} can5_sensor_mh_z16_co2_driverctl_t;

typedef struct can5_sensor_mh_z16_co2_s {
    bool read;
    uint16_t val;         /* two bytes*/
} can5_sensor_mh_z16_co2_data_t;

extern const can5_sensordriv_t sensordriv_mh_z16_co2;

void parse_co2_read_data(const can5_sensordriv_t *driv, const can5_sensor_mh_z16_co2_data_t *co2_data, void *pout, size_t *plen);

#endif //CAN5_APP_CAN5_SENSOR_MHZ16_H
