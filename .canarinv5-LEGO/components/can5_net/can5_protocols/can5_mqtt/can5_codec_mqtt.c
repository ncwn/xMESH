/**************************************************
 * Author: rmukhia
 * Creation Date: 17/11/22
 * Description: 
 **************************************************/
#include <esp_log.h>
#include <malloc.h>
#include <cJSON.h>
#include "can5_codec_mqtt.h"
#include "can5_config.h"
#include "can5_utils.h"

static const char *TAG = "NETCODEC";

#if 0
#define TRACE_FUNC ESP_LOGI(TAG, "in -> %s() :%d", __FUNCTION__, __LINE__)
#else
#define TRACE_FUNC
#endif


/* ---------------------------------------------------------------------
 * Forward declarations
 -----------------------------------------------------------------------*/
static const struct {
    can5_sensor_data_type_t type;
    const char *str;
} sensor_type_to_str[] = {
        {CAN5_SENSOR_DATA_TYPE_TIMESTAMP,           "timestamp"},
        {CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LAT,   "gps_lat"},
        {CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_LNG,   "gps_lon"},
        {CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_GPS_ALT,   "gps_alt"},
        {CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_NORTH, "vel_north"},
        {CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_EAST,  "vel_east"},
        {CAN5_SENSOR_DATA_TYPE_UBLOX_NEO_IMU_DOWN,  "vel_down"},
        {CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_CF1,   "pm1.0"},
        {CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_CF1,   "pm2.5"},
        {CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_CF1,    "pm10"},
        {CAN5_SENSOR_DATA_TYPE_PMS7003_PM1_0_ATM,   "pm1.0_atm"},
        {CAN5_SENSOR_DATA_TYPE_PMS7003_PM2_5_ATM,   "pm2.5_atm"},
        {CAN5_SENSOR_DATA_TYPE_PMS7003_PM10_ATM,    "pm10_atm"},
        {CAN5_SENSOR_DATA_TYPE_MH_Z16_CO2,          "mhz16_co2"},
        {CAN5_SENSOR_DATA_TYPE_ZE03_NO2,            "no2"},
        {CAN5_SENSOR_DATA_TYPE_ZE07_CO,             "co"},
        {CAN5_SENSOR_DATA_TYPE_BME280_TEMP,         "temperature"},
        {CAN5_SENSOR_DATA_TYPE_BME280_PRES,         "pressure"},
        {CAN5_SENSOR_DATA_TYPE_BME280_HUMI,         "humidity"},
        {CAN5_SENSOR_DATA_TYPE_WS3226_RAIN,         "rain"},
        {CAN5_SENSOR_DATA_TYPE_WS3226_WIND_SPD,     "wind_spd"},
        {CAN5_SENSOR_DATA_TYPE_WS3226_WIND_DIR,     "wind_dir"},
        {CAN5_SENSOR_DATA_TYPE_WS3226_BATTERY_VOLT, "batt_v"},
        {CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LAT,     "gps_lat"},
        {CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_LNG,     "gps_lon"},
        {CAN5_SENSOR_DATA_TYPE_SIM7600_GPS_ALT,     "gps_alt"},
        {CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_X,     "imu_accel_x"},
        {CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Y,     "imu_accel_y"},
        {CAN5_SENSOR_DATA_TYPE_MPU6400_ACCEL_Z,     "imu_accel_z"},
        {CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_X,     "imu_gyro_x"},
        {CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Y,     "imu_gyro_y"},
        {CAN5_SENSOR_DATA_TYPE_MPU6400_GYRO_Z,     "imu_gyro_z"},
        {CAN5_SENSOR_DATA_TYPE_SCD41_CO2,         "mhz16_co2"},
        {CAN5_SENSOR_DATA_TYPE_SCD41_TEMP,         "temperature"},
        {CAN5_SENSOR_DATA_TYPE_SCD41_HUMI,         "humidity"},
};

typedef struct multi_data_key_s {
    can5_sensor_data_type_t type;
    int8_t index;
} multi_data_key_t;

#define MULTI_DATA_KEY_MAX 32
static multi_data_key_t multi_data_keys[MULTI_DATA_KEY_MAX] = {
        {CAN5_SENSOR_DATA_TYPE_WS3226_WIND_DIR, 0},
        {CAN5_SENSOR_DATA_TYPE_WS3226_WIND_SPD, 0},
};


static can5_err_t __make_mqtt_data(const can5_sensor_data_list_t *list,
                                   const can5_sensor_data_t *time_sensor_data,
                                   char **out_str);

static const char * __get_type_simple_str(can5_sensor_data_type_t type);
/* ---------------------------------------------------------------------
 * Function definition
 -----------------------------------------------------------------------*/

can5_err_t mqtt_make_tx(const can5_sensor_data_list_t *list, char **out_str, char *out_timestamp)
{
    TRACE_FUNC;
    can5_sensor_data_t *timestamp_data;

    VERIFY_NOT_NULL(list);

    timestamp_data = can5_sensor_data_get_timestamp(list);

    if (!timestamp_data) {
        return CAN5_CODEC_ERR_NO_TIMESTAMP;
    }

    strcpy(out_timestamp, timestamp_data->val);

    // reset index for multi data keys
    for (int i = 0; i < MULTI_DATA_KEY_MAX; i++) {
        multi_data_keys[i].index = 0;
    }

    return __make_mqtt_data( list,timestamp_data, out_str);
}

#define MQTT_DATA_MAX_KEY_LEN   32
static can5_err_t __make_mqtt_data(const can5_sensor_data_list_t *list,
                                   const can5_sensor_data_t *time_sensor_data,
                                   char **out_str)
{
    TRACE_FUNC;

    can5_err_t ret;
    can5_sensor_data_t *sensor_data;
    const char *sensor_port, *sensor_type;
    size_t topic_len;
    cJSON *root;

    ret = CAN5_SUCCESS;

    root = NULL;
    root = cJSON_CreateObject();
    if (!root) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    if (!cJSON_AddStringToObject(root, "timestamp", time_sensor_data->val)) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }

    TAILQ_FOREACH(sensor_data, list, te) {
        char key[MQTT_DATA_MAX_KEY_LEN + 1];
        int8_t index = -1;
        // skip timestamp
        if (sensor_data == time_sensor_data) {
            continue;
        }

        sensor_port = can5_sensor_data_get_port_simple_str(sensor_data->port);
        sensor_type = __get_type_simple_str(sensor_data->type);

        for (int i = 0; i< MULTI_DATA_KEY_MAX; i++) {
            if (multi_data_keys[i].type == sensor_data->type) {
                index = multi_data_keys[i].index++;
                break;
            }
        }

        if (index == -1) {
            topic_len = snprintf(NULL, 0, "%s-%s", sensor_type, sensor_port);
        }
        else {
            topic_len = snprintf(NULL, 0, "%s-%d-%s", sensor_type, index, sensor_port);
        }

        if (topic_len > MQTT_DATA_MAX_KEY_LEN) {
            if (index == -1) {
                ESP_LOGE(TAG, "Key too long \"%s-%s\". Max size %d", sensor_type, sensor_port, MQTT_DATA_MAX_KEY_LEN);
            }
            else {
                ESP_LOGE(TAG, "Key too long \"%s-%d-%s\". Max size %d", sensor_type, index, sensor_port, MQTT_DATA_MAX_KEY_LEN);
            }
            ret = CAN5_SENSOR_ERR_PARSE_ERROR;
            goto done;
        }

        if (index == -1) {
            snprintf(key, MQTT_DATA_MAX_KEY_LEN + 1, "%s-%s", sensor_type, sensor_port);
        }
        else {
            snprintf(key, MQTT_DATA_MAX_KEY_LEN + 1, "%s-%d-%s", sensor_type, index, sensor_port);
        }

        if (!cJSON_AddStringToObject(root, key, sensor_data->val)) {
            ESP_LOGE(TAG, "Out of heap memory to add %s", key);
            /* just continue */
        }
    }

    *out_str = cJSON_Print(root);
    if (!*out_str) {
        ret = CAN5_ERR_OUT_OF_HEAP_MEMORY;
        goto done;
    }


done:
    if (root) {
        cJSON_Delete(root);
    }
    return ret;
}

static const char * __get_type_simple_str(can5_sensor_data_type_t type) {
    const char *val;

    val = "unknown";

    for(size_t i = 0; i < sizeof(sensor_type_to_str) / sizeof(sensor_type_to_str[0]); i++) {
        if (type == sensor_type_to_str[i].type) {
            val = sensor_type_to_str[i].str;
            break;
        }
    }

    return val;
}
