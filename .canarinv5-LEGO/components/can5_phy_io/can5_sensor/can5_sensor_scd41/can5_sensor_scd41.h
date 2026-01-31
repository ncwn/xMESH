/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_SCD41_H
#define CAN5_APP_CAN5_SENSOR_SCD41_H

#include "can5_sensordriv.h"

typedef enum can5_sensor_scd41_ctl_e {
    SENSOR_SCD41_FORCE_CALIBRATE,
} can5_sensor_scd41_ctl_t;

typedef struct can5_sensor_scd41_data_s {
    volatile bool read;
    uint16_t co2;
    float temperature;
    float humidity;
} can5_sensor_scd41_data_t;

extern const can5_sensordriv_t sensordriv_scd41;

void parse_scd41_read_data(const can5_sensordriv_t *driv, const can5_sensor_scd41_data_t *co_data, void *pout, size_t *plen);

#endif // CAN5_APP_CAN5_SENSOR_SCD41_H
