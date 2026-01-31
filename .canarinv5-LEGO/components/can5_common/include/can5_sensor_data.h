/**************************************************
 * Author: rmukhia
 * Creation Date: 21/7/22
 * Description: 
 **************************************************/

#ifndef __CAN5_SENSOR_DATA__
#define __CAN5_SENSOR_DATA__

#include <sys/queue.h>
#include "can5_error.h"
#include "can5_types.h"

#define SENSOR_DATA_TAG             "p_data"
#define IMU_DATA_TAG                "imu_data"

/* Add more sensor types here */
typedef enum can5_sensor_data_type_e {
    CAN5_SENSOR_DATA_TYPE_TIMESTAMP = 0,

    CAN5_SENSOR_DATA_TYPE_NUM_CYCLE_DATA,
    // uart sensors
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT,
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG,
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT,
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_NORTH,
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_EAST,
    CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_DOWN,

    CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_CF1,
    CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_CF1,
    CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_CF1,
    CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_ATM,
    CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_ATM,
    CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_ATM,

    CAN5_SENSOR_DATA_TYPE_MH_Z16_CO2,

    CAN5_SENSOR_DATA_TYPE_ZE03_NO2,
    // adc
    CAN5_SENSOR_DATA_TYPE_ZE07_CO,
    // I2C
    CAN5_SENSOR_DATA_TYPE_BME280_TEMP,
    CAN5_SENSOR_DATA_TYPE_BME280_PRES,
    CAN5_SENSOR_DATA_TYPE_BME280_HUMI,

    CAN5_SENSOR_DATA_TYPE_WS3226_RAIN,
    CAN5_SENSOR_DATA_TYPE_WS3226_WIND_SPD,
    CAN5_SENSOR_DATA_TYPE_WS3226_WIND_DIR,
    CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_VOLT,
    CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_PERCENTAGE,

    // 4g module
    CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LAT,
    CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LNG,
    CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_ALT,

    // IMU
    CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_X,
    CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Y,
    CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Z,
    CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_X,
    CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Y,
    CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Z,

    CAN5_SENSOR_DATA_TYPE_SCD41_CO2,
    CAN5_SENSOR_DATA_TYPE_SCD41_HUMI,
    CAN5_SENSOR_DATA_TYPE_SCD41_TEMP,

    CAN5_SENSOR_DATA_TYPE_COUNT,
} can5_sensor_data_type_t;

typedef enum can5_sensor_data_datatype_e {
    CAN5_SENSOR_DATA_DATATYPE_NUM,
    CAN5_SENSOR_DATA_DATATYPE_DEC,
    CAN5_SENSOR_DATA_DATATYPE_STR,

    CAN5_SENSOR_DATA_DATATYPE_COUNT,
} can5_sensor_data_datatype_t;

typedef struct can5_sensor_data_s {
    can5_sensor_data_type_t type;
    can5_sensor_data_datatype_t datatype;
    can5_port_idx_t port;
    char *val;
    TAILQ_ENTRY(can5_sensor_data_s) te;
} can5_sensor_data_t;

typedef TAILQ_HEAD(can5_sensor_data_list_s, can5_sensor_data_s) can5_sensor_data_list_t;


size_t can5_sensor_data_dumps(const can5_sensor_data_t *sensor_data, char *out_str);

size_t can5_sensor_data_list_dumps(const can5_sensor_data_list_t *sensor_data_list, char *out_str);

can5_sensor_data_t *can5_sensor_data_loads(const char *in_str);

can5_err_t can5_sensor_data_list_loads(const char *in_str, can5_sensor_data_list_t *out_list);

void can5_sensor_data_free(can5_sensor_data_t *sensor_data);

void can5_sensor_data_list_free(can5_sensor_data_list_t *sensor_data_list);

can5_sensor_data_t *can_5_sensor_data_create(can5_sensor_data_type_t type, can5_port_idx_t port,
                                             can5_sensor_data_datatype_t datatype,
                                             const char *str_val, int64_t num_val, double dec_val);

int64_t can5_sensor_data_get_num(const can5_sensor_data_t *data, bool *success, bool check_datatype);

double can5_sensor_data_get_dec(const can5_sensor_data_t *data, bool *success, bool check_datatype);

char *can5_sensor_data_get_str(const can5_sensor_data_t *data, bool check_datatype);

can5_sensor_data_t *can5_sensor_data_get_timestamp(const can5_sensor_data_list_t *list);

const char *can5_sensor_data_type_getstr(can5_sensor_data_type_t type);

const char *can5_sensor_data_datatype_getstr(can5_sensor_data_datatype_t type);

bool can5_sensor_data_is_gps_type(can5_sensor_data_type_t type);

// return common names to refer to the sensor and the ports.
const char * can5_sensor_data_get_port_simple_str(can5_port_idx_t port);
#endif // __CAN5_SENSOR_DATA__
