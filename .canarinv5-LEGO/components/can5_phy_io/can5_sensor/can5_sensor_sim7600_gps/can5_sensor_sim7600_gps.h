/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   26/09/22
*
* File:  can5_sensor_sim7600_gps.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_SIM7600_GPS_H
#define CAN5_APP_CAN5_SENSOR_SIM7600_GPS_H
#include "can5_sensordriv.h"

#define CAN5_GPS_FIX            (1 << 0)
#define CAN5_GPS_LAT            (1 << 1)
#define CAN5_GPS_LNG            (1 << 2)
#define CAN5_GPS_ALT            (1 << 3)

typedef struct can5_sensor_sim7600_gps_data_s {
    uint8_t flags;
    bool fix;                           /* got satellite fix */
    double lat;     /* Decimal Lat, Lng */
    double lng;     /* Decimal Lat, Lng */
    double alt;     /* In meters */
    uint8_t n_sat_active;               /* active satellites */
    uint8_t n_sat_view;                 /* Satellites in view */
} can5_sensor_sim7600_gps_data_t;

extern const can5_sensordriv_t sensordriv_sim7600_gps;

void parse_sim7600_gps_read_data(const can5_sensordriv_t *driv, const can5_sensor_sim7600_gps_data_t *gps_data, void *pout, size_t *plen);

#endif //CAN5_APP_CAN5_SENSOR_SIM7600_GPS_H
