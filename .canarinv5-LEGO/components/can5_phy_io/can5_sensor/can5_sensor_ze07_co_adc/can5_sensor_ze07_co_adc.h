/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_CO_ADC_H
#define CAN5_APP_CAN5_SENSOR_CO_ADC_H

#include "can5_sensordriv.h"

typedef struct can5_ze07_co_adc_s {
    double val;
} can5_ze07_co_adc_t;

extern const can5_sensordriv_t sensordriv_ze07_co_adc;

void parse_co_adc_read_data(const can5_sensordriv_t *driv, const can5_ze07_co_adc_t *co_data, void *pout, size_t *plen);

#endif //CAN5_APP_CAN5_SENSOR_CO_ADC_H
