/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_ZE07CO_H
#define CAN5_APP_CAN5_SENSOR_ZE07CO_H

#include "can5_sensordriv.h"

typedef enum can5_sensor_ze07_co_driverctl_t {
    CO_NONE,
} can5_sensor_ze07_co_driverctl_t;

typedef struct can5_sensor_ze07_co_s {
    bool read;
    double val;         /* two bytes*/
} can5_sensor_ze07_co_data_t;

extern const can5_sensordriv_t sensordriv_ze07_co;

void parse_co_read_data(const can5_sensordriv_t *driv, const can5_sensor_ze07_co_data_t *co_data, void *pout, size_t *plen);

#endif //CAN5_APP_CAN5_SENSOR_ZE07CO_H
