/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   12/01/22
*
* File:  can5_sensor_co.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_UBLOX_NEO_GPS_H
#define CAN5_APP_CAN5_SENSOR_UBLOX_NEO_GPS_H

#include "can5_sensordriv.h"

#define CAN5_GPS_FIX            (1 << 0)
#define CAN5_GPS_LAT            (1 << 1)
#define CAN5_GPS_LNG            (1 << 2)
#define CAN5_GPS_ALT            (1 << 3)
#define CAN5_GPS_N_SAT_ACTIVE   (1 << 4)
#define CAN5_GPS_N_SAT_VIEW     (1 << 5)
#define CAN5_GPS_TIME_SET       (1 << 6)


typedef enum can5_sensor_ublox_neo_gps_driverctl_e {
    CAN5_SENSOR_UBLOX_NEO_GPS_DRIVERCTL_PRINT,
    CAN5_SENSOR_UBLOX_NEO_GPS_DRIVERCTL_GET_NULL_HDL,
} can5_sensor_ublox_neo_gps_driverctl_t;

typedef struct can5_sensor_ublox_neo_data_s {
    bool read;
    uint8_t flags;
    bool fix;                           /* got satellite fix */
    double lat;     /* Decimal Lat, Lng */
    double lng;     /* Decimal Lat, Lng */
    double alt;     /* In meters */
    double vel_north;
    double vel_east;
    double vel_down;
    uint8_t n_sat;               /* active satellites */
} can5_sensor_ublox_neo_data_t;

extern const can5_sensordriv_t sensordriv_ublox_neo;


void parse_gps_read_data(const can5_sensordriv_t *driv, const can5_sensor_ublox_neo_data_t *gps_data, void *pout, size_t *plen);

#endif //CAN5_APP_CAN5_SENSOR_UBLOX_NEO_GPS_H
