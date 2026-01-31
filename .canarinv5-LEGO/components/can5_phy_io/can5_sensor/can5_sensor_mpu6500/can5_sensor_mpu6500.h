/*******************************************************************************
* Author: Raunak Mukhia @rmukhia
* Date:   29/05/23
*
* File:  can5_sensor_mpu6500_imu.h
* Descr:
*******************************************************************************/

#ifndef CAN5_APP_CAN5_SENSOR_MPU6500_H
#define CAN5_APP_CAN5_SENSOR_MPU6500_H

#include "can5_sensordriv.h"

typedef enum can5_sensor_mpu6500_ctl_e {
    SENSOR_MPU6500_SLEEP_MODE,
    SENSOR_MPU6500_FORCED_MODE,
    SENSOR_MPU6500_NORMAL_MODE,
    SENSOR_MPU6500_RESET,
} can5_sensor_mpu6500_ctl_t;


typedef struct can5_sensor_mpu6500_data_s {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float imu_temp;
}can5_sensor_mpu6500_data_t;

extern const can5_sensordriv_t sensordriv_mpu6500;

void parse_mpu6500_read_data(const can5_sensordriv_t *driv, const can5_sensor_mpu6500_data_t *mpu6500_data,
                            void *pout, size_t *plen);

int64_t parse_mpu6500_to_csv(const can5_sensor_mpu6500_data_t *mpu_6500_data, int64_t timestamp_ms, char *pout);
#endif //CAN5_APP_CAN5_SENSOR_MPU6500_H