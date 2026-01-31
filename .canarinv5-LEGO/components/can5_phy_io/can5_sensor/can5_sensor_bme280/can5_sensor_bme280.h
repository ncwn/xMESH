/*******************************************************************************
* Author: Kalana Jayaratne @kalanaj
* Date:   31/01/22
*
* File:  can5_sensor_bme280.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_BME280_H
#define CAN5_APP_CAN5_SENSOR_BME280_H

#include "can5_sensordriv.h"

typedef enum can5_sensor_bme280_ctl_e {
    SENSOR_BME280_SLEEP_MODE,
    SENSOR_BME280_FORCED_MODE,
    SENSOR_BME280_NORMAL_MODE,
    SENSOR_BME280_RESET,
} can5_sensor_bme280_ctl_t;


typedef struct can5_sensor_bme280_data_s {
    float temp;
    float pres;
    float humi;
}can5_sensor_bme280_data_t;

extern const can5_sensordriv_t sensordriv_bme280;

void parse_bme280_read_data(const can5_sensordriv_t *driv, const can5_sensor_bme280_data_t *bme280_data,
                            void *pout, size_t *plen);
#endif //CAN5_APP_CAN5_SENSOR_BME280_H