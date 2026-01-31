/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_ZE03NO2_H
#define CAN5_APP_CAN5_SENSOR_ZE03NO2_H

#include "can5_sensordriv.h"

typedef enum can5_sensor_ze03_no2_driverctl_t {
    NO2_NONE,
} can5_sensor_ze03_no2_driverctl_t;

typedef struct can5_sensor_ze03_no2_s {
    bool read;
    uint16_t val;         /* two bytes*/
} can5_sensor_ze03_no2_data_t;

extern const can5_sensordriv_t sensordriv_ze03_no2;

void parse_no2_read_data(const can5_sensordriv_t *driv, const can5_sensor_ze03_no2_data_t *no2_data, void *pout, size_t *plen);

#endif //CAN5_APP_CAN5_SENSOR_ZE03NO2_H
